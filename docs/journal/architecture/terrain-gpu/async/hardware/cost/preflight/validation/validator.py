#!/usr/bin/env python3
"""Independently recheck every archived production stationary receipt."""
import argparse
import csv
import gzip
import hashlib
import io
import json
from pathlib import Path
import statistics

ROOT = Path(__file__).resolve().parent
DATA = ROOT / 'validation'


def read(path):
    return gzip.decompress(path.read_bytes()).decode() if path.suffix == '.gz' else path.read_text()


def csv_rows(path):
    return list(csv.DictReader(io.StringIO(read(path))))


def geometry(frame):
    return [{key: g[key] for key in ['body', 'land_field', 'land_topology', 'water_field', 'water_topology']}
            for g in frame['publication_geometry']]


def validate():
    report = json.loads((DATA / 'results.json').read_text())
    provenance = json.loads((DATA / 'provenance.json').read_text())
    assert report['frozen_provenance']['source_sha256'] == provenance['source_sha256']
    assert report['frozen_provenance']['build_test_sha256'] == provenance['build_test_input_sha256']
    checks = {'runs': [], 'pairs': [], 'measured_frames': 0, 'excluded_frames': 0,
              'gpu_work_rows': 0, 'publication_rows': 0}
    landmarks = {}
    for run in report['runs']:
        folder = DATA / f"pair-{run['pair']}" / run['backend']
        frames = [json.loads(row) for row in read(folder / 'frames.jsonl.gz').splitlines()]
        observed = {f['profile_frame']: f for f in frames}
        measured = {n: f for n, f in observed.items() if f['controls'].get('phase') == 'measured'}
        perf = {int(r['frame']): r for r in csv_rows(folder / 'performance.csv.gz')}
        native = {int(r['frame']): r for r in csv_rows(folder / 'performance.csv.native-loop.csv.gz')}
        observers = {int(r['frame']): r for r in csv_rows(folder / 'frames.jsonl.observer.csv.gz')}
        assert len(observed) == len(frames) and observed.keys() == perf.keys() == native.keys() == observers.keys()
        assert len(measured) == run['measured_frames'] == 240
        assert len(frames) - len(measured) == run['excluded_frames']
        assert max(measured) - min(measured) + 1 == len(measured)
        first = measured[min(measured)]
        assert first['benchmark'] == run['workload_first'] == run['workload_last']
        for n, f in measured.items():
            assert f['paused'] and f['selected'] == 0 and f['mode'] == 3 and not f['pose']['airborne']
            assert not any(f['keys'].values()) and native[n]['outcome'] == 'rendered'
            assert f['benchmark'] == first['benchmark'] and geometry(f) == geometry(first)
            assert abs(float(perf[n]['simulation_s'])) < 1e-9 and perf[n]['gpu_status'] == 'ready'
            assert float(native[n]['wall_ms']) >= float(perf[n]['frame_ms'])
            assert all(f['worker'][key] <= 1 for key in ['running', 'queued', 'ready'])
            assert all(f['gl'][key] == 0 for key in ['blocking_polls', 'server_waits', 'bulk_reads', 'finishes', 'memory_queries'])
        b = first['benchmark']
        assert b['viewport'] == b['scene_size'] == [1280, 720] and b['quality_scale'] == 1
        earth, moon = b['bodies']
        assert earth['triangles'] == 100000 and moon['triangles'] == 960
        assert earth['unique_samples'] == 50002 and earth['foliage']['budget'] == 2000000
        assert earth['foliage']['candidates'] == 1999246 and earth['foliage']['patches'] == 27496
        assert abs(earth['foliage']['density'] - 47.1492289127359) < 1e-9
        assert earth['cpu_render_bytes'] == (0 if run['backend'] == 'compute' else 12000000)
        memory = csv_rows(folder / 'performance.csv.memory.csv.gz')
        assert all(r['context_uuid'] == report['expected_uuid'] and r['nvml_status'] == 'ok' and
                   r['context_status'] == 'uuid_verified' and r['stale'] == '0' and
                   int(r['dropped_events']) == 0 for r in memory)
        work = csv_rows(folder / 'performance.csv.gpu-work.csv.gz')
        publications = csv_rows(folder / 'performance.csv.publications.csv.gz')
        assert all(r['status'] == 'ready' for r in work)
        assert len(publications) == 2 and all(r['outcome'] == 'published' for r in publications)
        assert all(int(r['end_frame']) in native for r in publications)
        # Retain every outlier. Tukey's upper fence is a descriptive label,
        # never a reason to trim frame samples or improve the reported p95.
        wall = [float(native[n]['wall_ms']) for n in measured]
        q1, _, q3 = statistics.quantiles(wall, n=4, method='inclusive')
        fence = q3 + 1.5 * (q3 - q1)
        checks['runs'].append({'pair': run['pair'], 'backend': run['backend'],
                               'upper_outlier_fence_ms': fence, 'upper_outliers_retained': sum(x > fence for x in wall),
                               'physical_rows': len(memory), 'gpu_work_rows': len(work),
                               'publication_rows': len(publications), 'geometry': geometry(first)})
        landmarks[(run['pair'], run['backend'])] = first
        checks['measured_frames'] += len(measured)
        checks['excluded_frames'] += len(frames) - len(measured)
        checks['gpu_work_rows'] += len(work)
        checks['publication_rows'] += len(publications)
    for pair in range(1, 4):
        cpu, gpu = landmarks[(pair, 'cpu')], landmarks[(pair, 'compute')]
        assert geometry(cpu) == geometry(gpu)
        assert cpu['camera_local'] == gpu['camera_local'] and cpu['pose']['root'] == gpu['pose']['root']
        for body in range(2):
            a, b = cpu['benchmark']['bodies'][body], gpu['benchmark']['bodies'][body]
            assert a['terrain_plan_eye'] == b['terrain_plan_eye']
            if a['foliage']['enabled']:
                assert a['foliage']['plan_eye'] == b['foliage']['plan_eye']
        checks['pairs'].append({'pair': pair, 'field_topology_keys_equal': True,
                                'exact_camera_root_and_planning_anchors': True})
    assert len(checks['runs']) == 6 and checks['measured_frames'] == 1440
    return checks


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--write', action='store_true')
    args = p.parse_args()
    checks = validate()
    if args.write:
        (DATA / 'checks.json').write_text(json.dumps(checks, indent=2) + '\n')
    else:
        assert checks == json.loads((DATA / 'checks.json').read_text())
        evidence = json.loads((DATA / 'evidence.json').read_text())
        for name, sha in evidence['artifacts'].items():
            assert hashlib.sha256((ROOT / name).read_bytes()).hexdigest() == sha, name
    print(f"Validated {checks['measured_frames']} measured + {checks['excluded_frames']} excluded frames; exact paired geometry/pose/anchors, full GPU work and memory identity")
