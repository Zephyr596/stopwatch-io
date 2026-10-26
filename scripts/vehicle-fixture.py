#!/usr/bin/env python3
"""Local-only synthetic vehicle endpoint. No Tesla credentials or vehicle access.
Bind to a LAN IP explicitly for device tests; do not deploy as the production API.
Change --mode-file contents locally: live, charging, stale, invalid, oversized,
error, slow, truncated, chunked, redirect, auth, real_over_http.
"""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--host', default='127.0.0.1')
parser.add_argument('--port', type=int, default=8765)
parser.add_argument('--mode-file', required=True)
args = parser.parse_args()


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path != '/v1/vehicle':
            self.send_error(404)
            return
        mode = Path(args.mode_file).read_text().strip()
        if mode == 'slow':
            time.sleep(5)
        if mode in ('error', 'auth'):
            self.send_error(503 if mode == 'error' else 401)
            return
        if mode == 'redirect':
            self.send_response(302)
            self.send_header('Location', '/v1/vehicle')
            self.end_headers()
            return
        data = dict(schema_version=1, source='fixture', battery_percent=78.0,
                    range_km=351.0, speed_kmh=0 if mode == 'charging' else 42.0,
                    charging=mode == 'charging', observed_at=int(time.time()),
                    data_age_ms=60000 if mode == 'stale' else 0)
        if mode == 'invalid':
            data['battery_percent'] = 'not a number'
        if mode == 'real_over_http':
            data['source'] = 'teslamate'
        body = json.dumps(data).encode()
        if mode == 'oversized':
            body = b'x' * 2000
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Cache-Control', 'no-store')
        if mode == 'chunked':
            self.send_header('Transfer-Encoding', 'chunked')
        else:
            self.send_header('Content-Length', str(len(body) + (30 if mode == 'truncated' else 0)))
        self.end_headers()
        try:
            self.wfile.write(body if mode != 'chunked' else b'%x\r\n' % len(body) + body + b'\r\n0\r\n\r\n')
        except (BrokenPipeError, ConnectionResetError):
            pass

    def log_message(self, fmt, *values):
        print('[fixture]', fmt % values, flush=True)


print(f'Fixture listening on {args.host}:{args.port}', flush=True)
ThreadingHTTPServer((args.host, args.port), Handler).serve_forever()
