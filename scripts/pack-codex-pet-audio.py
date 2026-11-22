#!/usr/bin/env python3
"""Generate the pet action WAVs from an offline 16 kHz mono reading.

Hear the same verse at the hundredth knock using a Mandarin reading of
玄奘《般若波罗蜜多心经》, CBETA T0251, p. 848c:
色不异空，空不异色。色即是空，空即是色。受、想、行、识，亦复如是。
"""
import argparse
import math
from pathlib import Path
import random
import struct
import wave

RATE = 16000
ASSETS = Path(__file__).resolve().parents[1] / "assets"


def mix_at(samples, start, sound, gain=1.0):
    for index, value in enumerate(sound, round(start * RATE)):
        samples[index] += value * gain


def impact(duration, resonances, noise, click):
    for index in range(round(duration * RATE)):
        t = index / RATE
        ringing = sum(level * math.sin(math.tau * hz * t) * math.exp(-t / decay)
                      for hz, level, decay in resonances)
        transient = click * noise.uniform(-1, 1) * math.exp(-t / 0.002)
        yield min(1.0, t * 2500, (duration - t) * 600) * (ringing + transient)


def air(duration, noise, low_cut, high_cut, modulation):
    upper = lower = 0.0
    upper_speed = 1 - math.exp(-math.tau * high_cut / RATE)
    lower_speed = 1 - math.exp(-math.tau * low_cut / RATE)
    for index in range(round(duration * RATE)):
        t = index / RATE
        upper += upper_speed * (noise.uniform(-1, 1) - upper)
        lower += lower_speed * (upper - lower)
        envelope = math.sin(math.pi * t / duration) ** 2
        flutter = 0.72 + 0.28 * math.sin(math.tau * modulation * t)
        yield (upper - lower) * envelope * flutter


def smoke():
    noise = random.Random(19)
    samples = [0.0] * round(1.34 * RATE)
    lighter = ((1850, 0.35, 0.010), (2820, 0.26, 0.005))
    mix_at(samples, 0, impact(0.055, lighter, noise, 0.9))
    mix_at(samples, 0.077, impact(0.055, lighter, noise, 0.7), 0.56)
    mix_at(samples, 0.13, air(0.14, noise, 870, 3500, 25), 0.28)
    mix_at(samples, 0.65, air(0.62, noise, 280, 2600, 5), 0.8)
    return samples


def whisky():
    noise = random.Random(29)
    samples = [0.0] * round(1.31 * RATE)
    clink = ((1570, 0.46, 0.040), (2420, 0.37, 0.057), (3090, 0.22, 0.030))
    mix_at(samples, 0.01, impact(0.19, clink, noise, 0.28), 0.8)
    mix_at(samples, 0.092, impact(0.17, clink, noise, 0.22), 0.63)
    mix_at(samples, 0.448, air(0.42, noise, 250, 1750, 19), 0.68)
    mix_at(samples, 1.01, impact(0.24, clink, noise, 0.25))
    return samples


def dice():
    noise = random.Random(53)
    samples = [0.0] * round(0.78 * RATE)
    for start, pitch, gain in (
        (0.0, 790, 0.9), (0.077, 550, 0.68), (0.147, 680, 0.86),
        (0.231, 740, 0.8), (0.331, 590, 0.75), (0.445, 820, 0.55),
        (0.57, 650, 0.61), (0.715, 550, 1.0),
    ):
        bounce = ((pitch, 0.55, 0.012), (pitch * 2.40, 0.33, 0.007), (2820, 0.2, 0.004))
        mix_at(samples, start, impact(0.06, bounce, noise, 0.82), gain)
    return samples


def workout():
    noise = random.Random(43)
    samples = [0.0] * round(1.55 * RATE)
    plates = ((610, 0.40, 0.035), (1290, 0.34, 0.044), (2640, 0.27, 0.052))
    mix_at(samples, 0.01, impact(0.16, plates, noise, 0.22), 0.6)
    mix_at(samples, 0.38, impact(0.22, plates, noise, 0.38))
    mix_at(samples, 1.24, impact(0.23, plates, noise, 0.34), 0.86)
    return samples


def read_pcm(path):
    with wave.open(str(path), "rb") as wav:
        if (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) != (1, 2, RATE):
            raise ValueError(f"{path} must be 16 kHz mono PCM16")
        return wav.readframes(wav.getnframes())


def pcm(samples, peak_fraction):
    scale = peak_fraction * 32767 / max(map(abs, samples))
    return struct.pack(f"<{len(samples)}h", *(round(value * scale) for value in samples))


def write_wav(name, data):
    with wave.open(str(ASSETS / f"codex-{name}.wav"), "wb") as wav:
        wav.setparams((1, 2, RATE, 0, "NONE", "not compressed"))
        wav.writeframes(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reading", type=Path, help="Path to the offline Mandarin reading")
    args = parser.parse_args()
    reading = tuple(sample for sample, in struct.iter_unpack("<h", read_pcm(args.reading)))
    audible = [index for index, sample in enumerate(reading) if abs(sample) >= 128]
    if not audible:
        parser.error("The Mandarin reading contains no audible samples")

    # Keep the wood strike intact, then leave a short pause before the verse.
    margin = round(0.08 * RATE)
    verse = reading[max(0, audible[0] - margin):audible[-1] + margin + 1]
    pause = bytes(round(0.10 * RATE) * 2)
    write_wav("muyu-100", read_pcm(ASSETS / "codex-muyu.wav") + pause + pcm(verse, 0.88))
    write_wav("smoke", pcm(smoke(), 0.76))
    write_wav("whisky", pcm(whisky(), 0.76))
    write_wav("dice", pcm(dice(), 0.78))
    write_wav("workout", pcm(workout(), 0.76))


if __name__ == "__main__":
    main()
