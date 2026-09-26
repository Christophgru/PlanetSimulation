#!/usr/bin/env python3
"""Exercise traced moving/paused frames through the real application."""
import argparse
import csv
import hashlib
from pathlib import Path
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--binary', type=Path, required=True)
p.add_argument('--output-dir', type=Path, required=True)
args = p.parse_args()
root = Path(__file__).resolve().parent.parent
out = args.output_dir.resolve()
out.mkdir(parents=True, exist_ok=True)


def capture(name, frames, step, full_resolution=False):
    image, trace = out / f'{name}.png', out / f'{name}.csv'
    command = [str(args.binary.resolve()), '--config', 'tests/scenarios/atmosphere/base.json',
               '--surface-capture', str(image), '--render-size', '320', '240',
               '--simulation-time', '20', '--benchmark-frames', str(frames),
               '--benchmark-step', str(step), '--performance-trace', str(trace)]
    if full_resolution:
        command.append("--atmosphere-full-resolution")
    result = subprocess.run(command, cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (out / f'{name}.log').write_text(result.stdout)
    assert result.returncode == 0, result.stdout
    assert 'Shader compilation error' not in result.stdout and 'Shader linking error' not in result.stdout
    rows = sorted(csv.DictReader(trace.open()), key=lambda r: int(r['frame']))
    assert [int(r['frame']) for r in rows] == list(range(frames)), rows
    assert any(r['gpu_valid'] == '1' for r in rows)
    for row in rows:
        assert float(row['frame_ms']) >= 0
        if row['gpu_valid'] == '1':
            assert float(row['gpu_ms']) >= 0
        else:
            assert row['gpu_ms'] == ''  # Missing samples must not look like zero GPU work.
    return hashlib.sha256(image.read_bytes()).hexdigest(), rows


single, _ = capture('single', 1, 0)
paused, paused_rows = capture('paused', 5, 0)
assert single == paused, 'Reusing a paused frame changed the image'
assert all(int(r['scene_reuses']) == 1 for r in paused_rows[1:]), paused_rows
assert float(paused_rows[0]['terrain_build_ms']) > 0
assert all(float(r['terrain_build_ms']) == 0 for r in paused_rows[1:])
_, moving = capture('moving', 5, 0.001)
assert all(int(r['scene_reuses']) == 0 for r in moving), moving
assert sum(int(r['shadow_reuses']) for r in moving) >= 6, moving
assert int(moving[0]['shadow_updates']) == 2, moving
print('Validated traced frames, paused image identity, and moving shadow reuse')

full, _ = capture('full_resolution', 1, 0, full_resolution=True)
replayed = out / 'full_resolution_replay.png'
result = subprocess.run([str(args.binary.resolve()), '--replay', str(out / 'full_resolution.png.json'),
                         '--surface-capture', str(replayed)], cwd=root, text=True,
                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
assert result.returncode == 0, result.stdout
assert hashlib.sha256(replayed.read_bytes()).hexdigest() == full, 'Replay lost atmosphere quality settings'
print('Validated exact replay of full-resolution atmosphere capture')
