#!/usr/bin/env python3
"""Current production CPU/GPU feature costs; sequential, UUID-verified NVIDIA runs.

Raw traces/images stay local. Render-stage scopes are inclusive; nested compute
work and CPU wall/thread CPU timings must never be added to GPU totals.
"""
import argparse
import csv
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import re
import statistics
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'scripts/benchmarks'))
from archives.local import local_output
from cpu_report import analyze, write_report


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def stats(values):
    ordered = sorted(values)
    if not ordered:
        return {'n': 0}
    return {'n': len(values), 'mean': statistics.mean(values),
            'median': statistics.median(values),
            'p95': ordered[math.ceil(.95 * len(values)) - 1],
            'p99': ordered[math.ceil(.99 * len(values)) - 1], 'max': max(values)}


def summarize(folder, frames, warmup):
    rows = sorted(csv.DictReader((folder / 'frames.csv').open()), key=lambda r: int(r['frame']))
    assert [int(r['frame']) for r in rows] == list(range(frames))
    measured = [r for r in rows if warmup <= int(r['frame']) < frames - 1]
    assert all(int(r['scene_reuses']) == 0 for r in measured), 'Cached scene in full-render study'
    assert all(r['gpu_status'] == 'ready' and int(r['gpu_events_dropped']) == 0 for r in measured)
    steady = [r for r in measured if not int(r['mesh_uploads']) and not int(r['foliage_rebuilds'])
              and float(r['terrain_build_ms']) == 0]
    columns = [k for k in rows[0] if k.endswith('_ms')]
    work = list(csv.DictReader((folder / 'frames.csv.gpu-work.csv').open()))
    events = {}
    for row in work:
        if row['frame'] and warmup <= int(row['frame']) < frames - 1:
            key = row['stage'] + (':' + row['view'] if row['view'] else '')
            events.setdefault(key, []).append(row)
    return {'measured_frames': len(measured), 'steady_frames': len(steady),
            'stats': {k: stats([float(r[k]) for r in measured if r[k]]) for k in columns},
            'steady_stats': {k: stats([float(r[k]) for r in steady if r[k]]) for k in columns},
            'counts': {k: sum(int(r[k]) for r in measured) for k in
                       ('mesh_uploads', 'foliage_rebuilds', 'shadow_updates', 'shadow_reuses', 'scene_reuses')},
            'cold_frame': rows[0],
            'rebuild_frames': [r for r in measured if r not in steady],
            'nested_work': {k: {'events': len(v), 'missing': sum(r['status'] != 'ready' for r in v),
                               'cpu_submit_ms': stats([float(r['cpu_submit_ms']) for r in v]),
                               'gpu_ms': stats([float(r['gpu_ms']) for r in v if r['status'] == 'ready'])}
                            for k, v in events.items()}}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--binary', type=Path, required=True)
    p.add_argument('--config', type=Path, default=ROOT / 'configs/scenarios/solar_system.json')
    p.add_argument('--output-dir', type=local_output, required=True)
    p.add_argument('--devices', type=int, nargs='+', default=[0, 1])
    p.add_argument('--pairs', type=int, default=3)
    p.add_argument('--frames', type=int, default=120)
    p.add_argument('--warmup', type=int, default=20)
    p.add_argument('--resume', action='store_true', help='Keep completed runs after validating frozen inputs')
    args = p.parse_args()
    if args.pairs < 3 or args.frames <= args.warmup + 20 or len(set(args.devices)) != len(args.devices):
        p.error('Need three repeats, distinct devices and at least twenty measured frames')
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    binary, config = args.binary.resolve(), args.config.resolve()
    frozen = {'binary_sha256': sha(binary), 'config_sha256': sha(config),
              'asset_sha256': sha(ROOT / 'data/character/aces/suit.bin')}
    scene = json.loads(config.read_text())
    (out / 'startup.json').write_bytes(config.read_bytes())
    mountain = json.loads(config.read_text())
    mountain['surface_camera'].update(latitude_deg=-18.726025155509635,
        longitude_deg=113.23762488691663, altitude=.002, simulation_time_seconds=180.34667306699998,
        direction_ned=[.7430543107717261, .15323081761658303, .6514526903590994],
        up_ned=[.6380276560948606, .13157248129530347, -.7586892593307851])
    (out / 'mountain.json').write_text(json.dumps(mountain, indent=2) + '\n')
    adapter = out / 'libegl_capture.so'
    subprocess.run(['c++', '-std=c++17', '-O2', '-fPIC', '-shared',
        str(ROOT / 'scripts/benchmarks/gpu_compare/EglCapture.cpp'), '-o', str(adapter),
        '-lEGL', '-lGL', '-lGLEW', '-ldl'], check=True)
    vendor = out / 'nvidia-egl.json'
    vendor.write_text(json.dumps({'file_format_version': '1.0.0', 'ICD': {'library_path': 'libEGL_nvidia.so.0'}}))
    env = {**os.environ, '__EGL_VENDOR_LIBRARY_FILENAMES': str(vendor), 'LD_PRELOAD': str(adapter)}
    for key in ('LIBGL_ALWAYS_SOFTWARE', 'MESA_GL_VERSION_OVERRIDE', 'MESA_GLSL_VERSION_OVERRIDE',
                '__GLX_VENDOR_LIBRARY_NAME', '__NV_PRIME_RENDER_OFFLOAD'):
        env.pop(key, None)
    identity = subprocess.check_output(['nvidia-smi', '--query-gpu=index,name,uuid,driver_version,memory.total',
                                       '--format=csv'], text=True)
    (out / 'devices.csv').write_text(identity)
    devices = {r[' uuid'].strip(): {k.strip(): v.strip() for k, v in r.items()}
               for r in csv.DictReader(identity.splitlines())}
    baseline = subprocess.check_output(['nvidia-smi', '--query-gpu=index,name,uuid,temperature.gpu,utilization.gpu,memory.used',
                                       '--format=csv'], text=True)
    report = {'generated_utc': datetime.now(timezone.utc).isoformat(),
              'revision': subprocess.check_output(['git', '-c', f'safe.directory={ROOT}', 'rev-parse', 'HEAD'], text=True).strip(),
              **frozen, 'script_sha256': sha(Path(__file__)), 'adapter_source_sha256': sha(ROOT / 'scripts/benchmarks/gpu_compare/EglCapture.cpp'),
              'baseline_device_telemetry': baseline,
              'method': {'size': [1280, 720], 'frames': args.frames, 'warmup': args.warmup,
                         'excluded_final_frame': args.frames - 1, 'pairs': args.pairs,
                         'step_s': 1 / 60, 'walking_m_per_frame': .1,
                         'backend': 'compute', 'grass_planner': 'gpu-v1',
                         'presentation': 'uncapped NVIDIA EGL pbuffer; no desktop presentation',
                         'quality': 'current config, adaptive budgets retained; differing camera/workload counts are reported'},
              'config_summary': {'surface_camera': scene['surface_camera'], 'foliage': scene['planets'][0]['foliage'],
                                 'terrain_lod': scene['planets'][0]['terrain_lod']}, 'runs': [], 'cpu_diagnostics': []}
    if args.resume:
        previous = json.loads((out / 'results.json').read_text())
        assert all(previous[k] == v for k, v in frozen.items()), 'Cannot resume changed binary/config/asset'
        assert previous['method'] == report['method'], 'Cannot resume a different measurement method'
        report = previous
        report['diagnostic_script_sha256'] = sha(Path(__file__))
    cases = [('surface-idle', 'startup', False, 0), ('surface-walk', 'startup', False, .1),
             ('aces-idle', 'startup', True, 0), ('aces-walk', 'startup', True, .1),
             ('mountain-idle', 'mountain', False, 0)]
    telemetry = (out / ('telemetry-resumed.csv' if args.resume else 'telemetry.csv')).open('w')
    monitor = subprocess.Popen(['nvidia-smi', '--query-gpu=timestamp,index,name,uuid,temperature.gpu,clocks.sm,clocks.mem,utilization.gpu,memory.used',
                                '--format=csv', '-lms', '500'], stdout=telemetry)

    def run(case, device, pair, cpu_trace=False):
        label, view, astronaut, walk = case
        folder = out / ('cpu' if cpu_trace else 'timings') / label / f'pair-{pair}' / f'device-{device}'
        folder.mkdir(parents=True, exist_ok=True)
        image = folder / 'frame.png'
        frames, warmup = (args.frames, args.warmup)
        command = [str(binary), '--config', str(out / (view + '.json')), '--terrain-backend', 'compute',
                   '--terrain-grass-planner', 'gpu', '--astronaut-capture' if astronaut else '--surface-capture',
                   str(image), '--render-size', '1280', '720', '--benchmark-frames', str(frames),
                   '--benchmark-step', str(1 / 60), '--benchmark-walk-step', str(walk),
                   '--performance-trace', str(folder / 'frames.csv')]
        if cpu_trace:
            command += ['--cpu-trace', str(folder / 'cpu.json')]
        started = time.time()
        with (folder / 'run.log').open('w') as log:
            result = subprocess.run(command, cwd=ROOT, env={**env, 'PLANET_EGL_DEVICE': str(device)},
                                    stdout=log, stderr=subprocess.STDOUT, timeout=240)
        log = (folder / 'run.log').read_text()
        assert result.returncode == 0, log[-4000:]
        assert 'Shader compilation error' not in log and 'Shader linking error' not in log
        uuid = re.search(r'EGL device UUID: (GPU-[a-f0-9-]+)', log)[1]
        assert uuid in devices
        metadata = json.loads(Path(str(image) + '.json').read_text())
        render = metadata['render']
        assert devices[uuid]['name'] in render['renderer'] and render['terrain_backend'] == 'compute'
        assert render['terrain_grass_planner'] == 'gpu-v1' and [render['width'], render['height']] == [1280, 720]
        if astronaut:
            assert render['astronaut_model']['name'] == 'NASA ACES' and render['astronaut_pixels'] > 0
        summary = {'case': label, 'device': device, 'uuid': uuid, 'gpu': devices[uuid], 'pair': pair,
                   'started_unix_s': started, 'ended_unix_s': time.time(), 'command': command,
                   'image_sha256': sha(image), 'render': render, 'surface_camera': metadata['surface_camera'],
                   'elapsed_s': time.time() - started, **summarize(folder, frames, warmup)}
        assert {'binary_sha256': sha(binary), 'config_sha256': sha(config),
                'asset_sha256': sha(ROOT / 'data/character/aces/suit.bin')} == frozen, 'Inputs changed during measurement'
        if cpu_trace:
            document = json.loads((folder / 'cpu.json').read_text())
            write_report(document, folder / 'warm-report', warmup, frames - warmup - 1)
            events, rows, _, _ = analyze(document, warmup, frames - warmup - 1)
            summary['cpu_scopes'] = [dict(thread=thread, scope=name, **row) for (thread, name), row in rows.items()]
            _, cold, _, _ = analyze(document, 0, 1)
            summary['cold_cpu_scopes'] = [dict(thread=thread, scope=name, **row) for (thread, name), row in cold.items()]
            summary['cpu_trace_sha256'] = sha(folder / 'cpu.json')
            report['cpu_diagnostics'].append(summary)
        else:
            report['runs'].append(summary)
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
        print(f'{label} pair {pair}, {devices[uuid]["name"]}: median {summary["stats"]["frame_ms"]["median"]:.3f} ms; p95 {summary["stats"]["frame_ms"]["p95"]:.3f} ms; rebuilds {summary["counts"]["mesh_uploads"]}', flush=True)

    try:
        for pair in range(1, args.pairs + 1):
            for case in cases:
                for device in args.devices if pair % 2 else list(reversed(args.devices)):
                    if not any(r['case'] == case[0] and r['device'] == device and r['pair'] == pair for r in report['runs']):
                        run(case, device, pair)
        # Separate instrumented diagnostics; never replace the primary timing runs.
        for device in args.devices:
            for case in (cases[0], cases[3], cases[4]):
                if not any(r['case'] == case[0] and r['device'] == device for r in report['cpu_diagnostics']):
                    run(case, device, 1, True)
    finally:
        monitor.terminate()
        monitor.wait(timeout=10)
        telemetry.close()
    report['validation'] = 'Frozen binary/config/asset; physical GPU UUIDs; complete frame/stage samples; no cached renders; first 20 and final frame excluded; primary runs separate from CPU trace diagnostics.'
    (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    print('Validated current NVIDIA feature timing study', flush=True)


if __name__ == '__main__':
    main()
