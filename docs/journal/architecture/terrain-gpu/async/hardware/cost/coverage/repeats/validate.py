#!/usr/bin/env python3
"""Reconstruct fresh route gates and crossing receipts from frozen raw evidence."""
import csv
import gzip
import hashlib
import importlib.util
import json
import lzma
from pathlib import Path
import sys
from unittest.mock import patch
import numpy as np
sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
DATA = HERE/'validation'


def read(path): return json.loads(path.read_text())
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec); spec.loader.exec_module(value)
    return value


def queues(live, folder):
    receipt = read(folder/'snapshot.json')
    assert receipt['schema'] == 1 and receipt['inspection']['blocking']
    assert not receipt['inspection']['timing_acceptance']
    for i, name in enumerate(('detailed', 'quads')):
        blades = live.array(folder/(name+'.blades'), (-1, 16))
        info = receipt['queues'][i]
        assert len(blades) == info['instances'] <= info['capacity']
        assert info['offset_bytes'] == i*info['capacity']*64
        assert info['record_bytes'] == 64 and np.isfinite(blades).all()
        assert np.all((blades[:, 3] > 0) & (blades[:, 3] <= 1))
        assert np.all(np.abs(np.linalg.norm(blades[:, 4:7].astype(float), axis=1)-1) < 1e-4)
    w, h = receipt['viewport']
    ground, grass = [live.array(folder/name, (h, w)) for name in ('ground.depth', 'grass.depth')]
    assert np.isfinite(ground).all() and np.isfinite(grass).all()
    assert np.all((grass >= 0) & (grass <= ground) & (ground <= 1))


def root_sets(live, folder, common):
    root = np.asarray(common['astronaut']['root'])
    scale = common['meters_per_radius']
    result = {'exact': [set(), set(), set()], 'rounded_mm': [set(), set(), set()]}
    for name in ('detailed', 'quads'):
        blades = live.array(folder/'grass'/(name+'.blades'), (-1, 16))
        xyz = blades[:, :3]
        distance = np.linalg.norm(xyz.astype(float)*scale-root, axis=1)
        # Exact float root identity, ignoring camera-dependent fade/attributes.
        for i, (lo, hi) in enumerate(live.BANDS):
            selected = xyz[(distance >= lo) & (distance < hi)]
            result['exact'][i].update(row.tobytes() for row in selected)
            rounded = np.rint(selected.astype(float)*scale*1000).astype('<i8')
            result['rounded_mm'][i].update(row.tobytes() for row in rounded)
    return result


def main():
    evidence = read(HERE/'evidence.json')
    for name, expected in evidence['artifact_sha256'].items():
        assert sha(HERE/name) == expected, name
    before, after = [read(DATA/(name+'provenance.json')) for name in ('before-', '')]
    assert before['source_sha256'] == after['source_sha256']
    assert before['driver_fixtures'] == after['driver_fixtures']
    changed = [p for p, h in before['binaries'].items() if after['binaries'].get(p) != h]
    assert changed == ['build-resume/tests/terrain_coverage_probe'], changed
    assert len(after['binaries']) == 44
    coverage = DATA/'inputs/native/coverage'
    live = module('frozen_live', coverage/'live/measure.py')
    session = module('frozen_session', coverage.parent/'NativeSession.py')
    crossing = module('frozen_crossing', DATA/'inputs/driver/crossing.py')
    # Only clip provenance changed. All qualified raster/area algorithms retain
    # their exact b2c2 bytes, rather than merely sharing filenames.
    old = HERE.parent/'live/validation/inputs/native/coverage'
    for p in coverage.rglob('*'):
        if p.is_file() and p.relative_to(coverage).as_posix() != 'matched/Render.h':
            assert sha(p) == sha(old/p.relative_to(coverage)), p
    manifest = read(DATA/'manifest.json')
    preflight_count = 0
    for name in manifest['preflight']:
        base = DATA/'preflight'/name
        for entry in read(base/'storage.json')['files']:
            p = base/entry['path']; encoded = p.read_bytes()
            raw = lzma.decompress(encoded) if p.suffix == '.xz' else gzip.decompress(encoded)
            assert len(encoded) == entry['encoded_bytes'] and len(raw) == entry['decoded_bytes']
            assert hashlib.sha256(raw).hexdigest() == entry['decoded_sha256']
        for p in base.rglob('snapshot.json'):
            queues(live, p.parent); preflight_count += 1
    qualification_count = 0
    qualification_change = 0.
    for kind in manifest['qualification']:
        base = DATA/'qualification'/kind
        for entry in read(base/'storage.json')['files']:
            p = base/entry['path']; encoded = p.read_bytes()
            raw = lzma.decompress(encoded) if p.suffix == '.xz' else gzip.decompress(encoded)
            assert len(encoded) == entry['encoded_bytes'] and len(raw) == entry['decoded_bytes']
            assert hashlib.sha256(raw).hexdigest() == entry['decoded_sha256']
        for p in sorted(base.rglob('live-analysis.json')):
            matched = p.parent; expected = read(p)
            assert live.analyze(matched) == expected and expected['qualified_area']
            qualification_change = max(qualification_change, max(abs(v) for v in expected['two_to_four_relative_change']))
            queues(live, matched.parent); queues(live, matched/'grass')
            run = matched.parent.parents[1]
            frames = [json.loads(l) for l in (run/'frames.jsonl').read_text().splitlines()]
            session.validate(frames, managed=frames[0]['backend'] == 'compute')
            if kind == 'quadro':
                rows = list(csv.DictReader((run/'performance.csv.memory.csv').open()))
                assert rows and all(r['context_uuid'] == manifest['expected_uuid'] and
                    r['context_status'] == 'uuid_verified' and r['nvml_status'] == 'ok' for r in rows)
            else:
                assert all('llvmpipe' in f['renderer'] for f in frames)
            qualification_count += 1
        print('Requalified preserved projection:', kind, flush=True)
    assert qualification_count == 16
    count = passed = 0; max_change = max_edge = max_overshoot = max_bracket = 0.
    findings = []
    for case in manifest['cases']:
        base = DATA/'final'/case; report = read(base/'results.json')
        assert len(report['pairs']) == 3 and len(report['runs']) == 6
        assert report['frozen_provenance']['source_sha256'] == after['source_sha256']
        assert report['frozen_provenance']['build_test_sha256'] == after['build_test_input_sha256']
        assert report['frozen_provenance']['inspection_probe_sha256'] == after['binaries']['build-resume/tests/terrain_coverage_probe']
        for name, expected in report['method_sha256'].items():
            relative = Path(name).relative_to('tests/app/terrain/native/coverage')
            assert sha(coverage/relative) == expected
        assert report['declared_relative_tolerance'] == .05
        assert report['declared_area_convergence_tolerance'] == .01
        cached = {}; native = {}; roots = {}; common = None
        assert [(r['pair'], r['backend']) for r in report['runs']] == [
            (1, 'cpu'), (1, 'compute'), (2, 'compute'), (2, 'cpu'), (3, 'cpu'), (3, 'compute')]
        for run in report['runs']:
            folder = base/run['path']; matched = folder/'coverage/1/matched'
            for entry in read(folder/'coverage/storage.json')['files']:
                p = folder/entry['path']; encoded = p.read_bytes()
                raw = lzma.decompress(encoded) if p.suffix == '.xz' else gzip.decompress(encoded)
                assert len(encoded) == entry['encoded_bytes'] and len(raw) == entry['decoded_bytes']
                assert hashlib.sha256(raw).hexdigest() == entry['decoded_sha256'], p
            frames = [json.loads(l) for l in (folder/'frames.jsonl').read_text().splitlines()]
            assert session.validate(frames, managed=run['backend'] == 'compute') == run['audit']
            assert len(frames) == run['all_native_frames_excluded_from_cost_acceptance']
            snapshot = read(folder/'coverage/1/snapshot.json')
            assert crossing.bracket(frames, snapshot, report['target_distance_m']) == run['crossing']
            assert 'Native audit:' in (folder/'run.log').read_text()
            rows = list(csv.DictReader((folder/'performance.csv.memory.csv').open()))
            assert rows and all(r['context_uuid'] == manifest['expected_uuid'] and
                r['context_status'] == 'uuid_verified' and r['nvml_status'] == 'ok' for r in rows)
            body = snapshot['workload']['bodies'][0]
            assert snapshot['viewport'] == [1280, 720] and snapshot['workload']['quality_scale'] == 1
            assert body['triangles'] == 100000
            assert body['foliage']['budget'] == body['foliage']['configured_budget'] == 2000000
            queues(live, folder/'coverage/1'); queues(live, matched/'grass')
            controls = read(matched/'controls.json')
            if common is None: common = controls
            assert controls == common
            actual = live.analyze(matched)
            assert actual == run['analysis'] == read(matched/'live-analysis.json')
            cached[matched] = actual; native[run['pair'], run['backend']] = snapshot
            roots[run['pair'], run['backend']] = root_sets(live, matched, common)
            count += 1
            max_change = max(max_change, max(abs(v) for v in actual['two_to_four_relative_change']))
            max_edge = max(max_edge, max(b['composed_boundary_fraction'] for b in actual['bands']))
            max_overshoot = max(max_overshoot, run['crossing']['overshoot_m'])
            max_bracket = max(max_bracket, run['crossing']['interval_width_m'])
            print('Reconstructed', case, run['pair'], run['backend'], flush=True)
        for pair in report['pairs']:
            number = pair['pair']
            paths = [base/f'pair-{number}'/b/'coverage/1/matched' for b in ('cpu', 'compute')]
            # Measurements above were freshly reconstructed; avoid repeating the
            # expensive integrals solely to invoke the unchanged pair formula.
            with patch.object(live, 'analyze', side_effect=lambda p: cached[Path(p)]):
                comparison = live.compare(*paths, .05)
            samples = all(b['eligible_pixels'] >= 100 and b['ground_visible_roots'] >= 100
                          for p in paths for b in cached[p]['bands'])
            parity = comparison['coverage_parity'] and comparison['qualified_area'] and samples
            assert pair['coverage_parity'] == parity and pair['sample_minimums_met'] == samples
            for key in ('bands', 'qualified_area', 'relative_tolerance', 'coverage_acceptance', 'timing_acceptance'):
                assert pair[key] == comparison[key], key
            passed += int(parity)
            a, b = [native[number, backend] for backend in ('cpu', 'compute')]
            equal = a['land_topology'] == b['land_topology']
            assert pair['native_topology_equal'] == equal
            assert pair['native_root_separation_m'] == float(np.linalg.norm(np.array(a['root_m'])-b['root_m'])) or abs(
                pair['native_root_separation_m']-float(np.linalg.norm(np.array(a['root_m'])-b['root_m']))) < 1e-12
            overlaps = {kind: [{'cpu_unique_roots': len(x), 'compute_unique_roots': len(y),
                               'shared_roots': len(x & y)}
                              for x, y in zip(roots[number, 'cpu'][kind], roots[number, 'compute'][kind])]
                        for kind in ('exact', 'rounded_mm')}
            findings.append({'case': case, 'pair': number, 'coverage_parity': parity,
                             'topology_equal': equal, 'native_root_separation_m': pair['native_root_separation_m'],
                             'root_identity_bands': overlaps, 'bands': comparison['bands']})
    assert count == 24 and len(findings) == 12
    result = {'qualification_snapshots': qualification_count,
              'qualification_maximum_area_change': qualification_change,
              'preflight_queue_depth_snapshots': preflight_count,
              'live_snapshots': count, 'live_grids': count*3, 'biome_cases': count*9,
              'queue_depth_snapshots': count*2, 'passing_pairs': passed, 'total_pairs': len(findings),
              'maximum_area_change': max_change, 'maximum_native_mask_edge_fraction': max_edge,
              'maximum_crossing_overshoot_m': max_overshoot, 'maximum_crossing_bracket_m': max_bracket,
              'pairs': findings, 'timing_acceptance': False, 'migration_acceptance': False}
    (HERE/'validation-result.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps({k: v for k, v in result.items() if k != 'pairs'}, indent=2))


if __name__ == '__main__': main()
