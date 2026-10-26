import copy
from datetime import datetime, timezone, timedelta
import json
from pathlib import Path
import sys
import threading
import unittest
import urllib.request
import urllib.error
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from model import vehicle_payload
from server import BoundedServer

NOW = datetime(2026, 10, 27, tzinfo=timezone.utc)
ROW = dict(observed_at=NOW-timedelta(seconds=120), battery_percent=85, range_km=328.16,
           range_kind='rated', speed_kmh=0, speed_valid=False, charging=False, vehicle_state='offline')


class ModelTests(unittest.TestCase):
    def test_age_and_metadata(self):
        a = vehicle_payload(ROW, NOW)
        b = vehicle_payload(ROW, NOW+timedelta(seconds=10))
        self.assertEqual(a['data_age_ms'], 120000)
        self.assertEqual(b['data_age_ms'], 130000)
        self.assertEqual(a['range_kind'], 'rated')
        self.assertFalse(a['speed_valid'])
        self.assertEqual(a['vehicle_state'], 'offline')
        self.assertNotIn('latitude', a)

    def test_invalid_values(self):
        for key, value in [('battery_percent', 101), ('range_km', float('nan')), ('vehicle_state', 'unknown')]:
            row = copy.copy(ROW); row[key] = value
            with self.assertRaises(ValueError): vehicle_payload(row, NOW)
        row = dict(ROW, observed_at=NOW+timedelta(seconds=31))
        with self.assertRaises(ValueError): vehicle_payload(row, NOW)

    def test_old_data_and_naive_utc(self):
        row = dict(ROW, observed_at=(NOW-timedelta(days=100)).replace(tzinfo=None))
        self.assertEqual(vehicle_payload(row, NOW)['data_age_ms'], 4294967295)


class HttpTests(unittest.TestCase):
    def setUp(self):
        class Store:
            def payload(self): return vehicle_payload(ROW, NOW)
        self.server = BoundedServer(('127.0.0.1', 0), Store(), 'test-only-token')
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.url = 'http://127.0.0.1:%d' % self.server.server_port

    def tearDown(self):
        self.server.shutdown(); self.server.server_close(); self.thread.join()

    def request(self, path='/v1/vehicle', token='test-only-token'):
        req = urllib.request.Request(self.url+path, headers={'Authorization': 'Bearer '+token})
        return urllib.request.urlopen(req, timeout=2)

    def test_auth_and_contract(self):
        with self.assertRaises(urllib.error.HTTPError) as caught: self.request(token='wrong')
        self.assertEqual(caught.exception.code, 401); caught.exception.close()
        with self.request() as response:
            data = response.read()
            self.assertEqual(len(data), int(response.headers['Content-Length']))
            self.assertLessEqual(len(data), 1536)
            self.assertEqual(response.headers['Cache-Control'], 'no-store')
            self.assertEqual(json.loads(data)['source'], 'teslamate')
        with self.assertRaises(urllib.error.HTTPError) as caught: self.request('/v1/vehicle?car_id=1')
        self.assertEqual(caught.exception.code, 404); caught.exception.close()

    def test_db_failure_and_rate_limit(self):
        self.server.store.payload = lambda: (_ for _ in ()).throw(RuntimeError('sensitive detail'))
        with self.assertRaises(urllib.error.HTTPError) as caught: self.request()
        self.assertEqual(caught.exception.code, 503)
        self.assertNotIn(b'sensitive', caught.exception.read()); caught.exception.close()
        self.server.requests = 60
        with self.assertRaises(urllib.error.HTTPError) as caught: self.request()
        self.assertEqual(caught.exception.code, 429); caught.exception.close()

if __name__ == '__main__': unittest.main()
