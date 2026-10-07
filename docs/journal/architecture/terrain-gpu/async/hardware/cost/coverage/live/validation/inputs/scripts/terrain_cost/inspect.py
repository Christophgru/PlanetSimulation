#!/usr/bin/env python3
"""Excluded live-generation inspection routes; never use these for cost timing."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
sys.dont_write_bytecode = True
from run import ROOT, inputs, sha
from NativeSession import Session, validate, write_json
sys.path.insert(0, str(ROOT / 'tests/app/terrain/native/coverage'))
from analyze import analyze


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--probe', type=Path, required=True)
    p.add_argument('--output-dir', type=Path, required=True)
    p.add_argument('--expected-uuid', required=True)
    p.add_argument('--distance', type=int, default=350)
    p.add_argument('--case', choices=('walking', 'sprint'), default='sprint')
    args = p.parse_args()
    if not 25 <= args.distance <= 400:
        p.error('Inspection distance must be 25..400 m')
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    frozen = {**inputs(), 'inspection_probe_sha256': sha(args.probe)}
    scene = json.loads((ROOT / 'configs/scenarios/solar_system.json').read_text())
    replay = out / 'production-input.json'
    write_json(replay, {'scenario': scene, 'surface_camera': scene['surface_camera']})
    report = {'scope': 'Blocking live-generation inspection foundation; coverage/cost acceptance pending',
              'frozen_provenance': frozen, 'replay_sha256': sha(replay), 'case': args.case,
              'target_distance_m': args.distance, 'expected_uuid': args.expected_uuid, 'runs': []}
    (out / 'devices.csv').write_text(subprocess.check_output(['nvidia-smi', '--query-gpu=name,uuid,driver_version,memory.total', '--format=csv'], text=True))
    for backend in ('cpu', 'compute'):
        folder = out / backend
        if (folder / 'coverage/1/snapshot.json').exists():
            p.error('Existing inspection snapshot; choose a fresh output directory')
        trace = folder / 'performance.csv'
        flags = ['--replay', str(replay), '--terrain-backend', backend, '--terrain-grass-planner',
                 'gpu' if backend == 'compute' else 'cpu', '--performance-trace', str(trace)]
        session = Session(args.probe, ROOT, folder, flags, environment={'PLANET_NATIVE_BENCHMARK': '1',
                          'PLANET_NATIVE_COVERAGE': str(folder / 'coverage')})
        try:
            session.focus()
            session.key('4')
            grounded = lambda f: f['mode'] == 3 and f.get('pose') and not f['pose']['airborne'] and not f['publication'].get('loading', False)
            first = session.wait(grounded, timeout=180)
            session.wait(lambda f: grounded(f) and f['frame'] >= first['frame'] + 30 and
                         f['observed_ns'] >= first['observed_ns'] + 3_000_000_000, timeout=180)
            session.control({'coverage_request': 1, 'coverage_distance_m': args.distance})
            if args.case == 'sprint':
                session.down('Shift_L')
            session.down('w')
            try:
                session.wait(lambda f: (folder / 'coverage/1/snapshot.json').exists(), timeout=240)
            finally:
                session.up('w')
                if args.case == 'sprint':
                    session.up('Shift_L')
            session.close()
            audit = validate(session.frames, managed=backend == 'compute')
            metadata = json.loads((folder / 'coverage/1/snapshot.json').read_text())
            assert metadata['profile_frame'] in {f['profile_frame'] for f in session.frames}
            assert metadata['walked_m'] >= args.distance
            assert metadata['workload']['quality_scale'] == 1 and metadata['viewport'] == [1280, 720]
            body = metadata['workload']['bodies'][0]
            assert body['triangles'] == 100000 and body['foliage']['configured_budget'] == body['foliage']['budget'] == 2000000
            import csv
            memory = list(csv.DictReader(Path(str(trace) + '.memory.csv').open()))
            assert memory and all(r['context_uuid'] == args.expected_uuid and r['context_status'] == 'uuid_verified' for r in memory)
            result = analyze(folder / 'coverage/1')
            assert sum(result['generated_instances']) > 0 and result['opaque_grass_pixels'] > 0
            assert {**inputs(), 'inspection_probe_sha256': sha(args.probe)} == frozen
            report['runs'].append({'backend': backend, 'audit': audit, 'analysis': result,
                                   'profile_frame': metadata['profile_frame'], 'walked_m': metadata['walked_m'],
                                   'all_native_frames_excluded_from_cost_acceptance': len(session.frames)})
            write_json(out / 'results.json', report)
            print(f'{backend}: {metadata["walked_m"]:.3f} m; roots {result["root_band_counts"]}; opaque grass pixels {result["opaque_grass_pixels"]}; inspection only', flush=True)
        finally:
            session.abort()


if __name__ == '__main__':
    main()
