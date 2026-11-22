#!/usr/bin/env python3
"""Pack the StopWatch pet frames and wooden percussion audio."""
import argparse
from io import BytesIO
import math
from pathlib import Path
import random
from statistics import median
import struct
import wave

from PIL import Image

workspace = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--source-dir", type=Path, default=workspace / ".local/pet/round-mascot")
work = parser.parse_args().source_dir
atlas = Image.open(work / "sprite-atlas.png").convert("RGBA")
assert atlas.size == (1254, 1254)
smoke_atlas = Image.open(work / "smoke-atlas.png").convert("RGBA")
assert smoke_atlas.size == (1536, 1024)
whisky_atlas = Image.open(work / "whisky-atlas.png").convert("RGBA")
assert whisky_atlas.size == (1536, 1024)
muyu_atlas = Image.open(work / "muyu-atlas.png").convert("RGBA")
assert muyu_atlas.size == (1536, 1024)
workout_atlas = Image.open(work / "workout-atlas.png").convert("RGBA")
width, height = 176, 196
workout_width, workout_height = 232, 146
scale = 0.427
baseline = 183
background = (7, 12, 16)

backdrop = Image.new("RGBA", (width, height))
for y in range(height):
    for x in range(width):
        distance = math.hypot((x-87)/84, (y-96)/93)
        mix = max(0, 1-distance) ** 1.5
        backdrop.putpixel((x, y), (round(7+17*mix), round(12+27*mix), round(16+61*mix), 255))

def sprite_from_cell(sheet, number, rows):
    col, row = number % 3, number // 3
    cell_w, cell_h = sheet.width // 3, sheet.height // rows
    sprite = sheet.crop((col*cell_w, row*cell_h, (col+1)*cell_w, (row+1)*cell_h))
    alpha = sprite.getchannel("A")
    extent = alpha.point(lambda value: 255 if value >= 32 else 0).getbbox()
    assert extent and min(extent[:2]) >= 2 and extent[2] <= cell_w-2 and extent[3] <= cell_h-2
    return sprite.crop(extent)


def art_from_cell(sheet, number, rows):
    sprite = sprite_from_cell(sheet, number, rows)
    pixels = sprite.load()
    spans = []
    for y in range(int(sprite.height * .39)):
        bright = [x for x in range(sprite.width) if pixels[x, y][3] > 180 and min(pixels[x, y][:3]) > 165]
        if bright:
            spans.append((bright[0], bright[-1]))
    longest = max(right-left for left, right in spans)
    anchor = median((left+right)/2 for left, right in spans if right-left > longest*.72)
    return sprite, anchor, longest

names = ("idle", "blink", "work", "work_blink", "wave", "wave_out", "jump", "land", "think")
smoke_names = ("puff_ready", "puff_lift", "puff_draw", "puff_exhale", "puff_drift", "puff_fade")
whisky_names = ("whisky_ready", "whisky_raise", "whisky_sip", "whisky_lower", "whisky_smile", "whisky_rest")
muyu_names = ("mallet_up", "mallet_hit", "mallet_bounce", "mallet_lower", "mallet_smile", "mallet_rest")
workouts = ("benchPress", "latPulldown", "gobletSquat")
workout_poses = {name: tuple(f"{name}_{index}" for index in range(3)) for name in workouts}
art = {name: art_from_cell(atlas, number, 3) for number, name in enumerate(names)}
art.update({name: art_from_cell(smoke_atlas, number, 2) for number, name in enumerate(smoke_names)})
art.update({name: art_from_cell(whisky_atlas, number, 2) for number, name in enumerate(whisky_names)})
art.update({name: art_from_cell(muyu_atlas, number, 2) for number, name in enumerate(muyu_names)})
smoke_x = scale * art["idle"][2] / art["puff_ready"][2]
smoke_y = scale * art["idle"][0].height / art["puff_ready"][0].height
whisky_x = scale * art["idle"][2] / art["whisky_ready"][2]
whisky_y = scale * art["idle"][0].height / art["whisky_ready"][0].height
muyu_x = scale * art["idle"][2] / art["mallet_hit"][2]
muyu_y = scale * art["idle"][0].height / art["mallet_hit"][0].height
sequence = {
    "idle": (("idle", 0, 780), ("blink", 0, 130), ("idle", 0, 670)),
    "working": (("work", 0, 420), ("work_blink", 0, 145), ("work", 0, 235)),
    "waving": (("wave", 0, 145), ("wave_out", 0, 145), ("wave", 0, 145), ("wave_out", 0, 145), ("wave", 0, 160)),
    "smoking": tuple((name, 0, hold) for name, hold in zip(smoke_names, (130, 155, 380, 260, 285, 210))),
    "complete": (("land", 0, 120), ("jump", -8, 120), ("jump", -15, 155), ("jump", -7, 115), ("land", 0, 290)),
    "interrupted": (("think", 0, 290), ("think", 2, 135), ("think", 0, 300)),
    "drinking": tuple((name, 0, hold) for name, hold in zip(whisky_names, (200, 220, 525, 235, 375, 220))),
    "rolling": (("wave", 0, 130), ("wave_out", 0, 130)) * 3,
    "knocking": tuple((name, 0, hold) for name, hold in zip(muyu_names[1:], (120, 140, 125, 130, 160))),
    "rapidKnocking": (("mallet_hit", 0, 78), ("mallet_bounce", 0, 102)),
}


def workout_cycle(poses):
    ready, middle, peak = poses
    return ((ready, 0, 220), (middle, 0, 160), (peak, 0, 340), (middle, 0, 155),
            (ready, 0, 210), (middle, 0, 155), (peak, 0, 340), (middle, 0, 155),
            (ready, 0, 340))


workout_backdrop = backdrop.resize((workout_width, workout_height), Image.Resampling.BICUBIC)
frames = {}
workout_keys = []
for row, name in enumerate(workouts):
    poses = workout_poses[name]
    sequence[name] = workout_cycle(poses)
    sprites = [sprite_from_cell(workout_atlas, row * 3 + column, 3) for column in range(3)]
    fit = min((workout_width - 12) / max(sprite.width for sprite in sprites),
              (workout_height - 8) / max(sprite.height for sprite in sprites))
    for pose, sprite in zip(poses, sprites):
        sprite = sprite.resize((round(sprite.width * fit), round(sprite.height * fit)), Image.Resampling.LANCZOS)
        cel = workout_backdrop.copy()
        cel.alpha_composite(sprite, ((workout_width - sprite.width) // 2, workout_height - 4 - sprite.height))
        key = (pose, 0)
        frames[key] = cel.convert("RGB")
        workout_keys.append(key)

for animation in sequence.values():
    for name, lift, _duration in animation:
        if (name, lift) in frames:
            continue
        sprite, anchor, _head_width = art[name]
        scale_x, scale_y = scale, scale
        if name in smoke_names:
            scale_x, scale_y = smoke_x, smoke_y
        elif name in whisky_names:
            scale_x, scale_y = whisky_x, whisky_y
        elif name in muyu_names:
            scale_x, scale_y = muyu_x, muyu_y
        sprite = sprite.resize((round(sprite.width*scale_x), round(sprite.height*scale_y)), Image.Resampling.LANCZOS)
        sx = round(width/2 - anchor*scale_x)
        sy = baseline + lift - sprite.height
        assert 0 <= sx and 0 <= sy and sx+sprite.width <= width and sy+sprite.height <= height, (name, sx, sy)
        matte = backdrop.copy()
        matte.alpha_composite(sprite, (sx, sy))
        frames[name, lift] = matte.convert("RGB")

packed = bytearray()
positions = {}
# Keep the existing palettes stable and preserve the warm colors of the props.
groups = (
    [key for key in frames if key[0] not in whisky_names and key[0] not in muyu_names and key not in workout_keys],
    [(name, 0) for name in whisky_names],
    [key for key in frames if key[0] in muyu_names],
    workout_keys,
)
for group in groups:
    cel_width, cel_height = frames[group[0]].size
    color_source = Image.new("RGB", (cel_width, cel_height * len(group)))
    for index, key in enumerate(group):
        color_source.paste(frames[key], (0, index * cel_height))
    palette = color_source.quantize(colors=128, method=Image.Quantize.MEDIANCUT)
    matte_index = Image.new("RGB", (1, 1), background).quantize(palette=palette, dither=Image.Dither.NONE).getpixel((0, 0))
    colors = palette.getpalette()
    colors[3*matte_index:3*matte_index+3] = background
    palette.putpalette(colors)

    for key in group:
        png = BytesIO()
        frames[key].quantize(palette=palette, dither=Image.Dither.NONE).save(png, "PNG", optimize=True)
        chunk = png.getvalue()
        assert len(chunk) < 65536
        checked = Image.open(BytesIO(chunk)).convert("RGB")
        assert checked.size == (cel_width, cel_height) and checked.getpixel((0, 0)) == background
        positions[key] = (len(packed), len(chunk))
        packed.extend(chunk)

header = [
    "// Frames of the custom round-head pet, packed as consecutive PNGs in assets/codex-pet.bin.",
    "#pragma once",
    "#include <cstdint>",
    "",
    "namespace sw::codexPet {",
    "struct Frame { uint32_t offset; uint16_t bytes; uint16_t holdMs; };",
    f"inline constexpr int width = {width};",
    f"inline constexpr int workoutWidth = {workout_width};",
    'extern const uint8_t data[] asm("_binary_assets_codex_pet_bin_start");',
    "",
]
for name, poses in sequence.items():
    header.append(f"inline constexpr Frame {name}[] = {{")
    for pose, lift, duration in poses:
        offset, size = positions[pose, lift]
        header.append(f"    {{{offset}, {size}, {duration}}},")
    header.extend(("};", ""))
header.extend(("} // namespace sw::codexPet", ""))

(workspace / "assets/codex-pet.bin").write_bytes(packed)
(workspace / "src/ui/CodexPetFrames.h").write_text("\n".join(header), encoding="ascii", newline="\n")

# A short wooden impact with rapidly decaying resonances and no clipping.
rate = 16000
noise = random.Random(13)
samples = []
for i in range(rate * 17 // 100):
    t = i / rate
    tone = sum(level * math.sin(math.tau * frequency * t) * math.exp(-t / decay)
               for level, frequency, decay in ((0.48, 690, 0.026), (0.22, 1230, 0.012), (0.11, 2270, 0.006)))
    click = 0.14 * noise.uniform(-1, 1) * math.exp(-t / 0.0019)
    samples.append(round(28500 * min(1, t * 2800) * (tone + click)))
with wave.open(str(workspace / "assets/codex-muyu.wav"), "wb") as sound:
    sound.setparams((1, 2, rate, 0, "NONE", "not compressed"))
    sound.writeframes(struct.pack(f"<{len(samples)}h", *samples))
print(f"Packed {len(frames)} aligned PNG cels in {len(packed)} bytes for {len(sequence)} animations.")
