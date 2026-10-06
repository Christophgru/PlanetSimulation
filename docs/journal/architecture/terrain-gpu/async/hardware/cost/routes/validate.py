#!/usr/bin/env python3
"""Independently recompute archived native distance-cohort and landmark checks."""
import argparse
from collections import Counter
import csv
import gzip
import hashlib
import io
import json
import math
from pathlib import Path
import statistics

ROOT = Path(__file__).resolve().parent
DATA = ROOT / 'validation'


def read(path):
    return gzip.decompress(path.read_bytes()).decode() if path.suffix == '.gz' else path.read_text()


def rows(path):
    return list(csv.DictReader(io.StringIO(read(path))))


def validate():
    report = json.loads((DATA / 'results.json').read_text())
    provenance = json.loads((DATA / 'provenance.json').read_text())
    assert len(report['runs']) == 12 and len(report['pairs']) == 6
    assert report['method']['route_distance_m'] == 400
    frozen = report['frozen_provenance']
    assert frozen['source_sha256'] == provenance['source_sha256']
    assert frozen['build_test_sha256'] == provenance['build_test_input_sha256']
    assert frozen['application_sha256'] == provenance['binaries']['build-resume/PlanetSimulation']
    assert frozen['probe_sha256'] == provenance['binaries']['build-resume/tests/terrain_native_probe']
    assert hashlib.sha256((DATA / 'production-input.json').read_bytes()).hexdigest() == report['replay_sha256']
    checks = {'runs': [], 'pairs': [], 'measured_frames': 0, 'excluded_frames': 0, 'retained_upper_outliers': 0,
              'publication_rows': 0, 'gpu_work_rows': 0}
    runs = {}
    for run in report['runs']:
        folder = DATA / run['case'] / f"pair-{run['pair']}" / run['backend']
        frames = [json.loads(line) for line in read(folder / 'frames.jsonl.gz').splitlines()]
        observed = {f['profile_frame']: f for f in frames}
        native = {int(r['frame']): r for r in rows(folder / 'performance.csv.native-loop.csv.gz')}
        perf = {int(r['frame']): r for r in rows(folder / 'performance.csv.gz')}
        observer = {int(r['frame']): r for r in rows(folder / 'frames.jsonl.observer.csv.gz')}
        assert len(observed) == len(frames) and observed.keys() == native.keys() == perf.keys() == observer.keys()
        sprint = run['case'] == 'sprint'
        moving = [f for f in frames if f['controls'].get('phase') == 'route' and f['keys']['w'] and f['keys']['shift'] == sprint]
        assert moving[-1]['pose']['walked_m'] >= 400
        assert all(b['profile_frame'] == a['profile_frame'] + 1 and b['pose']['walked_m'] > a['pose']['walked_m']
                   for a, b in zip(moving, moving[1:]))
        measured = [f for f in moving if 5 <= f['pose']['walked_m'] < 400]
        assert len(measured) == run['measured_frames'] and len(measured) >= 240
        assert len(frames) - len(measured) == run['excluded_frames']
        for f in measured:
            n = f['profile_frame']
            assert f['paused'] and f['selected'] == 0 and f['mode'] == 3 and not f['pose']['airborne']
            assert f['benchmark']['viewport'] == f['benchmark']['scene_size'] == [1280, 720]
            assert f['benchmark']['quality_scale'] == 1 and native[n]['outcome'] == 'rendered'
            assert abs(float(perf[n]['simulation_s'])) < 1e-9
            assert f['benchmark']['bodies'][0]['triangles'] == 100000
            foliage = f['benchmark']['bodies'][0]['foliage']
            assert foliage['budget'] == foliage['configured_budget'] == 2000000
            assert all(f['gl'][key] == 0 for key in ('blocking_polls', 'server_waits', 'bulk_reads', 'finishes', 'memory_queries'))
            assert all(f['worker'][key] <= 1 for key in ('running', 'queued', 'ready'))
            if run['backend'] == 'compute':
                p = f['publication']
                assert p['astronaut_contact_revision'] == p['consumers'][0]['land_revision']
                for c in p['consumers']:
                    assert c['land'] == c['grass'] == c['contacts']
                    assert c['land_revision'] == c['grass_revision'] == c['main_revision']
        start, end = measured[0], measured[-1]
        distance = end['pose']['walked_m'] - start['pose']['walked_m']
        wall_s = (end['observed_ns'] - start['observed_ns']) / 1e9
        assert abs(distance / wall_s - run['wall_mps']) < 1e-9
        assert abs(run['wall_mps'] / (12 if sprint else 6) - 1) < .05
        walls = sorted(float(native[f['profile_frame']]['wall_ms']) for f in measured)
        assert abs(statistics.mean(walls) - run['stats']['wall_ms']['mean']) < 1e-9
        assert walls[math.ceil(.95 * len(walls)) - 1] == run['stats']['wall_ms']['p95']
        q1, _, q3 = statistics.quantiles(walls, n=4, method='inclusive')
        outliers = sum(w > q3 + 1.5 * (q3 - q1) for w in walls)
        assert outliers == run['upper_outliers_retained']
        assert run['mesh_upload_frames'] == sum(int(perf[f['profile_frame']]['mesh_uploads']) > 0 for f in measured)
        assert run['foliage_rebuilds'] == sum(int(perf[f['profile_frame']]['foliage_rebuilds']) for f in measured)
        assert dict(Counter(perf[f['profile_frame']]['gpu_status'] for f in measured)) == run['gpu_status_counts']
        density = [f['benchmark']['bodies'][0]['foliage']['density'] for f in measured]
        anchor_distances = [math.dist([x / 1000 for x in f['pose']['root']],
                                     f['benchmark']['bodies'][0]['foliage']['plan_eye']) * 1000
                            for f in measured if f['benchmark']['bodies'][0]['foliage']['plan_eye']]
        for values, recorded in ((density, run['effective_density']),
                                 (anchor_distances, run['root_to_grass_plan_eye_m']),
                                 ([float(perf[f['profile_frame']]['gpu_frame_span_ms']) for f in measured
                                   if perf[f['profile_frame']]['gpu_frame_span_ms']], run['stats']['gpu_frame_span_ms'])):
            assert len(values) == recorded['samples']
            assert min(values) == recorded['min'] and max(values) == recorded['max']
            assert abs(statistics.mean(values) - recorded['mean']) < 1e-9
            assert sorted(values)[math.ceil(.95 * len(values)) - 1] == recorded['p95']
        for distance_bin in run['distance_bins']:
            values = sorted(float(native[f['profile_frame']]['wall_ms']) for f in measured
                            if distance_bin['start_m'] <= f['pose']['walked_m'] < distance_bin['end_m'])
            assert len(values) == distance_bin['wall_ms']['samples']
            assert values[math.ceil(.95 * len(values)) - 1] == distance_bin['wall_ms']['p95']
        landmarks = []
        for mark in run['landmarks']:
            a, b = [observed[n] for n in mark['bracket_frames']]
            target = mark['distance_m']
            assert a['pose']['walked_m'] <= target <= b['pose']['walked_m']
            t = (target - a['pose']['walked_m']) / (b['pose']['walked_m'] - a['pose']['walked_m'])
            assert abs(t - mark['fraction']) < 1e-12
            root = [x + t * (y - x) for x, y in zip(a['pose']['root'], b['pose']['root'])]
            assert root == mark['root_m']
            camera = [x + t * (y - x) for x, y in zip(a['camera_local'], b['camera_local'])]
            assert camera == mark['camera_local']
            nearest = a if t < .5 else b
            assert nearest['profile_frame'] == mark['nearest_frame']
            assert nearest['benchmark'] == mark['workload'] and nearest['publication_geometry'] == mark['geometry']
            landmarks.append(mark)
        assert [m['distance_m'] for m in landmarks] == list(range(25, 401, 25))
        memory = rows(folder / 'performance.csv.memory.csv.gz')
        assert all(r['context_uuid'] == report['expected_uuid'] and r['context_status'] == 'uuid_verified' and
                   r['nvml_status'] == 'ok' and int(r['dropped_events']) == 0 for r in memory)
        physical = [int(r['used_bytes']) for r in memory if r['kind'] == 'sample' and
                    start['observed_ns'] <= int(r['query_end_ns']) <= end['observed_ns']]
        assert max(physical) == run['memory']['sampled_device_used_peak_bytes']
        assert len(physical) == run['memory']['physical_samples']
        logical = [int(r['overlap_reserved_bytes']) for r in memory if r['overlap_reserved_bytes']]
        assert max(logical, default=None) == run['memory']['logical_overlap_peak_bytes']
        if run['backend'] == 'cpu':
            assert run['memory']['logical_overlap_peak_bytes'] is None
        publications = rows(folder / 'performance.csv.publications.csv.gz')
        work = rows(folder / 'performance.csv.gpu-work.csv.gz')
        assert dict(Counter(r['outcome'] for r in publications)) == run['publication_outcomes_all_frames']
        assert dict(Counter(r['status'] for r in work)) == run['gpu_work_statuses_all_frames']
        checks['runs'].append({'case': run['case'], 'pair': run['pair'], 'backend': run['backend'],
                               'wall_mps': run['wall_mps'], 'landmarks': len(landmarks), 'measured_frames': len(measured),
                               'excluded_frames': len(frames) - len(measured), 'outliers_retained': outliers,
                               'publication_rows': len(publications), 'gpu_work_rows': len(work)})
        checks['measured_frames'] += len(measured)
        checks['excluded_frames'] += len(frames) - len(measured)
        checks['retained_upper_outliers'] += outliers
        checks['publication_rows'] += len(publications)
        checks['gpu_work_rows'] += len(work)
        runs[(run['case'], run['pair'], run['backend'])] = run
    for pair in report['pairs']:
        cpu, gpu = [runs[(pair['case'], pair['pair'], b)] for b in ('cpu', 'compute')]
        assert pair['wall_p95_ratio'] == gpu['stats']['wall_ms']['p95'] / cpu['stats']['wall_ms']['p95']
        roots = [math.dist(a['root_m'], b['root_m']) for a, b in zip(cpu['landmarks'], gpu['landmarks'])]
        assert max(roots) == pair['max_landmark_root_difference_m']
        cameras = [math.dist(a['camera_local'], b['camera_local']) * 1000
                   for a, b in zip(cpu['landmarks'], gpu['landmarks'])]
        assert max(cameras) == pair['max_landmark_camera_difference_m']
        density_ratios = []
        for a, b in zip(cpu['landmarks'], gpu['landmarks']):
            assert a['workload']['viewport'] == b['workload']['viewport'] == [1280, 720]
            assert a['workload']['quality_scale'] == b['workload']['quality_scale'] == 1
            assert len(a['workload']['bodies']) == len(b['workload']['bodies'])
            for x, y in zip(a['workload']['bodies'], b['workload']['bodies']):
                assert x['body'] == y['body']
                for key in ('configured_density', 'configured_budget', 'configured_distance_m',
                            'budget', 'distance_m', 'compute_placement', 'enabled'):
                    assert x['foliage'][key] == y['foliage'][key]
                if x['foliage']['enabled']:
                    assert x['foliage']['density'] > 0
                    density_ratios.append(y['foliage']['density'] / x['foliage']['density'])
        assert [min(density_ratios), max(density_ratios)] == pair['effective_density_ratio_range']
        for a, b, ratio in zip(cpu['distance_bins'], gpu['distance_bins'], pair['distance_bin_ratios']):
            assert a['start_m'] == b['start_m'] == ratio['start_m']
            assert a['end_m'] == b['end_m'] == ratio['end_m']
            assert a['wall_ms']['samples'] == ratio['cpu_frames']
            assert b['wall_ms']['samples'] == ratio['compute_frames']
            assert b['wall_ms']['p95'] / a['wall_ms']['p95'] == ratio['wall_p95_ratio']
        assert pair['fixed_quality_and_budget_matched'] and not pair['differences']
        checks['pairs'].append({'case': pair['case'], 'pair': pair['pair'], 'max_root_difference_m': max(roots),
                                'native_p95_ratio': pair['wall_p95_ratio'], 'coverage_verified': False})
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
    print(f"Validated {checks['measured_frames']} measured + {checks['excluded_frames']} excluded frames across twelve native routes; coverage remains pending")
