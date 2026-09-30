#!/usr/bin/env python3
"""Fixed-resolution, frozen-orbit camera benchmark. Run under a GL display/Xvfb."""
import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import statistics
import subprocess
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--binary', type=Path, required=True)
p.add_argument('--output-dir', type=Path, required=True)
p.add_argument('--replay', type=Path, default=ROOT / 'docs/captures/replay/foliage/grass-detail.png.json')
p.add_argument('--config', type=Path, default=ROOT / 'configs/scenarios/solar_system.json')
p.add_argument('--frames', type=int, default=36)
p.add_argument('--warmup', type=int, default=5)
p.add_argument('--walk-step', type=float, default=2.0, help='metres per frame')
p.add_argument('--size', type=int, nargs=2, default=[640, 360])
p.add_argument('--cases', nargs='+', choices=['stationary', 'walking', 'walking-bare'],
               default=['stationary', 'walking', 'walking-bare'])
args = p.parse_args()
if args.warmup < 0 or args.frames < args.warmup + 3:
    p.error('Need at least two measured frames plus the final readback frame')
if not math.isfinite(args.walk_step) or not 0 < args.walk_step <= 100:
    p.error('--walk-step must be finite and in (0, 100] metres')
out = args.output_dir.resolve()
out.mkdir(parents=True, exist_ok=True)
binary = args.binary.resolve()
replay = json.loads(args.replay.read_text())
# Reuse the documented camera with the current working scene/foliage settings.
replay['scenario'] = json.loads(args.config.read_text())

def summarize(rows):
    # The final frame is completed by glFinish outside the frame timer.
    rows = [r for r in rows if args.warmup <= int(r['frame']) < args.frames - 1]
    result = {'measured_frames': len(rows)}
    for key in rows[0]:
        if key.endswith('_ms'):
            values = sorted(float(r[key]) for r in rows if r[key] != '')
            if values:
                result[key] = {'mean': statistics.mean(values), 'median': statistics.median(values),
                               'p95': values[math.ceil(.95 * len(values)) - 1]}
    for key in ['scene_reuses', 'mesh_uploads', 'foliage_rebuilds', 'shadow_updates', 'shadow_reuses']:
        result[key] = sum(int(r[key]) for r in rows)
    rebuilt = [float(r['cpu_foliage_ms']) for r in rows if int(r['foliage_rebuilds'])]
    result['foliage_rebuild_frame_mean_ms'] = statistics.mean(rebuilt) if rebuilt else 0
    result['gpu_measured_frames'] = sum(int(r['gpu_valid']) for r in rows)
    return result

report = {'utc': datetime.now(timezone.utc).isoformat(), 'platform': platform.platform(),
          'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
          'frames': args.frames, 'warmup': args.warmup, 'size': args.size,
          'environment': {key: os.environ.get(key) for key in
                          ['LIBGL_ALWAYS_SOFTWARE', 'LP_NUM_THREADS']},
          'terrain_mode': 'synchronous capture; interactive walking builds terrain asynchronously',
          'walk_step_m': args.walk_step, 'orbit_step_s': 0, 'cases': {}}
try:
    report['graphics'] = subprocess.check_output(['glxinfo', '-B'], text=True)
except (OSError, subprocess.CalledProcessError):
    report['graphics'] = 'glxinfo unavailable'
for case in args.cases:
    case_dir = out / case
    case_dir.mkdir(exist_ok=True)
    scene = json.loads(json.dumps(replay))
    if case == 'walking-bare':
        for planet in scene['scenario']['planets']:
            planet.setdefault('foliage', {})['enabled'] = False
    source = case_dir / 'input.json'
    source.write_text(json.dumps(scene, indent=2) + '\n')
    image, trace = case_dir / 'capture.png', case_dir / 'frames.csv'
    command = [str(binary), '--replay', str(source), '--surface-capture', str(image),
               '--render-size', *map(str, args.size), '--benchmark-frames', str(args.frames),
               '--benchmark-step', '0', '--benchmark-walk-step',
               str(0 if case == 'stationary' else args.walk_step), '--performance-trace', str(trace)]
    with (case_dir / 'capture.log').open('w') as log:
        subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
    with trace.open() as stream:
        rows = sorted(csv.DictReader(stream), key=lambda row: int(row['frame']))
    assert len(rows) == args.frames
    summary = summarize(rows)
    summary['command'] = command
    summary['input_sha256'] = hashlib.sha256(source.read_bytes()).hexdigest()
    report['cases'][case] = summary
    (out / 'summary.json').write_text(json.dumps(report, indent=2) + '\n')
    print(case, json.dumps({key: summary[key] for key in
          ['frame_ms', 'cpu_mesh_ms', 'cpu_foliage_ms', 'foliage_rebuilds', 'scene_reuses']}), flush=True)
