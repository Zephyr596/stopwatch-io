#!/bin/sh
# Run on the server from this project's dedicated directory after copying sources.
set -eu
umask 077
: "${PUBLIC_IP:?Set the EC2 public IP used in the server certificate}"
: "${BIND_IP:?Set the primary private IP (avoid existing Tailscale listener)}"
mkdir -p private
if [ ! -f private/api_token ]; then openssl rand -hex 32 > private/api_token; fi
if [ ! -f private/db_password ]; then openssl rand -hex 32 > private/db_password; fi
if [ ! -f private/ca.crt ]; then
    openssl req -x509 -newkey rsa:2048 -nodes -days 3650 \
      -keyout private/ca.key -out private/ca.crt -subj '/CN=StopWatch Private API CA' \
      -addext 'basicConstraints=critical,CA:TRUE' -addext 'keyUsage=critical,keyCertSign,cRLSign' 2>/dev/null
fi
if [ ! -f private/server.crt ]; then
    openssl req -newkey rsa:2048 -nodes -keyout private/server.key -out private/server.csr \
      -subj "/CN=$PUBLIC_IP" 2>/dev/null
    printf 'subjectAltName=IP:%s,DNS:%s\nbasicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\n' "$PUBLIC_IP" "$PUBLIC_IP" > private/server.ext
    openssl x509 -req -in private/server.csr -CA private/ca.crt -CAkey private/ca.key \
      -CAcreateserial -days 365 -sha256 -extfile private/server.ext -out private/server.crt 2>/dev/null
fi
# Private directory is 0700; container-readable file modes do not expose it to other host users.
chmod 700 private
chmod 644 private/api_token private/db_password private/server.key private/server.crt
printf 'BIND_IP=%s\n' "$BIND_IP" > .env
# Password is sent on stdin, never interpolated into a process command line.
{
    printf "\\set reader_password '%s'\n" "$(cat private/db_password)"
    cat <<'SQL'
SELECT 'CREATE ROLE stopwatch_reader LOGIN NOSUPERUSER NOCREATEDB NOCREATEROLE NOINHERIT'
WHERE NOT EXISTS (SELECT 1 FROM pg_roles WHERE rolname='stopwatch_reader') \gexec
ALTER ROLE stopwatch_reader PASSWORD :'reader_password';
ALTER ROLE stopwatch_reader SET default_transaction_read_only = on;
ALTER ROLE stopwatch_reader SET statement_timeout = '2s';
ALTER ROLE stopwatch_reader CONNECTION LIMIT 4;
SQL
    cat schema.sql
    cat <<'SQL'
GRANT CONNECT ON DATABASE teslamate TO stopwatch_reader;
GRANT USAGE ON SCHEMA stopwatch_api TO stopwatch_reader;
GRANT SELECT ON stopwatch_api.vehicle_snapshot TO stopwatch_reader;
SQL
} | docker exec -i teslamate-database-1 sh -c 'exec psql -X -v ON_ERROR_STOP=1 -U "${POSTGRES_USER:-teslamate}" -d "${POSTGRES_DB:-teslamate}"' > private/provision.log
# Launch only this compose project; no TeslaMate services are restarted.
docker compose -p stopwatch-api up -d --build
