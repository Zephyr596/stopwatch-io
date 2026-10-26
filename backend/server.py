"""Small authenticated read-only HTTPS adapter; bounded workers and DB timeouts."""
import hmac
import json
import logging
import os
from pathlib import Path
import socketserver
import ssl
import threading
import time
from http.server import BaseHTTPRequestHandler, HTTPServer

from model import vehicle_payload

logging.basicConfig(level=logging.INFO, format='%(asctime)s %(message)s')


class SnapshotStore:
    def __init__(self, password):
        self.password = password
        self.lock = threading.Lock()
        self.row = None
        self.loaded = 0

    def payload(self):
        import psycopg2
        from psycopg2.extras import RealDictCursor
        with self.lock:
            if self.row is None or time.monotonic() - self.loaded >= 3:
                # No Tesla tokens, writes, wake-up requests, or location columns.
                connection = psycopg2.connect(
                    host=os.getenv('DB_HOST', 'database'), dbname=os.getenv('DB_NAME', 'teslamate'),
                    user='stopwatch_reader', password=self.password, connect_timeout=3,
                    options='-c default_transaction_read_only=on -c statement_timeout=2000 -c timezone=UTC')
                try:
                    with connection.cursor(cursor_factory=RealDictCursor) as cursor:
                        cursor.execute('SELECT * FROM stopwatch_api.vehicle_snapshot')
                        row = cursor.fetchone()
                        vehicle_payload(row)  # Invalid rows do not replace a good cached row.
                        self.row = row
                        self.loaded = time.monotonic()
                finally:
                    connection.close()
            # Compute age on every response, including cache hits.
            return vehicle_payload(self.row)


class RequestHandler(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'
    server_version = 'StopWatchAPI'
    sys_version = ''

    def log_message(self, *args):
        pass  # Never log Authorization, queries, or vehicle state.

    def reply(self, status, payload):
        data = json.dumps(payload, separators=(',', ':'), allow_nan=False).encode()
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(data)))
        self.send_header('Cache-Control', 'no-store')
        self.send_header('Connection', 'close')
        self.send_header('X-Content-Type-Options', 'nosniff')
        self.end_headers()
        self.close_connection = True
        self.wfile.write(data)

    def do_GET(self):
        expected = 'Bearer ' + self.server.token
        if not hmac.compare_digest(self.headers.get('Authorization', '').encode(), expected.encode()):
            self.reply(401, {'error': 'unauthorized'})
            return
        if self.path not in ('/v1/vehicle', '/healthz'):
            self.reply(404, {'error': 'not_found'})
            return
        with self.server.rate_lock:
            now = time.monotonic()
            if now - self.server.window >= 60:
                self.server.window, self.server.requests = now, 0
            if self.server.requests >= 60:
                self.reply(429, {'error': 'rate_limit'})
                return
            self.server.requests += 1
        if self.path == '/healthz':
            self.reply(200, {'status': 'ok'})
            return
        try:
            self.reply(200, self.server.store.payload())
        except Exception as error:
            logging.warning('snapshot unavailable: %s', type(error).__name__)
            self.reply(503, {'error': 'data_unavailable'})


class BoundedServer(socketserver.ThreadingMixIn, HTTPServer):
    daemon_threads = True
    request_queue_size = 8

    def __init__(self, address, store, token, tls=None):
        self.store, self.token, self.tls = store, token, tls
        self.slots = threading.BoundedSemaphore(4)
        self.rate_lock = threading.Lock()
        self.window, self.requests = time.monotonic(), 0
        super().__init__(address, RequestHandler)

    def process_request(self, request, address):
        if not self.slots.acquire(blocking=False):
            request.close()
            return
        try:
            super().process_request(request, address)
        except Exception:
            self.slots.release()
            raise

    def process_request_thread(self, request, address):
        try:
            request.settimeout(5)
            # TLS handshakes are inside bounded workers, not in the accept loop.
            if self.tls:
                request = self.tls.wrap_socket(request, server_side=True)
            self.finish_request(request, address)
        except (OSError, ValueError):
            pass
        finally:
            self.shutdown_request(request)
            self.slots.release()


def main():
    token = Path('/run/secrets/api_token').read_text().strip()
    password = Path('/run/secrets/db_password').read_text().strip()
    if len(token) < 32 or not password:
        raise RuntimeError('missing credentials')
    tls = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    tls.minimum_version = ssl.TLSVersion.TLSv1_2
    tls.load_cert_chain('/run/tls/server.crt', '/run/tls/server.key')
    server = BoundedServer(('0.0.0.0', 8443), SnapshotStore(password), token, tls)
    logging.info('read-only vehicle API ready')
    server.serve_forever()


if __name__ == '__main__':
    main()
