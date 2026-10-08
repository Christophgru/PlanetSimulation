#!/usr/bin/env python3
"""Recompute matched coverage from archived pixels, live roots and native frames."""
import argparse
import csv
import gzip
import hashlib
import io
import importlib.util
import json
import lzma
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parent
DATA = ROOT / 'validation'
BANDS = ((0, 5), (5, 15), (15, 30))


def content(path):
    if not path.exists():
        path = next(p for p in (Path(str(path)+'.gz'),Path(str(path)+'.xz')) if p.exists())
    raw=path.read_bytes()
    return gzip.decompress(raw) if path.suffix == '.gz' else lzma.decompress(raw) if path.suffix == '.xz' else raw


def load(path):
    return json.loads(content(path))


def floats(path, columns=None):
    values = np.frombuffer(content(path), '<f4')
    assert np.isfinite(values).all(), path
    return values.reshape(-1, columns) if columns else values


def close(a, b):
    if isinstance(a, dict):
        assert a.keys() == b.keys()
        for k in a: close(a[k], b[k])
    elif isinstance(a, list):
        assert len(a) == len(b)
        for x, y in zip(a, b): close(x, y)
    elif isinstance(a, float):
        assert np.isclose(a, b, rtol=1e-9, atol=1e-9), (a, b)
    else:
        assert a == b, (a, b)


def queues(folder, receipt):
    result = []
    for q, name in enumerate(('detailed', 'quads')):
        records = floats(folder/(name+'.blades'), 16)
        info = receipt['queues'][q]
        assert len(records) == info['instances'] <= info['capacity']
        assert info['offset_bytes'] == q*info['capacity']*64 and info['record_bytes'] == 64
        assert np.all((records[:, 3] > 0) & (records[:, 3] <= 1))
        assert np.all(np.abs(np.linalg.norm(records[:, 4:7], axis=1)-1) < 1e-4)
        result.append(records)
    before, after = [floats(folder/name) for name in ('ground.depth', 'grass.depth')]
    w, h = receipt['viewport']
    assert len(before) == len(after) == w*h and np.all((after >= 0) & (after <= before) & (before <= 1))
    return result, (after < before).reshape(h, w)


def verify(folder):
    native = load(folder/'snapshot.json')
    if not (folder/'matched').exists():
        values, mask = queues(folder, native)
        saved = load(folder/'analysis.json')
        assert int(mask.sum()) == saved['opaque_grass_pixels']
        counts = [0, 0, 0]
        for records in values:
            distance = np.linalg.norm(records[:, :3].astype(float)*native['meters_per_radius']-native['root_m'], axis=1)
            for i, (lo, hi) in enumerate(BANDS): counts[i] += int(((distance >= lo) & (distance < hi)).sum())
        assert counts == saved['root_band_counts']
        return {'ordinary_snapshot': True, 'opaque_grass_pixels': int(mask.sum())}
    folder = folder/'matched'
    control, audit, grass, saved = [load(folder/name) for name in ('controls.json', 'receipt.json', 'grass/snapshot.json', 'analysis.json')]
    assert control['schema'] == audit['schema'] == 1
    assert not audit['timing_acceptance'] and not audit['coverage_acceptance']
    assert audit['source_native_frame'] == native['profile_frame'] == grass['profile_frame']
    assert audit['native_workload_before'] == audit['native_workload_after'] == native['workload']
    assert audit['native_publication_before'] == audit['native_publication_after'] == native['publication_after_frame']
    assert audit['cpu_preparations_skipped'] == (0 if native['backend'] == 'compute' else 4)
    for key in ('model', 'view', 'projection', 'viewport', 'meters_per_radius'): assert control[key] == grass[key]
    assert grass['root_m'] == control['astronaut']['root']
    assert grass['trail_segments_m'] == control['astronaut']['grass_trail']
    for key in ('root', 'up', 'forward', 'right', 'arm_swing', 'feet', 'body_offset_m', 'velocity_mps'):
        assert grass['astronaut'][key] == control['astronaut'][key]
    records, opaque_stage = queues(folder/'grass', grass)
    w, h = control['viewport']
    rgba = [np.frombuffer(content(folder/'images'/(n+'.rgba')), np.uint8).reshape(h, w, 4) for n in ('grass', 'bare')]
    stencil = [np.frombuffer(content(folder/'images'/(n+'.stencil')), np.uint8).reshape(h, w) for n in ('grass', 'bare')]
    assert set(np.unique(stencil[0])) <= {0, 1, 2, 3, 4, 5} and 5 not in np.unique(stencil[1])
    opaque = stencil[0] == 5
    assert np.all(~opaque | opaque_stage), 'Final stencil must originate in main grass depth'
    delta = np.maximum.reduce([np.abs(rgba[0][..., c].astype(int)-rgba[1][..., c]) for c in range(3)])
    composed = opaque & (delta >= audit['composed_rgb_threshold'])
    surface = floats(folder/'ground/eligible.rgba32f', 4).reshape(h, w, 4).astype(float)
    plane = floats(folder/'ground/plane.rgba32f', 4).reshape(h, w, 4).astype(float)
    assert np.all(surface[..., 3] >= 0) and np.all(surface[..., 3] <= plane[..., 3]+1e-5)
    valid_plane = plane[..., 3] > 0
    assert np.all(np.abs(np.linalg.norm(plane[valid_plane, :3], axis=1)-1) < 1e-4)
    eligible = (stencil[1] == 2) & (surface[..., 3] > 0)
    radius = np.sqrt(np.sum(surface[..., :3]**2, axis=2))
    scale = control['meters_per_radius']; center = np.array(control['astronaut']['root'])
    matrices = {name: np.array(control[name]).reshape(4, 4, order='F') for name in ('model', 'view', 'projection')}
    camera_body = np.linalg.inv(matrices['view'] @ matrices['model'])[:3, 3]
    root_body = (center/scale).astype(np.float32).astype(float)
    vm = matrices['view'] @ matrices['model']
    model_radius = np.linalg.norm(matrices['model'][0, :3])
    totals = np.zeros((3, 3), float) # generated, visible, visible fade.
    for batch in records:
        points = batch[:, :3].astype(float)
        distance = np.sqrt(np.sum((points*scale-center)**2, axis=1))
        world = matrices['model'] @ np.vstack((points.T, np.ones(len(points))))
        camera = matrices['view'] @ world
        projected = matrices['projection'] @ camera
        in_view = (projected[3] > 0) & np.all(np.abs(projected[:3]) <= projected[3], axis=0)
        indices = np.flatnonzero(in_view)
        normalized = projected[:2, indices]/projected[3, indices]
        x = np.clip(((normalized[0]+1)*w/2).astype(int), 0, w-1)
        y = np.clip(((normalized[1]+1)*h/2).astype(int), 0, h-1)
        ground_point = surface[y, x, :3]/scale+root_body
        normals = plane[y, x, :3]
        ray = points[indices]-camera_body
        denom = np.sum(normals*ray, axis=1)
        relative_t = np.divide(np.sum(normals*(ground_point-points[indices]), axis=1), denom,
                               out=np.full(len(indices), np.nan), where=np.abs(denom)>1e-12)
        depth = -relative_t*(-camera[2, indices])*scale/model_radius
        visible = np.zeros(len(points), bool)
        visible[indices] = eligible[y, x] & np.isfinite(relative_t) & (relative_t > -1) & (depth <= audit['root_visibility_tolerance_m'])
        for i, (lo, hi) in enumerate(BANDS):
            band = (distance >= lo) & (distance < hi)
            totals[i] += [band.sum(), (band & visible).sum(), np.sum(batch[band & visible, 3], dtype=float)]
    rows = []
    for i, (lo, hi) in enumerate(BANDS):
        selection = eligible & (radius >= lo) & (radius < hi)
        area = float(surface[selection, 3].sum()); covered = float(surface[selection & composed, 3].sum())
        row = {'band_m': [lo, hi], 'eligible_visible_m2': area, 'eligible_pixels': int(selection.sum()),
               'generated_roots': int(totals[i, 0]), 'ground_visible_roots': int(totals[i, 1]),
               'visible_fade_sum': float(totals[i, 2]),
               'visible_roots_per_eligible_m2': totals[i, 1]/area if area else None,
               'fade_per_eligible_m2': totals[i, 2]/area if area else None,
               'composed_projected_eligible_coverage': covered/area if area else None}
        close(row, saved['bands'][i]);rows.append(row)
    assert saved['nearest_opaque_grass_pixels'] == int(opaque.sum())
    assert saved['composed_grass_pixels'] == int(composed.sum())
    assert saved['character_pixels'] == int((stencil[0] == 4).sum()) > 0
    for name, mask in (('composed', composed), ('eligible', eligible)):
        assert content(folder/(name+'-mask.pgm')) == f'P5\n{w} {h}\n255\n'.encode()+(mask[::-1].astype(np.uint8)*255).tobytes()
    return {'bands': rows, 'composed_grass_pixels': int(composed.sum()), 'nearest_opaque_grass_pixels': int(opaque.sum())}


def validate():
    manifest = load(DATA/'manifest.json'); summaries = []; traces = {}
    for entry in manifest['snapshots']:
        folder = DATA/entry['path'];native = load(folder/'snapshot.json')
        if entry['frames'] not in traces:
            traces[entry['frames']] = {f['profile_frame']: f for f in map(json.loads, content(DATA/entry['frames']).splitlines())}
        frame = traces[entry['frames']][native['profile_frame']]
        assert frame['pose']['root'] == native['root_m'] and frame['pose']['walked_m'] == native['walked_m']
        assert all(frame['gl'][k] == 0 for k in ('bulk_reads', 'blocking_polls', 'server_waits', 'finishes', 'memory_queries'))
        for key in ('land_field', 'land_topology', 'land_revision'): assert frame['publication_geometry'][0][key] == native[key]
        if entry.get('hardware'):
            assert 'Quadro M1000M' in frame['renderer']
            memory = list(csv.DictReader(io.StringIO(content(DATA/entry['memory']).decode())))
            assert memory and all(r['context_uuid'] == manifest['hardware_uuid'] and r['context_status'] == 'uuid_verified' and r['nvml_status'] == 'ok' for r in memory)
        if entry.get('production'):
            assert native['viewport'] == [1280, 720] and native['workload']['quality_scale'] == 1
            foliage = native['workload']['bodies'][0]['foliage']
            assert native['workload']['bodies'][0]['triangles'] == 100000
            assert foliage['budget'] == foliage['configured_budget'] == 2000000
            assert native['walked_m'] >= entry['distance_m']
        summaries.append({'path': entry['path'], **verify(folder)})
    reports = []
    for name in manifest['production_groups']:
        report = load(DATA/name/'results.json')
        assert report['declared_relative_tolerance'] == .05
        controls = []
        for pair in report['pairs']:
            roots = []
            for backend in ('cpu', 'compute'):
                folder = DATA/name/f'pair-{pair["pair"]}'/backend/'coverage/1/matched'
                roots.append(load(folder/'analysis.json'));controls.append(load(folder/'controls.json'))
            assert controls[-1] == controls[0] == controls[-2]
            samples = all(b['eligible_pixels'] >= 100 and b['ground_visible_roots'] >= 100 for result in roots for b in result['bands'])
            good = samples
            for a, b, claimed in zip(roots[0]['bands'], roots[1]['bands'], pair['bands']):
                ratio = {k: b[k]/a[k] if a[k] and b[k] is not None else None for k in claimed['compute_over_cpu']}
                close(ratio, claimed['compute_over_cpu'])
                passed = all(v is not None and abs(v-1) <= .05 for v in ratio.values())
                assert claimed['within_declared_tolerance'] == passed; good &= passed
            assert pair['sample_minimums_met'] == samples and pair['coverage_parity'] == good
        reports.append({'group': name, 'pairs': len(report['pairs']), 'passing_pairs': sum(p['coverage_parity'] for p in report['pairs'])})
    before, after = [load(DATA/name) for name in ('before-provenance.json', 'provenance.json')]
    assert before['source_sha256'] == after['source_sha256'] and before['driver_fixtures'] == after['driver_fixtures']
    assert len(before['binaries']) == len(after['binaries']) == 43
    assert all(after['binaries'][k] == v for k, v in before['binaries'].items() if not k.endswith('/terrain_coverage_probe'))
    spec=importlib.util.spec_from_file_location('stationary_checks',ROOT.parent.parent/'preflight/validate.py')
    stationary=importlib.util.module_from_spec(spec);spec.loader.exec_module(stationary)
    stationary.DATA=DATA/'stationary-repeat';stationary_checks=stationary.validate()
    repeat=load(DATA/'stationary-repeat/results.json');p95={}
    for run in repeat['runs']:
        folder=DATA/'stationary-repeat'/f'pair-{run["pair"]}'/run['backend']
        frames=[json.loads(line) for line in content(folder/'frames.jsonl').splitlines()]
        measured={f['profile_frame'] for f in frames if f['controls'].get('phase')=='measured'}
        wall=sorted(float(r['wall_ms']) for r in csv.DictReader(io.StringIO(content(folder/'performance.csv.native-loop.csv').decode())) if int(r['frame']) in measured)
        value=wall[int(np.ceil(.95*len(wall)))-1];close(value,run['stats']['wall_ms']['p95'])
        p95[(run['pair'],run['backend'])]=value
    ratios=[]
    for pair in repeat['pairs']:
        ratio=p95[(pair['pair'],'compute')]/p95[(pair['pair'],'cpu')]
        close(ratio,pair['wall_p95_ratio']);ratios.append(ratio)
    for path,digest in manifest['inspection_inputs_sha256'].items():
        assert hashlib.sha256((DATA/path).read_bytes()).hexdigest()==digest
    storage=load(DATA/'storage.json')
    for entry in storage['files']:
        decoded=content(DATA/entry['path'])
        assert len(decoded)==entry['decoded_bytes'] and hashlib.sha256(decoded).hexdigest()==entry['decoded_sha256']
    return {'snapshots': len(summaries), 'matched_snapshots': sum('bands' in s for s in summaries),
            'groups': reports, 'raw_checks': summaries, 'unchanged_prior_executables': 42, 'timing_acceptance': False,
            'stationary_repeat':stationary_checks,'stationary_p95_ratios':ratios}


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__);p.add_argument('--write', action='store_true');args = p.parse_args()
    checks = validate()
    if args.write: (DATA/'checks.json').write_text(json.dumps(checks, indent=2)+'\n')
    else:
        close(checks, load(DATA/'checks.json'))
        evidence = load(DATA/'evidence.json')
        for name, digest in evidence['artifacts'].items(): assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest() == digest, name
    print(f'Validated {checks["snapshots"]} native snapshots, {checks["matched_snapshots"]} controlled comparisons and all raw frame/area/root/compositing joins')
