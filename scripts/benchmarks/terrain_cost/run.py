#!/usr/bin/env python3
"""Three alternating production native CPU/resident stationary preflight pairs.

Run under NVIDIA PRIME GLX and Xvfb. This is the workload prerequisite for
T3c5c; it does not complete movement, coverage, reload or total migration gates.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import time

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tests/app/terrain/native'))
from NativeSession import Session, validate, write_json
from receipts import compare_pair, summarize


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inputs():
    groups = {}
    for label, names in [('source', ['src', 'shaders']), ('build_test', ['tests', 'scripts', 'configs', 'CMakeLists.txt'])]:
        files = []
        for name in names:
            path = ROOT / name
            files += [path] if path.is_file() else [p for p in path.rglob('*') if p.is_file() and '__pycache__' not in p.parts]
        h = hashlib.sha256()
        for path in sorted(files, key=lambda p: p.relative_to(ROOT).as_posix()):
            h.update(path.relative_to(ROOT).as_posix().encode() + b'\0' + path.read_bytes() + b'\0')
        groups[label + '_sha256'] = h.hexdigest()
    return groups


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--probe', type=Path, required=True)
    p.add_argument('--output-dir', type=Path, required=True)
    p.add_argument('--expected-uuid', required=True)
    p.add_argument('--pairs', type=int, default=3)
    p.add_argument('--seconds', type=float, default=10)
    p.add_argument('--frames', type=int, default=240)
    args = p.parse_args()
    if args.pairs < 3 or args.seconds < 5 or args.frames < 200:
        p.error('Require at least three pairs, five seconds and 200 measured frames')
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    probe = args.probe.resolve()
    frozen = {**inputs(), 'probe_sha256': sha(probe),
              'application_sha256': sha(probe.parents[1] / 'PlanetSimulation')}
    source = ROOT / 'configs/scenarios/solar_system.json'
    scene = json.loads(source.read_text())
    replay = out / 'production-input.json'
    write_json(replay, {'scenario': scene, 'surface_camera': scene['surface_camera']})
    devices = subprocess.check_output(['nvidia-smi', '--query-gpu=index,name,uuid,pci.bus_id,driver_version,memory.total', '--format=csv'], text=True)
    (out / 'devices.csv').write_text(devices)
    report = {'base_revision': subprocess.check_output(['git', '-c', f'safe.directory={ROOT}', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
              'utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()), 'platform': platform.platform(),
              'frozen_provenance': frozen, 'production_config_sha256': sha(source),
              'replay_sha256': sha(replay), 'expected_uuid': args.expected_uuid,
              'method': {'pairs': args.pairs, 'minimum_duration_s': args.seconds, 'minimum_frames': args.frames,
                         'warmup': 'ready third-person, >=3 seconds and >=30 frames before measurement',
                         'simulation_s': 0, 'spin_orbit': 'paused by public replay startup',
                         'viewport': [1280, 720], 'requested_swap_interval': 0,
                         'presentation': 'native GLFW GLX/Xvfb; requested uncapped, actual presentation retained in native wall',
                         'observer': 'private production-state inspection; measured primary observer overhead included, own timing CSV write excluded from observer receipt',
                         'exclusions': 'startup/camera transition, warmup and close; all raw rows retained; no measured outlier filtering'},
              'runs': [], 'pairs': []}
    for pair in range(1, args.pairs + 1):
        matched = {}
        for backend in (('cpu', 'compute') if pair % 2 else ('compute', 'cpu')):
            output = out / f'pair-{pair}' / backend
            trace = output / 'performance.csv'
            # Camera-only replay selects legacy planning unless explicitly
            # overridden. Keep the requested backend/planner pair unambiguous.
            flags = ['--replay', str(replay), '--terrain-backend', backend,
                     '--terrain-grass-planner', 'gpu' if backend == 'compute' else 'cpu',
                     '--performance-trace', str(trace)]
            session = Session(probe, ROOT, output, flags,
                              environment={'PLANET_NATIVE_BENCHMARK': '1'}, control={'phase': 'startup'})
            try:
                session.focus()
                session.key('4')
                def ready(f):
                    return f['mode'] == 3 and f.get('pose') and not f['pose']['airborne'] and not f['publication'].get('loading', False)
                session.wait(ready, timeout=180)
                warm = session.control({'phase': 'warmup'})
                session.wait(lambda f: ready(f) and f['frame'] >= warm['frame'] + 30 and
                             f['observed_ns'] >= warm['observed_ns'] + 3_000_000_000, timeout=180)
                start = session.control({'phase': 'measured'})
                end = session.wait(lambda f: ready(f) and f['frame'] >= start['frame'] + args.frames - 1 and
                                   f['observed_ns'] >= start['observed_ns'] + args.seconds * 1e9, timeout=180)
                session.control({'phase': 'close'})
                session.close()
                audit = validate(session.frames, managed=backend == 'compute')
                summary = summarize(trace, session.frames, args.expected_uuid)
                summary.update({'pair': pair, 'backend': backend, 'command': [str(probe), *flags],
                                'audit': audit, 'end_landmark_frame': end['profile_frame'],
                                'frozen_provenance': frozen, 'replay_sha256': sha(replay)})
                assert inputs() == {k: frozen[k] for k in ['source_sha256', 'build_test_sha256']}
                assert sha(probe) == frozen['probe_sha256'] and sha(source) == report['production_config_sha256']
                write_json(output / 'summary.json', summary)
                matched[backend] = summary
                report['runs'].append(summary)
                write_json(out / 'results.json', report)
                print(f"pair {pair} {backend}: {summary['measured_frames']} frames, native p95 {summary['stats']['wall_ms']['p95']:.3f} ms, density {summary['workload_first']['bodies'][0]['foliage']['density']:.3f}", flush=True)
            finally:
                session.abort()
        report['pairs'].append({'pair': pair, **compare_pair(matched['cpu'], matched['compute'])})
        write_json(out / 'results.json', report)
    print('Completed three production stationary preflight pairs; remaining acceptance gates are explicit', flush=True)


if __name__ == '__main__':
    main()
