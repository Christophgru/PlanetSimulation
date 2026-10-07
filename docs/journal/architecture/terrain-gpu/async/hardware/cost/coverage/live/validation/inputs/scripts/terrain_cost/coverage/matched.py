#!/usr/bin/env python3
"""Alternating excluded native coverage routes using common inspection controls."""
import argparse
import csv
from datetime import datetime, timezone
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import time
sys.dont_write_bytecode = True
# Load the standard library before adding the historical CLI directory, which
# contains inspect.py. NumPy needs the standard inspect module.
import inspect
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from run import ROOT, inputs, sha
from NativeSession import Session, validate, write_json
spec = importlib.util.spec_from_file_location('matched_analysis', ROOT / 'tests/app/terrain/native/coverage/matched/measure.py')
analysis = importlib.util.module_from_spec(spec)
spec.loader.exec_module(analysis)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--probe', type=Path, required=True)
    p.add_argument('--output-dir', type=Path, required=True)
    p.add_argument('--expected-uuid', required=True)
    p.add_argument('--distance', type=int, default=350)
    p.add_argument('--case', choices=('walking', 'sprint'), default='sprint')
    p.add_argument('--pairs', type=int, choices=(1, 3), default=3)
    p.add_argument('--live', action='store_true', help='Qualify tiled live analytic area; all frames remain excluded')
    args = p.parse_args()
    global analysis
    if args.live:
        spec = importlib.util.spec_from_file_location('live_analysis', ROOT / 'tests/app/terrain/native/coverage/live/measure.py')
        analysis = importlib.util.module_from_spec(spec); spec.loader.exec_module(analysis)
    if not 25 <= args.distance <= 400:
        p.error('Landmark must be 25..400 m')
    out = args.output_dir.resolve()
    if out.exists():
        p.error('Choose a fresh output directory')
    out.mkdir(parents=True)
    frozen = {**inputs(), 'inspection_probe_sha256': sha(args.probe)}
    scene = json.loads((ROOT / 'configs/scenarios/solar_system.json').read_text())
    replay = out / 'production-input.json'
    write_json(replay, {'scenario': scene, 'surface_camera': scene['surface_camera']})
    report = {'schema': 1, 'scope': 'Excluded matched live-plan coverage; not cost timing',
              'base_revision': subprocess.check_output(['git', '-c', f'safe.directory={ROOT}', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
              'utc': datetime.now(timezone.utc).isoformat(),
              'frozen_provenance': frozen, 'replay_sha256': sha(replay), 'case': args.case,
              'target_distance_m': args.distance, 'expected_uuid': args.expected_uuid,
              'live_analytic_qualification': args.live, 'declared_area_convergence_tolerance': .01,
              'declared_relative_tolerance': .05, 'minimum_eligible_pixels_per_band': 100,
              'minimum_visible_roots_per_band': 100, 'runs': [], 'pairs': []}
    write_json(out / 'results.json', report) # Publish tolerances before measurements.
    (out / 'devices.csv').write_text(subprocess.check_output(['nvidia-smi', '--query-gpu=name,uuid,driver_version,memory.total', '--format=csv'], text=True))
    reference = None
    for pair in range(1, args.pairs+1):
        order = ('compute', 'cpu') if pair % 2 == 0 else ('cpu', 'compute')
        paired = {}
        for backend in order:
            folder = out / f'pair-{pair}' / backend
            trace = folder / 'performance.csv'
            flags = ['--replay', str(replay), '--terrain-backend', backend, '--terrain-grass-planner',
                     'gpu' if backend == 'compute' else 'cpu', '--performance-trace', str(trace)]
            session = Session(args.probe, ROOT, folder, flags, environment={'PLANET_NATIVE_BENCHMARK': '1',
                              'PLANET_NATIVE_COVERAGE': str(folder / 'coverage')})
            try:
                session.focus(); session.key('4')
                grounded = lambda f: f['mode'] == 3 and f.get('pose') and not f['pose']['airborne'] and not f['publication'].get('loading', False)
                first = session.wait(grounded, timeout=180)
                session.wait(lambda f: grounded(f) and f['frame'] >= first['frame']+30 and
                             f['observed_ns'] >= first['observed_ns']+3_000_000_000, timeout=180)
                control = {'coverage_request': 1, 'coverage_distance_m': args.distance,
                           'coverage_matched': True, 'coverage_close': True}
                if args.live: control['coverage_live'] = True
                if reference:
                    control['coverage_reference'] = str(reference / 'controls.json')
                session.control(control)
                if args.case == 'sprint':
                    session.down('Shift_L')
                session.down('w')
                session_start = time.monotonic()
                matched = folder / 'coverage/1/matched'
                # A close request stops after the complete receipt, avoiding a
                # blocking inspection delay contaminating another landmark.
                while session.process.poll() is None:
                    session.read()
                    if session.frames and session.frames[-1]['pose'] and session.frames[-1]['pose']['walked_m'] > 450:
                        raise AssertionError('Missed inspection landmark')
                    if time.monotonic() - session_start > 300:
                        raise AssertionError('Native matched route timeout')
                    time.sleep(.025)
                session.up('w')
                if args.case == 'sprint':
                    session.up('Shift_L')
                session.close()
                assert session.process.returncode == 0 and (matched / 'receipt.json').exists(), session.text()
                audit = validate(session.frames, managed=backend == 'compute')
                native = json.loads((folder / 'coverage/1/snapshot.json').read_text())
                assert native['workload']['quality_scale'] == 1 and native['viewport'] == [1280, 720]
                body = native['workload']['bodies'][0]
                assert body['triangles'] == 100000 and body['foliage']['configured_budget'] == body['foliage']['budget'] == 2000000
                rows = list(csv.DictReader(Path(str(trace)+'.memory.csv').open()))
                assert rows and all(r['context_uuid'] == args.expected_uuid and r['context_status'] == 'uuid_verified' and
                                    r['nvml_status'] == 'ok' for r in rows)
                frame = next(f for f in session.frames if f['profile_frame'] == native['profile_frame'])
                assert frame['pose']['root'] == native['root_m'] and frame['pose']['walked_m'] == native['walked_m']
                result = analysis.analyze(matched)
                assert result['composed_grass_pixels']
                if args.live:
                    assert min(s['character_pixels'] for s in result['samples']) > 100
                else:
                    assert result['character_pixels']
                assert {**inputs(), 'inspection_probe_sha256': sha(args.probe)} == frozen
                if reference is None:
                    reference = matched
                paired[backend] = matched
                report['runs'].append({'pair': pair, 'backend': backend, 'path': str(folder.relative_to(out)),
                                       'command': session.process.args,
                                       'audit': audit, 'analysis': result, 'walked_m': native['walked_m'],
                                       'all_native_frames_excluded_from_cost_acceptance': len(session.frames)})
                write_json(out / 'results.json', report)
                print(f'{args.case} {args.distance} m pair {pair} {backend}: '+
                      f'{native["walked_m"]:.3f} m, {result["composed_grass_pixels"]} composed grass pixels', flush=True)
            finally:
                session.up('w')
                if args.case == 'sprint':
                    session.up('Shift_L')
                session.abort()
        comparison = analysis.compare(paired['cpu'], paired['compute'], report['declared_relative_tolerance'])
        samples_valid = all(b['eligible_pixels'] >= 100 and b['ground_visible_roots'] >= 100
                            for r in report['runs'][-2:] for b in r['analysis']['bands'])
        comparison['sample_minimums_met'] = samples_valid
        comparison['coverage_parity'] &= samples_valid
        report['pairs'].append({'pair': pair, **comparison})
        write_json(out / 'results.json', report)
    print('Coverage comparisons retained; all route frames excluded from timing', flush=True)


if __name__ == '__main__':
    main()
