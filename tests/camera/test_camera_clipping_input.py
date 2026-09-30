#!/usr/bin/env python3
"""Keep live mountain-ground clipping consistent with deterministic captures."""
import argparse
import json
from pathlib import Path
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--binary', type=Path, required=True)
parser.add_argument('--output-dir', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
out = args.output_dir.resolve()
out.mkdir(parents=True, exist_ok=True)
scene = json.loads((root / 'tests/fixtures/development/configs/scenarios/solar_system.json').read_text())
# A flat mountaintop isolates clipping from terrain approximation. The eye is
# 2 m above ground, 82 m above the reference sphere. The old live path used
# an 8.2 m near plane and showed stars through the ground when looking down.
scene['planets'][0]['surface_noise'] = []
scene['planets'][0]['terrain_landscape'] = {'enabled': True, 'elevation_offset_m': 80}
for body in scene['planets']:
    for feature in ('water', 'atmosphere', 'foliage'):
        body[feature] = {'enabled': False}
scene['lighting']['shadows']['enabled'] = False
scene['lighting']['ambient_light'] = .5
scene['surface_camera'].update(latitude_deg=0, longitude_deg=0, altitude=.002,
    direction_ned=[1, 0, 1], fov=80, simulation_time_seconds=20)
replay = out / 'scene.json'
replay.write_text(json.dumps({'scenario': scene, 'surface_camera': scene['surface_camera']}))
binary = args.binary.resolve()
with (out / 'capture.log').open('w') as log:
    subprocess.run([str(binary), '--replay', str(replay), '--surface-capture',
        str(out / 'expected.png'), '--render-size', '1280', '720'], cwd=root,
        stdout=log, stderr=subprocess.STDOUT, check=True)
expected = subprocess.check_output(['convert', str(out / 'expected.png'), '-depth', '8', 'rgb:-'])
metadata = json.loads((out / 'expected.png.json').read_text())['render']
assert metadata['terrain_pixels'] > 800000, metadata

with (out / 'live.log').open('w') as log:
    process = subprocess.Popen([str(binary), '--replay', str(replay)], cwd=root,
        stdout=log, stderr=subprocess.STDOUT)
    try:
        deadline = time.monotonic() + 35
        window = None
        while time.monotonic() < deadline:
            assert process.poll() is None, 'Application exited before opening its window'
            found = subprocess.run(['xdotool', 'search', '--onlyvisible', '--pid', str(process.pid)],
                                   capture_output=True)
            if found.returncode == 0 and found.stdout.strip():
                window = found.stdout.splitlines()[0].decode()
                break
            time.sleep(.1)
        assert window, 'Window did not open'
        # Keep the initial window dimensions. Resizing after mouse capture
        # warps the cursor and can turn the walking camera during the test.
        previous, stable = None, 0
        while time.monotonic() < deadline:
            assert process.poll() is None, 'Application exited during rendering'
            png = subprocess.check_output(['import', '-window', window, 'png:-'])
            rgb = subprocess.check_output(['convert', 'png:-', '-depth', '8', 'rgb:-'], input=png)
            stable = stable + 1 if rgb == previous and max(rgb) > 0 else 0
            if stable >= 2:
                break
            previous = rgb
            time.sleep(.5)
        assert stable >= 2, 'Paused live frame did not settle'
        (out / 'live.png').write_bytes(png)
        assert len(rgb) == len(expected), 'Live and capture dimensions differ'
        error = sum(abs(a-b) for a, b in zip(rgb, expected)) / len(expected)
        assert error < .5, f'Live ground differs from capture: mean channel error {error:.3f}'
        print(f'Validated live elevated-ground clipping against capture (mean error {error:.3f})')
    finally:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
