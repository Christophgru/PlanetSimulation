#!/usr/bin/env python3
"""Three alternating production CPU/compute pairs per native movement route."""

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
import time

sys.dont_write_bytecode = True
from run import ROOT, inputs, sha, production_input
from NativeSession import Session, validate, write_json
from route_receipts import compare_routes, summarize_route


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--probe', type=Path, required=True)
    p.add_argument('--output-dir', type=local_output, required=True)
    p.add_argument('--expected-uuid', required=True)
    p.add_argument('--distance', type=int, default=400)
    p.add_argument('--pairs', type=int, default=3)
    p.add_argument('--quality-replay', type=Path, help='Public production capture with a common locked foliage policy')
    p.add_argument('--case', choices=('both', 'walking', 'sprint'), default='both',
                   help='Repeat one bounded cohort without repeating completed routes')
    args = p.parse_args()
    if args.distance < 300 or args.distance > 1000 or args.distance % 25 or args.pairs < 3:
        p.error('Require 300..1000 m routes in 25 m multiples and at least three pairs')
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    probe = args.probe.resolve()
    frozen = {**inputs(), 'probe_sha256': sha(probe), 'application_sha256': sha(probe.parents[1] / 'PlanetSimulation')}
    scene = json.loads((ROOT / 'configs/scenarios/solar_system.json').read_text())
    replay = out / 'production-input.json'
    write_json(replay, production_input(scene, args.quality_replay))
    (out / 'devices.csv').write_text(subprocess.check_output(['nvidia-smi', '--query-gpu=name,uuid,driver_version,memory.total', '--format=csv'], text=True))
    report = {'base_revision': subprocess.check_output(['git', '-c', f'safe.directory={ROOT}', 'rev-parse', 'HEAD'], text=True).strip(),
              'utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()), 'scope': 'Native route cost preflight; rendered near-root coverage and final migration acceptance remain pending',
              'frozen_provenance': frozen, 'replay_sha256': sha(replay), 'expected_uuid': args.expected_uuid,
              'method': {'pairs_per_route': args.pairs, 'route_distance_m': args.distance, 'measured_interval_m': [5, args.distance],
                         'warmup': 'ready standing, >=3 seconds and 30 frames', 'minimum_frames_per_run': 240,
                         'landmarks_m': 25, 'simulation': 'time-zero public replay, orbit/spin paused; wind active',
                         'quality': 'untouched production 1280x720/100k Earth cap/2M foliage budget and optical consumers',
                         'presentation': 'native Quadro GLX/Xvfb, requested swap interval zero',
                         'exclusions': 'startup, warmup, input confirmation/first 5 m, endpoint crossing/keyup/settle/close; every raw row retained',
                         'observer': 'primary observer measured and included in uncorrected native wall; no outlier removal'},
              'runs': [], 'pairs': []}
    for case, sprint in [('walking', False), ('sprint', True)]:
        if args.case not in ('both', case):
            continue
        for pair in range(1, args.pairs + 1):
            matched = {}
            for backend in (('cpu', 'compute') if pair % 2 else ('compute', 'cpu')):
                folder = out / case / f'pair-{pair}' / backend
                trace = folder / 'performance.csv'
                flags = ['--replay', str(replay), '--terrain-backend', backend, '--terrain-grass-planner',
                         'gpu' if backend == 'compute' else 'cpu', '--performance-trace', str(trace)]
                session = Session(probe, ROOT, folder, flags, environment={'PLANET_NATIVE_BENCHMARK': '1'}, control={'phase': 'startup'})
                try:
                    session.focus()
                    session.key('4')
                    def grounded(f):
                        return f['mode'] == 3 and f.get('pose') and not f['pose']['airborne'] and not f['publication'].get('loading', False)
                    first = session.wait(grounded, timeout=180)
                    session.wait(lambda f: grounded(f) and f['frame'] >= first['frame'] + 30 and
                                 f['observed_ns'] >= first['observed_ns'] + 3_000_000_000, timeout=180)
                    session.control({'phase': 'route'})
                    if sprint:
                        session.down('Shift_L')
                    session.down('w')
                    try:
                        end = session.wait(lambda f: grounded(f) and f['pose']['walked_m'] >= args.distance,
                                           'Route did not reach the common endpoint', timeout=240)
                    finally:
                        session.up('w')
                        if sprint:
                            session.up('Shift_L')
                    session.control({'phase': 'settle'})
                    session.wait(lambda f: grounded(f) and not f['keys']['w'] and f['frame'] > end['frame'] + 10)
                    session.close()
                    audit = validate(session.frames, managed=backend == 'compute')
                    summary = summarize_route(trace, session.frames, args.expected_uuid, sprint, args.distance)
                    summary.update({'case': case, 'backend': backend, 'pair': pair, 'command': [str(probe), *flags],
                                    'audit': audit, 'frozen_provenance': frozen})
                    assert {**inputs(), 'probe_sha256': sha(probe), 'application_sha256': sha(probe.parents[1] / 'PlanetSimulation')} == frozen
                    assert sha(replay) == report['replay_sha256']
                    write_json(folder / 'summary.json', summary)
                    matched[backend] = summary
                    report['runs'].append(summary)
                    write_json(out / 'results.json', report)
                    print(f'{case} pair {pair} {backend}: {summary["measured_frames"]} frames, {summary["wall_mps"]:.3f} m/s, native p95 {summary["stats"]["wall_ms"]["p95"]:.3f} ms', flush=True)
                finally:
                    session.abort()
            comparison = {'case': case, 'pair': pair, **compare_routes(matched['cpu'], matched['compute'])}
            report['pairs'].append(comparison)
            write_json(out / 'results.json', report)
    print('Completed three alternating pairs for each route; coverage/final cost gates remain', flush=True)


if __name__ == '__main__':
    main()
