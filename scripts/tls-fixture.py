#!/usr/bin/env python3
"""LAN HTTPS fixture using the production HTTP handler and synthetic data only."""
import argparse
from datetime import datetime, timezone
from pathlib import Path
import ssl
import sys
import time
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'backend'))
from server import BoundedServer

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--host', default='127.0.0.1')
p.add_argument('--cert', required=True)
p.add_argument('--key', required=True)
p.add_argument('--token-file', required=True)
p.add_argument('--mode-file', required=True)
a = p.parse_args()

class Store:
    def payload(self):
        mode = Path(a.mode_file).read_text().strip()
        if mode == 'error': raise ValueError('fixture error')
        if mode == 'slow': time.sleep(5)
        return dict(schema_version=1, source='fixture', battery_percent='invalid' if mode == 'invalid' else 78,
                    range_km=351, speed_kmh=0 if mode == 'charging' else 42, charging=mode == 'charging',
                    observed_at=int(datetime.now(timezone.utc).timestamp()),
                    data_age_ms=60000 if mode == 'stale' else 0)

ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
ctx.minimum_version = ssl.TLSVersion.TLSv1_2
ctx.load_cert_chain(a.cert, a.key)
server = BoundedServer((a.host, 8765), Store(), Path(a.token_file).read_text().strip(), ctx)
print('Synthetic HTTPS fixture ready', flush=True)
server.serve_forever()
