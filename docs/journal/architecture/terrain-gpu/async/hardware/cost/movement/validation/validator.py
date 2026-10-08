#!/usr/bin/env python3
"""Recompute archived real-time movement speeds and stationary control joins."""
import argparse
import csv
import gzip
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import sys

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parent
DATA = ROOT / 'validation'


def read(path):
    return gzip.decompress(path.read_bytes()).decode() if path.suffix == '.gz' else path.read_text()


def rows(path):
    return list(csv.DictReader(io.StringIO(read(path))))


def validate():
    checks = {'speed_runs': [], 'movement_frames': 0, 'excluded_frames': 0}
    original = read(DATA / 'before/Interactive.cpp.gz')
    assert 'Ground walking stays capped.' in original and 'remainingGroundTime' not in original
    for label in ('before', 'short-preflight', 'after', 'slow'):
        report = json.loads((DATA / label / 'results.json').read_text())
        provenance = json.loads((DATA / ('before-provenance.json' if label == 'before' else 'after-provenance.json')).read_text())
        frozen = report['frozen_provenance']
        assert frozen['source_sha256'] == provenance['source_sha256']
        assert frozen['build_test_sha256'] == provenance['build_test_input_sha256']
        assert frozen['application_sha256'] == provenance['binaries']['build-resume/PlanetSimulation']
        assert frozen['probe_sha256'] == provenance['binaries']['build-resume/tests/terrain_native_probe']
        assert len(report['runs']) == 4
        for run in report['runs']:
            folder = DATA / label / run['case'] / run['backend']
            frames = [json.loads(line) for line in read(folder / 'frames.jsonl.gz').splitlines()]
            observed = {f['profile_frame']: f for f in frames}
            native = {int(r['frame']): r for r in rows(folder / 'performance.csv.native-loop.csv.gz')}
            perf = {int(r['frame']): r for r in rows(folder / 'performance.csv.gz')}
            observer = {int(r['frame']): r for r in rows(folder / 'frames.jsonl.observer.csv.gz')}
            assert len(observed) == len(frames) and observed.keys() == native.keys() == perf.keys() == observer.keys()
            moving = [f for f in frames if run['first_frame'] <= f['profile_frame'] <= run['last_frame']]
            assert len(moving) == run['movement_frames'] and len(moving) >= 20
            assert moving[-1]['profile_frame'] - moving[0]['profile_frame'] + 1 == len(moving)
            for f in moving:
                assert f['mode'] == 3 and f['paused'] and not f['pose']['airborne']
                assert f['keys']['w'] and f['keys']['shift'] == (run['case'] == 'sprint')
                b = f['benchmark']
                assert b['viewport'] == b['scene_size'] == [1280, 720] and b['quality_scale'] == 1
                assert 0 < b['bodies'][0]['triangles'] <= 100000 and b['bodies'][0]['foliage']['budget'] == 2000000
                assert native[f['profile_frame']]['outcome'] == 'rendered'
                assert f['controls']['present_delay_ms'] == report['presentation_delay_ms']
                assert all(f['gl'][k] == 0 for k in ('blocking_polls', 'server_waits', 'bulk_reads', 'finishes', 'memory_queries'))
                assert all(f['worker'][k] <= 1 for k in ('running', 'queued', 'ready'))
                if run['backend'] == 'compute':
                    p = f['publication']
                    assert p['astronaut_contact_revision'] == p['consumers'][f['selected']]['land_revision']
                    for c in p['consumers']:
                        assert c['land'] == c['grass'] == c['contacts']
                        assert c['land_revision'] == c['grass_revision'] == c['main_revision']
            a, b = moving[0], moving[-1]
            distance = b['pose']['walked_m'] - a['pose']['walked_m']
            wall = (b['observed_ns'] - a['observed_ns']) / 1e9
            animation = b['pose']['effect_s'] - a['pose']['effect_s']
            speed = distance / wall
            assert abs(speed - run['wall_mps']) < 1e-9
            assert abs(distance / animation - run['animation_mps']) < 1e-9
            assert abs(distance / animation / run['expected_mps'] - 1) < .01
            assert (abs(speed / run['expected_mps'] - 1) < .05) == run['wall_speed_pass']
            if label != 'short-preflight':
                assert run['wall_speed_pass'] == (label != 'before')
            assert all(y['pose']['walked_m'] > x['pose']['walked_m'] for x, y in zip(moving, moving[1:]))
            memory = rows(folder / 'performance.csv.memory.csv.gz')
            assert all(r['context_uuid'] == report['expected_uuid'] and r['context_status'] == 'uuid_verified' and
                       r['nvml_status'] == 'ok' and int(r['dropped_events']) == 0 for r in memory)
            checks['speed_runs'].append({'stage': label, 'case': run['case'], 'backend': run['backend'],
                                         'wall_mps': speed, 'animation_mps': distance / animation,
                                         'movement_frames': len(moving), 'excluded_frames': len(frames) - len(moving)})
            checks['movement_frames'] += len(moving)
            checks['excluded_frames'] += len(frames) - len(moving)
    spec = importlib.util.spec_from_file_location('stationary_checks', ROOT.parent / 'preflight/validate.py')
    stationary = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(stationary)
    stationary.DATA = DATA / 'stationary'
    checks['stationary'] = stationary.validate()
    software = json.loads((DATA / 'software/results.json').read_text())
    for name in ('clock-compute', 'default-cpu', 'gl33-fallback'):
        for case, expected in (('walking', 6), ('sprint', 12)):
            assert abs(software[name][case]['wall_mps'] / expected - 1) < .15
    assert software['clock-compute']['suspension_frames'] >= 3
    clock_frames = [json.loads(line) for line in read(DATA / 'software/clock-compute/frames.jsonl.gz').splitlines()]
    slow = [f for f in clock_frames if f['controls'].get('present_delay_ms') == 1250 and f['keys']['w']][1:]
    assert len(slow) == software['clock-compute']['suspension_frames']
    for a, b in zip(slow, slow[1:]):
        assert b['observed_ns'] - a['observed_ns'] > 1_000_000_000
        assert .99 <= b['pose']['effect_s'] - a['pose']['effect_s'] <= 1.001
        assert 5.8 < b['pose']['walked_m'] - a['pose']['walked_m'] < 6.1
    return checks


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--write', action='store_true')
    args = parser.parse_args()
    checks = validate()
    if args.write:
        (DATA / 'checks.json').write_text(json.dumps(checks, indent=2) + '\n')
    else:
        assert checks == json.loads((DATA / 'checks.json').read_text())
        for name, digest in json.loads((DATA / 'evidence.json').read_text())['artifacts'].items():
            assert hashlib.sha256((ROOT / name).read_bytes()).hexdigest() == digest, name
    print('Validated 12 speed-gate runs + four retained short preflights, bounded suspension regression and three fresh stationary pairs')
