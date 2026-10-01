#!/usr/bin/env python3
"""Check grass in the full HDR/atmosphere renderer and exact paused replay."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import re
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--binary', type=Path, required=True)
p.add_argument('--output-dir', type=Path, required=True)
args = p.parse_args()
root = Path(__file__).resolve().parents[2]
out = args.output_dir.resolve()
out.mkdir(parents=True, exist_ok=True)
scene = json.loads((root / 'tests/scenarios/foliage/surface.json').read_text())

def capture(name, frames=1, replay=None, walk=0):
    config = out / (name + '.json')
    config.write_text(json.dumps(scene))
    image, trace = out / (name + '.png'), out / (name + '.csv')
    command = [str(args.binary.resolve()), '--config', str(config),
               '--surface-capture', str(image), '--render-size', '640', '360',
               '--benchmark-frames', str(frames), '--benchmark-step', '0',
               '--benchmark-walk-step', str(walk),
               '--performance-trace', str(trace)]
    if replay:
        command += ['--replay', str(replay)]
    result = subprocess.run(command, cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (out / (name + '.log')).write_text(result.stdout)
    assert result.returncode == 0, result.stdout
    assert 'Shader compilation error' not in result.stdout and 'Shader linking error' not in result.stdout
    metadata = json.loads(Path(str(image) + '.json').read_text())['render']
    assert metadata['terrain_pixels'] > 10000, metadata
    triangles = re.findall(r'Planet \d+ terrain: (\d+) triangles', result.stdout)
    return hashlib.sha256(image.read_bytes()).hexdigest(), metadata, triangles, list(csv.DictReader(trace.open()))

on, metadata, triangles, _ = capture('grass')
assert 1000 < metadata['foliage_blades'] <= scene['planets'][0]['foliage']['max_blades'], metadata
paused, _, _, rows = capture('paused', 4)
assert paused == on, 'Paused grass moved or frame reuse changed its image'
assert all(int(r['scene_reuses']) == 1 for r in rows[1:]), rows
assert int(rows[0]['foliage_rebuilds']) == 2  # Detailed and GPU horizon patches.
assert float(rows[0]['foliage_placement_ms']) > 0
assert float(rows[0]['foliage_sort_ms']) > 0
assert float(rows[0]['foliage_upload_ms']) > 0
assert all(float(r['cpu_foliage_ms']) == 0 for r in rows[1:])
walked, _, _, walking = capture('walking', 6, walk=4)
assert walked != on, 'Walking benchmark did not move the view'
assert all(r['simulation_s'] == walking[0]['simulation_s'] for r in walking)
assert all(int(r['scene_reuses']) == 0 for r in walking)
assert sum(int(r['foliage_rebuilds']) for r in walking) >= 2, walking
start = json.loads((out / 'grass.png.json').read_text())['surface_camera']
end = json.loads((out / 'walking.png.json').read_text())['surface_camera']
assert (start['latitude_deg'], start['longitude_deg']) != (end['latitude_deg'], end['longitude_deg'])
scene['planets'][0]['foliage']['near_enabled'] = False
_, far_metadata, far_triangles, far_rows = capture('far-only')
assert far_metadata['foliage_blades'] == 0 and far_metadata['horizon_foliage_candidates'] > 0
assert int(far_rows[0]['foliage_rebuilds']) == 1 and far_triangles == triangles
scene['planets'][0]['foliage']['near_enabled'] = True
scene['planets'][0]['foliage']['enabled'] = False
off, metadata, bare_triangles, _ = capture('bare')
assert on != off, 'Grass did not change the rendered view'
assert metadata['foliage_blades'] == 0, metadata
assert triangles == bare_triangles, 'Foliage changed terrain tessellation'
replayed, metadata, _, _ = capture('replayed', replay=out / 'grass.png.json')
assert replayed == on and metadata['foliage_blades'] > 1000, 'Replay lost the foliage settings'
print('Validated visible grass, bounded instances, unchanged terrain, paused reuse and exact replay')
