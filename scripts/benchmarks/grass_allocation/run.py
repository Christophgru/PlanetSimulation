#!/usr/bin/env python3
"""Paired grass timings: real GLFW input loop with UUID-selected NVIDIA EGL rendering.

Run under xvfb-run. Raw evidence must stay in an ignored build directory.
The private adapter uses a pbuffer, so results do not certify desktop FPS.
"""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'scripts/benchmarks'))
from archives.local import local_output
sys.path.insert(0, str(ROOT / 'scripts/benchmarks/feature_costs'))
from run import stats
sys.path.insert(0, str(ROOT / 'tests/app/terrain/native'))
from NativeSession import Session, validate, write_json


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def shader_hashes(root):
    return {str(p.relative_to(root)): sha(p) for p in sorted((root / 'shaders').rglob('*')) if p.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--before-probe', type=Path, required=True)
    parser.add_argument('--before-root', type=Path, required=True)
    parser.add_argument('--after-probe', type=Path, required=True)
    parser.add_argument('--config', type=Path, default=ROOT / 'configs/scenarios/solar_system.json')
    parser.add_argument('--output-dir', type=local_output, required=True)
    parser.add_argument('--device', type=int, default=1)
    parser.add_argument('--pairs', type=int, default=3)
    parser.add_argument('--seconds', type=float, default=6)
    parser.add_argument('--camera-mode', choices=['surface', 'astronaut'], default='surface')
    parser.add_argument('--stages', nargs='+', default=['grass_metadata', 'grass_allocation'],
                        choices=['grass_metadata', 'grass_allocation', 'grass_roots', 'grass_placement', 'grass_animation', 'grass_draw', 'grass_cells'])
    args = parser.parse_args()
    if args.pairs < 3 or args.seconds < 5:
        parser.error('Use at least three pairs and five seconds per measured leg')
    out = args.output_dir.resolve();out.mkdir(parents=True, exist_ok=True)
    probes = {'before': args.before_probe.resolve(), 'after': args.after_probe.resolve()}
    roots = {'before': args.before_root.resolve(), 'after': ROOT}
    frozen = {k: sha(v) for k, v in probes.items()}
    identities = list(csv.DictReader(subprocess.check_output([
        'nvidia-smi', '--query-gpu=index,name,uuid,driver_version,memory.total', '--format=csv'], text=True).splitlines()))
    device = next({k.strip(): v.strip() for k, v in d.items()} for d in identities if int(d['index']) == args.device)
    scene = json.loads(args.config.read_text())
    scene['surface_camera']['walk_speed_mps'] = 6
    replay = out / 'input.json'
    write_json(replay, {'scenario': scene, 'surface_camera': scene['surface_camera']})
    adapter = out / 'libnative_egl.so'
    subprocess.run(['c++', '-std=c++17', '-O2', '-shared', '-fPIC',
                    str(Path(__file__).with_name('NativeEgl.cpp')), '-o', str(adapter), '-lEGL', '-lGL', '-lGLEW', '-ldl'], check=True)
    vendor = out / 'nvidia.json'
    write_json(vendor, {'file_format_version': '1.0.0', 'ICD': {'library_path': 'libEGL_nvidia.so.0'}})
    environment = {**os.environ, 'LD_PRELOAD': str(adapter), 'PLANET_EGL_DEVICE': str(args.device),
                   '__EGL_VENDOR_LIBRARY_FILENAMES': str(vendor), 'PLANET_NATIVE_BENCHMARK': '1'}
    for key in ('LIBGL_ALWAYS_SOFTWARE', 'MESA_GL_VERSION_OVERRIDE', 'MESA_GLSL_VERSION_OVERRIDE'):
        environment.pop(key, None)
    shaders = {k: shader_hashes(v) for k, v in roots.items()}
    report = {'device': device, 'probe_sha256': frozen, 'shader_sha256': shaders,
              'adapter_sha256': sha(adapter), 'adapter_source_sha256': sha(Path(__file__).with_name('NativeEgl.cpp')),
              'base_commit': subprocess.check_output(['git', '-c', f'safe.directory={ROOT}', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
              'config_sha256': sha(args.config),
              'input_sha256': sha(replay), 'method': {'pairs': args.pairs, 'seconds': args.seconds,
              'stages': args.stages,
              'camera_mode': args.camera_mode,
              'warmup': 'settled selected camera, then >=3 seconds and >=30 frames', 'walking_mps': 6,
              'viewport': [1280,720], 'orbit': 'paused replay; wind live',
              'presentation': 'real GLFW/X11 input and production loop; NVIDIA EGL pbuffer; no desktop FPS claim'}, 'runs': []}
    telemetry = (out / 'telemetry.csv').open('w')
    monitor = subprocess.Popen(['nvidia-smi', '--query-gpu=timestamp,index,name,uuid,utilization.gpu,memory.used,temperature.gpu',
                                '--format=csv', '-lms', '500'], stdout=telemetry)
    try:
        for pair in range(1, args.pairs + 1):
            for case in ('idle', 'walk'):
                for variant in (('before', 'after') if pair % 2 else ('after', 'before')):
                    folder = out / f'pair-{pair}' / case / variant
                    trace = folder / 'frames.csv'
                    session = Session(probes[variant], roots[variant], folder,
                        ['--replay', str(replay), '--terrain-backend', 'compute', '--terrain-grass-planner', 'gpu',
                         '--uncapped', '--performance-trace', str(trace)], environment=environment, control={'phase': 'startup'})
                    try:
                        session.focus();session.key('4' if args.camera_mode == 'astronaut' else '2')
                        def ready(f):
                            return f['mode'] == (3 if args.camera_mode == 'astronaut' else 1) and not f['publication'].get('loading', True) and (
                                args.camera_mode != 'astronaut' or (f.get('pose') and not f['pose']['airborne']))
                        session.wait(lambda f: ready(f) and not f['publication']['pending'] and
                                     not any(f['worker'].values()), timeout=180)
                        warm = session.control({'phase': 'warmup'})
                        session.wait(lambda f: ready(f) and f['frame'] >= warm['frame'] + 30 and
                                     f['observed_ns'] >= warm['observed_ns'] + 3_000_000_000, timeout=180)
                        start = session.control({'phase': 'measured'})
                        if case == 'walk': session.down('w')
                        end = session.wait(lambda f: ready(f) and f['frame'] >= start['frame'] + 100 and
                                           f['observed_ns'] >= start['observed_ns'] + args.seconds * 1e9, timeout=180)
                        if case == 'walk': session.up('w')
                        session.control({'phase': 'excluded'})
                        session.wait(lambda f: f['frame'] >= end['frame'] + 12 and
                                     f['observed_ns'] >= end['observed_ns'] + 500_000_000)
                        session.close()
                        audit = validate(session.frames)
                        frames = [f for f in session.frames if start['frame'] <= f['frame'] <= end['frame']]
                        ids = {f['profile_frame'] for f in frames}
                        assert all(f['renderer'].startswith(device['name']) for f in frames)
                        assert all(f['benchmark']['scene_size'] == [1280,720] for f in frames)
                        assert f"EGL device UUID: {device['uuid']}" in session.text()
                        rows = [r for r in csv.DictReader(trace.open()) if int(r['frame']) in ids]
                        loops = [r for r in csv.DictReader(Path(str(trace)+'.native-loop.csv').open()) if int(r['frame']) in ids]
                        work = [r for r in csv.DictReader(Path(str(trace)+'.gpu-work.csv').open()) if r['frame'] and int(r['frame']) in ids]
                        assert len(rows) == len(loops) == len(frames)
                        assert all(r['gpu_status'] == 'ready' and not int(r['scene_reuses']) and not int(r['gpu_events_dropped']) for r in rows)
                        assert all(r['status'] == 'ready' for r in work)
                        policies = [f['benchmark']['bodies'][0]['foliage']['policy'] for f in frames]
                        assert all(p['density'] == scene['planets'][0]['foliage']['density_per_m2'] and not p['near_infeasible'] for p in policies)
                        events = {stage: stats([float(r['gpu_ms']) for r in work if r['stage'] == stage])
                                  for stage in args.stages}
                        entry = {'pair': pair, 'case': case, 'variant': variant, 'frames': len(rows),
                                 'first_frame': start['profile_frame'], 'last_frame': end['profile_frame'], 'audit': audit,
                                 'wall_ms': stats([float(r['wall_ms']) for r in loops]),
                                 'mesh_ms': stats([float(r['cpu_mesh_ms']) for r in rows]),
                                 'gpu_render_ms': stats([float(r['gpu_ms']) for r in rows]), 'events': events,
                                 'opaque_gpu_ms': stats([float(r['gpu_opaque_ms']) for r in rows]),
                                 'reflection_gpu_ms': stats([float(r['gpu_reflection_ms']) for r in rows]),
                                 'trail_segments': [frames[0].get('pose', {}).get('trail_segments', 0),
                                                    frames[-1].get('pose', {}).get('trail_segments', 0)],
                                 'first_workload': frames[0]['benchmark'], 'last_workload': frames[-1]['benchmark'],
                                 'maximum_reserved_bytes': max(f['publication']['peak_reserved_bytes'] for f in frames),
                                 'raw_folder': str(folder.relative_to(out))}
                        if args.camera_mode == 'astronaut' and case == 'walk':
                            entry['walking_speed_mps'] = (end['pose']['walked_m']-start['pose']['walked_m'])/((end['observed_ns']-start['observed_ns'])*1e-9)
                            assert abs(entry['walking_speed_mps']-6) < .12, entry['walking_speed_mps']
                            assert entry['trail_segments'][1] > entry['trail_segments'][0]
                        report['runs'].append(entry);write_json(out/'results.json', report)
                        stage = 'grass_placement' if 'grass_placement' in events else args.stages[0]
                        print(f"pair {pair} {case} {variant}: wall p95 {entry['wall_ms']['p95']:.3f} ms, {stage} {events[stage]}", flush=True)
                    finally:
                        session.abort()
                    assert {k: sha(v) for k,v in probes.items()} == frozen
                    assert {k: shader_hashes(v) for k,v in roots.items()} == shaders
    finally:
        monitor.terminate();monitor.wait(timeout=10);telemetry.close()
    report['validation'] = 'All UUIDs, frame joins, samples, density and native audits passed; no measured outliers removed.'
    write_json(out/'results.json', report)


if __name__ == '__main__':
    main()
