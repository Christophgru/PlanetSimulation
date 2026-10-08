#!/usr/bin/env python3
"""Recompute archived GPU plane areas and declared known-geometry gates."""
import hashlib
import importlib.util
import json
import lzma
from pathlib import Path
import sys
import numpy as np

ROOT = Path(__file__).resolve().parent
DATA = ROOT/'validation'
BANDS = ((0, 5), (5, 15), (15, 30))


def load(path):
    return json.loads(path.read_text())


def close(actual, expected):
    assert np.allclose(actual, expected, rtol=1e-9, atol=1e-10), (actual, expected)


def decode(path):
    return lzma.decompress(path.with_suffix(path.suffix+'.xz').read_bytes())


def independently_measure(scene, receipt, ground, plane):
    """Intersect original pixel rays with the recorded plane in physical metres."""
    w, h = receipt['viewport']
    model, view, projection = [np.asarray(scene[k]).reshape(4, 4, order='F') for k in ('model', 'view', 'projection')]
    transform = view@model
    scale = scene['meters_per_radius']
    rotation = transform[:3, :3]
    radius = np.linalg.norm(model[:3, 0])
    root = np.asarray(scene['root_body'], np.float32).astype(float)
    origin = rotation@root+transform[:3, 3]
    point = ground[..., :3].astype(float)@rotation.T/scale+origin
    normal = plane[..., :3].astype(float)@rotation.T
    lengths = np.linalg.norm(normal, axis=-1)
    normal /= np.where(lengths > 0, lengths, 1)[..., None]
    horizontal = (2*(np.arange(w)+.5)/w-1)/projection[0, 0]
    vertical = (2*(np.arange(h)+.5)/h-1)/projection[1, 1]
    rays = np.empty((h, w, 3));rays[..., 0] = horizontal;rays[..., 1] = vertical[:, None];rays[..., 2] = -1
    product = np.einsum('ijk,ijk->ij', rays, normal)
    valid = (plane[..., 3] > 0) & (np.abs(product) > 1e-12)
    depth = np.zeros((h, w));depth[valid] = np.einsum('ijk,ijk->ij', point, normal)[valid]/product[valid]
    valid &= depth > 0
    square_metres = np.zeros((h, w))
    square_metres[valid] = (depth[valid]*scale/radius)**2*4/(w*h*abs(projection[0, 0]*projection[1, 1])*abs(product[valid]))
    retention = np.zeros((h, w));retention[plane[..., 3] > 0] = ground[..., 3][plane[..., 3] > 0]/plane[..., 3][plane[..., 3] > 0]
    square_metres *= np.clip(retention, 0, 1)
    # Convert the ray's plane intersection back into root-relative body metres.
    correction = (rays*depth[..., None]-point)@np.linalg.inv(rotation).T*scale
    distance = np.linalg.norm(ground[..., :3].astype(float)+correction, axis=2)
    areas, pixels, finite = [], [], []
    old_distance = np.linalg.norm(ground[..., :3].astype(float), axis=2)
    for lo, hi in BANDS:
        mask = (distance >= lo) & (distance < hi)
        areas.append(float(square_metres[mask].sum()))
        pixels.append(int(np.count_nonzero(mask & (square_metres > 0))))
        finite.append(float(ground[..., 3].astype(float)[(old_distance >= lo) & (old_distance < hi)].sum()))
    return areas, pixels, finite


def validate():
    manifest = load(DATA/'manifest.json')
    for name, digest in manifest['input_sha256'].items():
        assert hashlib.sha256((DATA/name).read_bytes()).hexdigest() == digest, name
    # The exact polygon/circle oracle is reused explicitly. Pixel reconstruction
    # above is independent of the frozen analyzer and consumes all raw GPU maps.
    sys.path.insert(0, str(DATA/'inputs/analysis'))
    spec = importlib.util.spec_from_file_location('exact_geometry', DATA/'inputs/analysis/geometry.py')
    geometry = importlib.util.module_from_spec(spec);spec.loader.exec_module(geometry)
    geometry.reference_checks()
    checked = []
    for dataset in manifest['final']:
        base = DATA/dataset['path'];report = load(base/'results.json')
        assert report['declared_relative_tolerance'] == .01 and report['declared_tile_area_tolerance'] == .0001
        assert report['minimum_band_samples'] == 100 and report['failures'] == []
        assert len(report['cases']) == 4
        for case in report['cases']:
            folder = base/case['case'];scene = load(folder/'scene.json')
            exact = geometry.exact_bands(scene);close(exact, case['exact_band_m2'])
            measured = []
            assert [(s['factor'], s['tile_edge']) for s in case['samples']] == [(1, 256), (2, 256), (4, 256), (4, 191)]
            for sample in case['samples']:
                target = folder/f'{sample["factor"]}x-tile-{sample["tile_edge"]}'
                receipt = load(target/'receipt.json');assert receipt == sample['receipt']
                w, h = receipt['viewport'];edge = receipt['tile_edge']
                assert [w, h] == [v*sample['factor'] for v in scene['viewport']]
                assert receipt['tiles'] == ((w+edge-1)//edge)*((h+edge-1)//edge)
                assert receipt['pixel_reads'] == 2*receipt['tiles'] and receipt['pixel_bytes'] == w*h*32
                assert receipt['gpu_target_bytes'] == edge*edge*36 <= 2359296
                assert receipt['tile_method'] == 'Unchanged full projection with translated viewport and bounded scissor'
                assert not receipt['timing_acceptance'] and not receipt['coverage_acceptance']
                if dataset['device'] == 'quadro':
                    assert receipt['context_uuid'] == manifest['expected_quadro_uuid'] and receipt['renderer'].startswith('Quadro M1000M')
                else:
                    assert receipt['renderer'].startswith('llvmpipe')
                maps = []
                for name in ('eligible', 'plane'):
                    raw = decode(target/(name+'.rgba32f'))
                    assert hashlib.sha256(raw).hexdigest() == sample['raw_sha256'][name]
                    assert len(raw) == w*h*16
                    values = np.frombuffer(raw, '<f4').reshape(h, w, 4)
                    assert np.isfinite(values).all() and (values[..., 3] >= 0).all()
                    maps.append(values)
                assert np.all(maps[0][..., 3] <= maps[1][..., 3]+1e-5)
                areas, pixels, finite = independently_measure(scene, receipt, *maps)
                close(areas, sample['analytic_band_m2']);close(finite, sample['finite_quad_band_m2'])
                assert pixels == sample['eligible_pixels']
                close([a/b-1 for a, b in zip(areas, exact)], sample['analytic_relative_error'])
                measured.append(areas)
            convergence = [y/x-1 for x, y in zip(measured[1], measured[2])]
            tiles = [y/x-1 for x, y in zip(measured[2], measured[3])]
            close(convergence, case['two_to_four_relative_change']);close(tiles, case['tile_relative_change'])
            assert case['qualified'] and max(abs(e) for e in case['samples'][2]['analytic_relative_error']+convergence) < .01
            assert max(abs(e) for e in tiles) < .0001 and min(case['samples'][2]['eligible_pixels']) >= 100
            checked.append({'device': dataset['device'], 'case': case['case'], 'exact_m2': exact,
                            'four_x_m2': measured[2], 'convergence': convergence, 'tile_change': tiles})
    for entry in load(DATA/'storage.json')['files']:
        raw = lzma.decompress((DATA/entry['path']).read_bytes())
        assert len(raw) == entry['decoded_bytes'] and hashlib.sha256(raw).hexdigest() == entry['decoded_sha256']
    before, after = [load(DATA/name) for name in ('before-provenance.json', 'provenance.json')]
    assert before['source_sha256'] == after['source_sha256'] and before['driver_fixtures'] == after['driver_fixtures']
    assert len(before['binaries']) == 43 and len(after['binaries']) == 44
    assert all(after['binaries'][name] == digest for name, digest in before['binaries'].items())
    for preflight, failures in zip(manifest['preflight'], (['scaled-grazing'], ['silhouette'])):
        assert preflight['failures'] == load(DATA/preflight['path']/'results.json')['failures'] == failures
    return {'final_gpu_snapshots': 32, 'final_scenes': checked, 'prior_executables_unchanged': 43,
            'route_acceptance': False, 'timing_acceptance': False}


if __name__ == '__main__':
    result = validate()
    if '--write' in sys.argv:
        (DATA/'checks.json').write_text(json.dumps(result, indent=2)+'\n')
    else:
        expected = load(DATA/'checks.json');assert result.keys() == expected.keys()
        for a, b in zip(result['final_scenes'], expected['final_scenes']):
            assert a['device'] == b['device'] and a['case'] == b['case']
            for name in ('exact_m2', 'four_x_m2', 'convergence', 'tile_change'):close(a[name], b[name])
        for name, digest in load(DATA/'evidence.json')['artifacts'].items():
            assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest() == digest, name
    print('Validated 32 known-geometry GPU snapshots, exact band areas, raw maps, bounded tiles and all declared gates')
