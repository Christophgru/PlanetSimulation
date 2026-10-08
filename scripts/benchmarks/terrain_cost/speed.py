#!/usr/bin/env python3
"""Production native walking/sprint speed prerequisite; not cost acceptance."""

import sys
from pathlib import Path
_repo = next(p for p in Path(__file__).resolve().parents if (p/'scripts/benchmarks/archives').is_dir())
sys.path.insert(0, str(_repo/'scripts/benchmarks'))
from archives.local import local_output
import argparse
import json
from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True
from run import ROOT, inputs, sha
from NativeSession import Session, validate, write_json
from receipts import rows, percentiles


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--probe', type=Path, required=True)
    p.add_argument('--output-dir', type=local_output, required=True)
    p.add_argument('--expected-uuid', required=True)
    p.add_argument('--distance', type=float, default=18)
    p.add_argument('--presentation-delay-ms', type=int, default=0)
    args = p.parse_args()
    if not 12 <= args.distance <= 100 or not 0 <= args.presentation_delay_ms <= 500:
        p.error('Require 12..100 metre routes and 0..500 ms presentation delay')
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    probe = args.probe.resolve()
    frozen = {**inputs(), 'probe_sha256': sha(probe), 'application_sha256': sha(probe.parents[1] / 'PlanetSimulation')}
    scene = json.loads((ROOT / 'configs/scenarios/solar_system.json').read_text())
    replay = out / 'production-input.json'
    write_json(replay, {'scenario': scene, 'surface_camera': scene['surface_camera']})
    report = {'scope': 'Grounded speed prerequisite; single short input run per case/backend, no cost acceptance',
              'frozen_provenance': frozen, 'replay_sha256': sha(replay), 'expected_uuid': args.expected_uuid,
              'distance_m': args.distance, 'presentation_delay_ms': args.presentation_delay_ms,
              'method': 'time-zero public replay; 1280x720 production quality; real X11 W/Shift; >=3 s and 30 frame warmup; report animation and monotonic wall speed',
              'runs': []}
    (out / 'devices.csv').write_text(subprocess.check_output(['nvidia-smi', '--query-gpu=name,uuid,driver_version,memory.total', '--format=csv'], text=True))
    for case, sprint, expected in [('walking', False, 6), ('sprint', True, 12)]:
        for backend in ['cpu', 'compute']:
            folder = out / case / backend
            trace = folder / 'performance.csv'
            flags = ['--replay', str(replay), '--terrain-backend', backend, '--terrain-grass-planner',
                     'gpu' if backend == 'compute' else 'cpu', '--performance-trace', str(trace)]
            control = {'phase': 'startup', 'present_delay_ms': args.presentation_delay_ms}
            session = Session(probe, ROOT, folder, flags, environment={'PLANET_NATIVE_BENCHMARK': '1'}, control=control)
            try:
                session.focus()
                session.key('4')
                def grounded(f):
                    return f['mode'] == 3 and f.get('pose') and not f['pose']['airborne'] and not f['publication'].get('loading', False)
                start = session.wait(grounded, timeout=180)
                session.wait(lambda f: grounded(f) and f['frame'] >= start['frame'] + 30 and
                             f['observed_ns'] >= start['observed_ns'] + 3_000_000_000, timeout=180)
                session.control({**control, 'phase': 'movement'})
                if sprint:
                    session.down('Shift_L')
                session.down('w')
                try:
                    first = session.wait(lambda f: grounded(f) and f['keys']['w'] and f['keys']['shift'] == sprint)
                    last = session.wait(lambda f: grounded(f) and f['frame'] >= first['frame'] + 20 and
                                        f['pose']['walked_m'] >= first['pose']['walked_m'] + args.distance,
                                        'Production walking did not reach the route endpoint', timeout=120)
                finally:
                    session.up('w')
                    if sprint:
                        session.up('Shift_L')
                session.control({**control, 'phase': 'settle'})
                session.wait(lambda f: grounded(f) and not f['keys']['w'] and f['frame'] > last['frame'] + 3)
                session.close()
                audit = validate(session.frames, managed=backend == 'compute')
                moving = [f for f in session.frames if first['frame'] <= f['frame'] <= last['frame']]
                assert len(moving) >= 20 and all(grounded(f) and f['keys']['w'] and f['keys']['shift'] == sprint for f in moving)
                assert all(f['paused'] and f['benchmark']['quality_scale'] == 1 and
                           f['benchmark']['viewport'] == f['benchmark']['scene_size'] == [1280, 720] for f in moving)
                distance = last['pose']['walked_m'] - first['pose']['walked_m']
                wall = (last['observed_ns'] - first['observed_ns']) / 1e9
                animation = last['pose']['effect_s'] - first['pose']['effect_s']
                native = {int(r['frame']): r for r in rows(str(trace) + '.native-loop.csv')}
                memory = rows(str(trace) + '.memory.csv')
                assert all(r['context_uuid'] == args.expected_uuid and r['nvml_status'] == 'ok' for r in memory)
                speed = distance / wall
                summary = {'case': case, 'backend': backend, 'command': [str(probe), *flags], 'expected_mps': expected,
                           'first_frame': first['profile_frame'], 'last_frame': last['profile_frame'],
                           'movement_frames': len(moving), 'distance_m': distance, 'wall_s': wall, 'animation_s': animation,
                           'wall_mps': speed, 'animation_mps': distance / animation,
                           'wall_speed_pass': abs(speed / expected - 1) < .05,
                           'native_wall_ms': percentiles([float(native[f['profile_frame']]['wall_ms']) for f in moving]),
                           'renderer': first['renderer'], 'opengl_version': first['opengl_version'],
                           'initial_workload': first['benchmark'], 'audit': audit,
                           'maximum_contact_revision': max(f['publication'].get('astronaut_contact_revision', 0) for f in moving)}
                assert {**inputs(), 'probe_sha256': sha(probe), 'application_sha256': sha(probe.parents[1] / 'PlanetSimulation')} == frozen
                assert sha(replay) == report['replay_sha256']
                report['runs'].append(summary)
                write_json(folder / 'summary.json', summary)
                write_json(out / 'results.json', report)
                print(f'{case} {backend}: {speed:.3f} m/s wall, {distance / animation:.3f} m/s animation; pass={summary["wall_speed_pass"]}', flush=True)
            finally:
                session.abort()
    report['all_wall_speeds_pass'] = all(r['wall_speed_pass'] for r in report['runs'])
    write_json(out / 'results.json', report)


if __name__ == '__main__':
    main()
