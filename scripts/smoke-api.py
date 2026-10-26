#!/usr/bin/env python3
"""Device HTTPS integration test; requires tls-fixture.py and a fixture-configured firmware."""
import argparse
from pathlib import Path
import re
import time
import serial
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--port',required=True)
p.add_argument('--mode-file',required=True)
p.add_argument('--reconnect',action='store_true')
a=p.parse_args()
mode=Path(a.mode_file)
s=serial.Serial();s.port=a.port;s.baudrate=115200;s.timeout=.1;s.dtr=False;s.rts=False;s.open()
latencies=[]
def command(c):
    s.write(c.encode());time.sleep(.08)
def state():
    s.reset_input_buffer()
    start=time.monotonic();s.write(b's');last_request=start
    while time.monotonic()-start<2:
        if time.monotonic()-last_request>.4:
            s.write(b's');last_request=time.monotonic()
        line=s.readline().decode(errors='replace')
        assert 'Guru Meditation' not in line and 'stack canary' not in line, line
        if '[state]' in line and 'clock=' in line:
            latencies.append(time.monotonic()-start)
            return {k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',line)}
    raise AssertionError('UI serial dispatcher stalled')
def until(predicate,timeout=18):
    end=time.monotonic()+timeout
    while time.monotonic()<end:
        value=state()
        if predicate(value):return value
        time.sleep(.15)
    raise AssertionError(value)
def change(value,predicate):
    mode.write_text(value)
    command('b')  # Tesla focused action is refresh; rate limited in firmware.
    result=until(predicate)
    print('PASS',value,result,flush=True)
    return result
try:
    time.sleep(1)
    command('r');command('h')
    for _ in range(3):
        if state()['launch']==0:break
        command('a')
    command('b')
    baseline=until(lambda s:s['valid']==1 and s['source']==2 and s['api_error']==0)
    assert baseline['mock']==0
    change('charging',lambda s:s['charging']==1 and s['speed']==0 and s['live']==1)
    change('stale',lambda s:s['age']>=60000 and s['api_error']==0 and s['live']==0)
    good=change('live',lambda s:s['speed']==42 and s['live']==1)
    bad=change('invalid',lambda s:s['api_error']==5 and s['live']==0)
    assert bad['valid']==1 and bad['revision']==good['revision']
    change('error',lambda s:s['api_error']==4 and s['valid']==1 and s['live']==0)
    change('slow',lambda s:s['api_error']==3 and s['valid']==1 and s['live']==0)
    healthy=change('live',lambda s:s['api_error']==0 and s['live']==1)
    assert healthy['worker_stack']>=1024
    if a.reconnect:
        command('w')
        offline=until(lambda s:s['wifi']==0 and s['api_error']==2)
        assert offline['valid']==1 and offline['live']==0
        command('h');until(lambda s:s['app']==0)
        healthy=until(lambda s:s['wifi']==1 and s['api_error']==0 and s['live']==1,50)
        print('PASS Wi-Fi disconnect/reconnect',healthy,flush=True)
    command('h')
    print('PASS HTTPS fixture: parsing/cache/freshness/errors/recovery; max sampled serial response ms',round(max(latencies)*1000))
finally:
    mode.write_text('live');s.close()
