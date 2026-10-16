#!/usr/bin/env python3
"""Summarize and independently check local paired grass preparation evidence."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'scripts/benchmarks/feature_costs'))
from run import stats


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    raw = json.loads(args.input.read_text())
    assert raw.get('validation') and len(raw['runs']) == raw['method']['pairs'] * 4
    grouped = {}
    manifest = {}
    for run in raw['runs']:
        folder = args.input.parent / run['raw_folder']
        for path in folder.iterdir():
            if path.is_file():
                manifest[str(path.relative_to(args.input.parent))] = digest(path)
        assert run['audit']['peak_logical_bytes'] <= 1024**3
        assert all(run['audit']['gl'][k] == 0 for k in
                   ('blocking_polls', 'bulk_reads', 'finishes', 'memory_queries', 'server_waits'))
        rows = [r for r in csv.DictReader((folder / 'frames.csv').open())
                if run['first_frame'] <= int(r['frame']) <= run['last_frame']]
        loops = [r for r in csv.DictReader((folder / 'frames.csv.native-loop.csv').open())
                 if run['first_frame'] <= int(r['frame']) <= run['last_frame']]
        assert len(rows) == len(loops) == run['frames']
        assert all(r['gpu_status'] == 'ready' and r['scene_reuses'] == '0' and r['gpu_events_dropped'] == '0' for r in rows)
        key = run['case'] + '-' + run['variant']
        group = grouped.setdefault(key, {'wall_ms': [], 'mesh_ms': [], 'gpu_render_ms': [],
            'allocation_gpu_ms': [], 'allocation_cpu_ms': [], 'metadata_fresh_gpu_ms': [],
            'metadata_repeated_generation_gpu_ms': []})
        seen = set()
        for line in (folder / 'frames.jsonl').read_text().splitlines():
            frame = json.loads(line)
            if not run['first_frame'] <= frame['profile_frame'] <= run['last_frame']:
                continue
            foliage = frame['benchmark']['bodies'][0]['foliage']
            if 'cell_updates' not in foliage:
                continue
            identity = tuple(foliage['plan_eye'])
            if identity in seen:
                continue
            seen.add(identity)
            if len(seen)==1:
                continue # The first snapshot was already live before measurement.
            updates = foliage['cell_updates']
            assert sum(updates[:3]) == foliage['patches']
            assert updates[4]+updates[5] == foliage['candidates'] and updates[6] == 0
            if updates[4]+updates[5]:
                group.setdefault('copied_root_fraction', []).append(updates[4]/(updates[4]+updates[5]))
            for name, value in zip(('retained_cells','entering_cells','dirty_cells','departing_cells','copied_roots','generated_roots'),updates):
                group.setdefault(name, []).append(value)
        if any(stage in raw['method'].get('stages', []) for stage in
               ('grass_roots', 'grass_placement', 'grass_animation', 'grass_draw', 'grass_cells')):
            for name in ('roots_gpu_ms', 'roots_cpu_ms', 'cells_gpu_ms', 'cells_cpu_ms', 'placement_main_gpu_ms',
                         'placement_reflection_gpu_ms', 'placement_main_cpu_ms',
                         'placement_reflection_cpu_ms'):
                group.setdefault(name, [])
        if any(stage in raw['method'].get('stages', []) for stage in ('grass_animation', 'grass_draw')):
            for view in ('main', 'reflection'):
                for stage in ('animation', 'draw'):
                    for clock in ('gpu', 'cpu'):
                        group.setdefault(f'{stage}_{view}_{clock}_ms', [])
        for name, column in (('opaque_gpu_ms', 'gpu_opaque_ms'), ('reflection_gpu_ms', 'gpu_reflection_ms')):
            if name in run:
                values = [float(r[column]) for r in rows]
                assert stats(values) == run[name]
                group.setdefault(name, []).extend(values)
        for name, values in [('wall_ms', [float(r['wall_ms']) for r in loops]),
                             ('mesh_ms', [float(r['cpu_mesh_ms']) for r in rows]),
                             ('gpu_render_ms', [float(r['gpu_ms']) for r in rows])]:
            assert stats(values) == run[name]
            group[name].extend(values)
        previous = None
        work = list(csv.DictReader((folder / 'frames.csv.gpu-work.csv').open()))
        # Root preparation may span several frames. Compare complete plan sums,
        # not individual dispatch events whose size changes with slicing.
        for stage in ('grass_roots', 'grass_cells'):
            plans = {}
            per_frame = {}
            for row in work:
                if row['stage'] != stage or not row['frame']:
                    continue
                identity = tuple(row[k] for k in ('attempt', 'epoch', 'request_serial', 'body', 'field', 'topology'))
                plans.setdefault(identity, []).append(row)
                if run['first_frame'] <= int(row['frame']) <= run['last_frame']:
                    assert row['status'] == 'ready'
                    values = per_frame.setdefault(int(row['frame']), [0., 0.])
                    values[0] += float(row['gpu_ms']); values[1] += float(row['cpu_submit_ms'])
            name = stage.removeprefix('grass_')
            for clock, index in (('gpu', 0), ('cpu', 1)):
                group.setdefault(f'{name}_active_frame_{clock}_ms', []).extend(v[index] for v in per_frame.values())
                column = 'gpu_ms' if clock == 'gpu' else 'cpu_submit_ms'
                complete = [events for events in plans.values() if all(
                    run['first_frame'] <= int(r['frame']) <= run['last_frame'] for r in events)]
                group.setdefault(f'{name}_plan_{clock}_ms', []).extend(
                    sum(float(r[column]) for r in events) for events in complete)
            group.setdefault(f'{name}_plan_events', []).extend(len(events) for events in complete)
        for row in work:
            if row['stage'] in ('grass_roots', 'grass_placement', 'grass_animation', 'grass_draw', 'grass_cells') and 'roots_gpu_ms' in group:
                if row['frame'] and run['first_frame'] <= int(row['frame']) <= run['last_frame']:
                    assert row['status'] == 'ready'
                    name = row['stage'].removeprefix('grass_') if row['stage'] in ('grass_roots', 'grass_cells') else row['stage'].removeprefix('grass_') + '_' + row['view']
                    group[name + '_gpu_ms'].append(float(row['gpu_ms']))
                    group[name + '_cpu_ms'].append(float(row['cpu_submit_ms']))
                continue
            if row['stage'] not in ('grass_metadata', 'grass_allocation'):
                continue
            identity = tuple(row[k] for k in ('body', 'field', 'topology', 'backend'))
            repeated = identity == previous
            if row['stage'] == 'grass_metadata':
                previous = identity
            if not row['frame'] or not run['first_frame'] <= int(row['frame']) <= run['last_frame']:
                continue
            assert row['status'] == 'ready'
            if row['stage'] == 'grass_metadata':
                name = 'metadata_repeated_generation_gpu_ms' if repeated else 'metadata_fresh_gpu_ms'
                group[name].append(float(row['gpu_ms']))
            else:
                group['allocation_gpu_ms'].append(float(row['gpu_ms']))
                group['allocation_cpu_ms'].append(float(row['cpu_submit_ms']))
    for name in ('results.json', 'input.json', 'telemetry.csv'):
        manifest[name] = digest(args.input.parent / name)
    summary = {k: v for k, v in raw.items() if k != 'runs'}
    summary['groups'] = {k: {name: stats(v) for name, v in group.items()} for k, group in grouped.items()}
    summary['pairs'] = [{k: v for k, v in r.items() if k not in ('first_workload', 'last_workload')}
                        for r in raw['runs']]
    # Keep scalar Earth endpoints; full per-body snapshots remain local.
    summary['workloads'] = []
    for run in raw['runs']:
        first, last = [run[k]['bodies'][0] for k in ('first_workload', 'last_workload')]
        entry = {k: run[k] for k in ('pair', 'case', 'variant')}
        entry['triangles'] = [first['triangles'], last['triangles']]
        entry['candidates'] = [first['foliage']['candidates'], last['foliage']['candidates']]
        entry['patches'] = [first['foliage']['patches'], last['foliage']['patches']]
        entry['gpu_bytes'] = [first['foliage']['gpu_bytes'], last['foliage']['gpu_bytes']]
        for key in ('density', 'budget', 'capacity', 'protected_m', 'sigma_m', 'near_infeasible'):
            entry[key] = [body['foliage']['policy'][key] for body in (first, last)]
        for key in ('cell_updates', 'cell_cache_bytes'):
            if key in first['foliage']:
                entry[key]=[body['foliage'][key] for body in (first,last)]
        summary['workloads'].append(entry)
    summary['raw_sha256'] = manifest
    summary['asset_sha256'] = digest(ROOT / 'data/character/aces/suit.bin')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(summary, indent=2) + '\n')
    for name, group in summary['groups'].items():
        print(name, 'wall', group['wall_ms'], 'allocation', group['allocation_gpu_ms'])


if __name__ == '__main__':
    main()
