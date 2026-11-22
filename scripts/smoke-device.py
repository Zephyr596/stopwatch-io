#!/usr/bin/env python3
"""USB integration test. Does not assert physical touch/button/display behavior.
Requires pyserial (included in PlatformIO's Python environment).
"""
import argparse
import re
import time
import serial

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--port', required=True)
parser.add_argument('--cycles', type=int, default=100)
args = parser.parse_args()
port = serial.Serial()
port.port, port.baudrate, port.timeout = args.port, 115200, 0.01
port.dtr = port.rts = False
port.open()


def drain(seconds):
    end = time.monotonic() + seconds
    result = ''
    while time.monotonic() < end:
        result += port.read(8192).decode(errors='replace')
    if 'Guru Meditation' in result or 'panic' in result or '[error]' in result:
        raise AssertionError(result)
    return result


def command(value):
    port.write(value.encode())
    return drain(0.06)  # Allows the 33ms renderer to process the transition.


def state():
    port.reset_input_buffer()
    deadline = time.monotonic() + 2
    text = ''
    while time.monotonic() < deadline:
        port.write(b's')
        text += drain(0.25)
        lines = [line[line.index('[state]'):] for line in text.splitlines()
                 if '[state]' in line and 'clock=' in line]
        if lines:
            return {key: int(value) for key, value in re.findall(r'(\w+)=(\d+)', lines[-1])}
    raise AssertionError('No complete firmware diagnostics. ' + text)


def cycle():
    command('b')  # Tesla (launcher index 0)
    command('h')
    command('a'); command('b')  # Motorcycle
    command('h')
    command('a'); command('b')  # Settings, do not activate haptics
    command('h')
    command('a'); command('b')  # Codex
    command('b'); command('a')  # Return launcher selection to Tesla


try:
    print(drain(1), end='')
    command('m')
    command('h')
    for _ in range(4):
        if state()['launch'] == 0:
            break
        command('a')
    first = state()
    assert first['app'] == 0 and first['launch'] == 0
    drain(1.2)
    assert state()['paints'] == first['paints'], 'Idle launcher repainted'
    command('b')
    first = state()
    assert first['app'] == 1 and first['valid'] == 1
    drain(1.2)
    second = state()
    assert second['revision'] > first['revision']
    if second['charging']:
        command('b')
    command('b')
    charged = state()
    assert charged['charging'] == 1 and charged['speed'] == 0
    command('b')
    assert state()['charging'] == 0
    command('a'); command('b')  # Tesla Home control, through application dispatch
    assert state()['app'] == 0
    command('a'); command('b')
    assert state()['app'] == 2
    command('b')
    command('a'); command('b')
    assert state()['app'] == 3
    command('a'); command('b')
    assert state()['app'] == 0
    command('a'); command('b')
    assert state()['app'] == 4
    command('1')
    assert state()['codex'] == 1
    command('2')
    assert state()['codex'] == 2
    before_puff = state()
    command('p')  # Inject a touch on the Smoke button.
    drain(0.6)
    puff = state()
    assert puff['smokes'] == before_puff['smokes'] + 1 and puff['waves'] == before_puff['waves']
    assert puff['paints'] >= before_puff['paints'] + 2
    command('k')  # Inject a touch on the Whisky button.
    drain(0.95)
    sip = state()
    assert sip['drinks'] == puff['drinks'] + 1 and sip['smokes'] == puff['smokes'] and sip['waves'] == puff['waves']
    assert sip['paints'] >= puff['paints'] + 4
    rolls = []
    for _ in range(12):
        command('g')
        rolls.append(state()['dice'])
    assert all(1 <= value <= 6 for value in rolls) and len(set(rolls)) > 1, rolls
    drain(0.85)
    rolled = state()
    assert rolled['rolls'] == sip['rolls'] + 12 and rolled['dice'] == rolls[-1]
    for _ in range(2):
        audio = command('j') + drain(0.18)
        assert '[muyu] audio=1' in audio, audio
    knocks = state()
    assert knocks['knocks'] == rolled['knocks'] + 2 and knocks['rolls'] == rolled['rolls']
    print('Dice results:', rolls, '| Wooden percussion: two playbacks accepted', flush=True)
    waves = state()['waves']
    command('a'); command('b')  # Wave at the pet.
    assert state()['waves'] == waves + 1
    command('h')
    assert state()['app'] == 0
    command('a')
    cycle()  # Warm all rendering paths before memory comparison.
    before = state()
    for index in range(args.cycles):
        cycle()
        if (index + 1) % 25 == 0:
            print(f'Switch cycles: {index + 1}', flush=True)
    after = state()
    assert after['app'] == 0 and after['launch'] == 0
    assert after['heap'] >= before['heap'] - 1024, (before, after)
    assert after['min'] >= before['min'] - 4096, (before, after)
    print('Heap before:', before)
    print('Heap after: ', after)
    print('PASS: app switching, mock charging, all Codex pet actions, dice range, audio playback, heap stability')
finally:
    port.close()
