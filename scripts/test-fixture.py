#!/usr/bin/env python3
"""Exercise the local fixture via HTTP and the exact firmware JSON decoder.
This is a host contract test, not an ESP32 transport/TLS integration test.
"""
from pathlib import Path
import socket
import subprocess
import tempfile
import time
import urllib.error
import urllib.request

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='stopwatch-fixture-') as directory:
    work = Path(directory)
    mode = work / 'mode'
    mode.write_text('live')
    decoder = work / 'decode'
    subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-Isrc',
                    '-I.pio/libdeps/m5stack-stopwatch/ArduinoJson/src',
                    'tests/decode_fixture.cpp', '-o', str(decoder)], cwd=root, check=True)
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        port = sock.getsockname()[1]
    server = subprocess.Popen(['python3', 'scripts/vehicle-fixture.py', '--port', str(port),
                               '--mode-file', str(mode)], cwd=root, stdout=subprocess.DEVNULL)
    url = f'http://127.0.0.1:{port}/v1/vehicle'
    try:
        for _ in range(30):
            try:
                with urllib.request.urlopen(url, timeout=1) as response:
                    response.read()
                break
            except OSError:
                time.sleep(.1)
        else:
            raise AssertionError('Fixture did not start')
        for value in ('live', 'charging', 'stale', 'invalid', 'oversized'):
            mode.write_text(value)
            with urllib.request.urlopen(url, timeout=1) as response:
                assert response.headers['Content-Type'] == 'application/json'
                body = response.read()
            result = subprocess.run([str(decoder)], input=body, capture_output=True)
            assert (result.returncode == 0) == (value in ('live', 'charging', 'stale')), value
            text = result.stdout.decode()
            if value == 'charging':
                assert 'speed=0 charging=1' in text
            if value == 'stale':
                assert 'age=60000' in text
            print('PASS fixture -> firmware decoder:', value)
        for value, expected in [('auth', 401), ('error', 503)]:
            mode.write_text(value)
            try:
                urllib.request.urlopen(url, timeout=1)
                raise AssertionError('Expected HTTP error')
            except urllib.error.HTTPError as error:
                assert error.code == expected
            print('PASS fixture HTTP status:', expected)
    finally:
        server.terminate()
        server.wait(timeout=5)
