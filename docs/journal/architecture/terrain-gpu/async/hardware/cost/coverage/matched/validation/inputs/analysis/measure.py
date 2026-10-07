"""Independent eligible-area and composed-grass analysis; no cost acceptance."""
import gzip
import json
from pathlib import Path
import numpy as np

BANDS = ((0, 5), (5, 15), (15, 30))


def raw(path):
    return path.read_bytes() if path.exists() else gzip.decompress(path.with_suffix(path.suffix + '.gz').read_bytes())


def occlusion_depth(points, sampled, normal, view_matrix, scale):
    eye_body = np.linalg.inv(view_matrix)[:3, 3]
    ray = points-eye_body
    divisor = np.sum(ray*normal, axis=1)
    intersection = np.divide(np.sum((sampled-eye_body)*normal, axis=1), divisor,
                             out=np.full(len(points), np.nan), where=np.abs(divisor)>1e-12)
    forward_body = view_matrix[2, :3]/np.linalg.norm(view_matrix[2, :3])
    depth = (1-intersection)*(-ray @ forward_body)*scale
    return depth, np.isfinite(intersection) & (intersection>0)


def analyze(folder):
    folder = Path(folder)
    common = json.loads((folder / 'controls.json').read_text())
    receipt = json.loads((folder / 'receipt.json').read_text())
    grass = json.loads((folder / 'grass/snapshot.json').read_text())
    native = json.loads((folder.parent / 'snapshot.json').read_text())
    assert receipt['schema'] == common['schema'] == 1
    assert not receipt['timing_acceptance'] and not receipt['coverage_acceptance']
    assert receipt['source_native_frame'] == native['profile_frame'] == grass['profile_frame']
    assert receipt['native_workload_before'] == receipt['native_workload_after'] == native['workload']
    assert receipt['native_publication_before'] == receipt['native_publication_after'] == native['publication_after_frame']
    for name in ('view', 'projection', 'model', 'viewport', 'meters_per_radius'):
        assert grass[name] == common[name]
    assert grass['root_m'] == common['astronaut']['root']
    assert grass['trail_segments_m'] == common['astronaut']['grass_trail']
    width, height = common['viewport']
    rgba = [np.frombuffer(raw(folder / 'images' / (name + '.rgba')), np.uint8).reshape(height, width, 4)
            for name in ('grass', 'bare')]
    stencil, bare = [np.frombuffer(raw(folder / 'images' / (name + '.stencil')), np.uint8).reshape(height, width)
                     for name in ('grass', 'bare')]
    ground = np.frombuffer(raw(folder / 'ground/eligible.rgba32f'), '<f4').reshape(height, width, 4)
    plane = np.frombuffer(raw(folder / 'ground/plane.rgba32f'), '<f4').reshape(height, width, 4)
    assert np.isfinite(ground).all() and (ground[..., 3] >= 0).all()
    assert np.isfinite(plane).all() and np.all(ground[..., 3] <= plane[..., 3]+1e-5)
    assert set(np.unique(stencil)) <= {0, 1, 2, 3, 4, 5} and 5 not in np.unique(bare)
    delta = np.max(np.abs(rgba[0][..., :3].astype(np.int16) - rgba[1][..., :3]), axis=2)
    opaque = stencil == 5
    composed = opaque & (delta >= receipt['composed_rgb_threshold'])
    eligible = (bare == 2) & (ground[..., 3] > 0)
    distance = np.linalg.norm(ground[..., :3].astype(np.float64), axis=2)
    area = ground[..., 3].astype(np.float64)
    root = np.array(common['astronaut']['root'], dtype=np.float64)
    scale = common['meters_per_radius']
    view = np.array(common['view']).reshape(4, 4, order='F')
    model = np.array(common['model']).reshape(4, 4, order='F')
    projection = np.array(common['projection']).reshape(4, 4, order='F')
    clip_matrix = projection @ view @ model
    view_matrix = view @ model
    generated, visible, faded = np.zeros(3, int), np.zeros(3, int), np.zeros(3, float)
    for q, name in enumerate(('detailed', 'quads')):
        values = np.frombuffer(raw(folder / 'grass' / (name + '.blades')), '<f4').reshape(-1, 16)
        assert len(values) == grass['queues'][q]['instances'] and np.isfinite(values).all()
        assert np.all((values[:, 3] > 0) & (values[:, 3] <= 1))
        points = values[:, :3].astype(np.float64)
        d = np.linalg.norm(points*scale - root, axis=1)
        homogeneous = np.column_stack((points, np.ones(len(points))))
        clip = homogeneous @ clip_matrix.T
        valid = (clip[:, 3] > 0) & np.all(np.abs(clip[:, :3]) <= clip[:, 3:4], axis=1)
        indices = np.flatnonzero(valid)
        ndc = clip[indices, :2] / clip[indices, 3:4]
        x = np.minimum(width-1, ((ndc[:, 0]+1)*width/2).astype(int))
        y = np.minimum(height-1, ((ndc[:, 1]+1)*height/2).astype(int))
        # Intersect the root's own ray with the live triangle plane, eliminating
        # pixel-center depth bias. One-sided tolerance admits above-ground roots
        # but rejects roots behind foreground terrain. Roots covered by the
        # character or other bodies are removed by bare final stencil.
        root_body = (root/scale).astype(np.float32).astype(np.float64)
        sampled = ground[y, x, :3].astype(np.float64)/scale + root_body
        normal = plane[y, x, :3].astype(np.float64)
        view_depth_difference, ray_valid = occlusion_depth(points[indices], sampled, normal, view_matrix, scale)
        visible_root = np.zeros(len(points), bool)
        visible_root[indices] = eligible[y, x] & ray_valid & (view_depth_difference <= receipt['root_visibility_tolerance_m'])
        for band, (lo, hi) in enumerate(BANDS):
            selected = (d >= lo) & (d < hi)
            generated[band] += np.count_nonzero(selected)
            visible[band] += np.count_nonzero(selected & visible_root)
            faded[band] += np.sum(values[selected & visible_root, 3], dtype=np.float64)
    rows = []
    for band, (lo, hi) in enumerate(BANDS):
        selected = eligible & (distance >= lo) & (distance < hi)
        denominator = float(np.sum(area[selected]))
        covered = float(np.sum(area[selected & composed]))
        rows.append({'band_m': [lo, hi], 'eligible_visible_m2': denominator,
                     'eligible_pixels': int(np.count_nonzero(selected)),
                     'generated_roots': int(generated[band]), 'ground_visible_roots': int(visible[band]),
                     'visible_fade_sum': float(faded[band]),
                     'visible_roots_per_eligible_m2': float(visible[band]/denominator) if denominator else None,
                     'fade_per_eligible_m2': float(faded[band]/denominator) if denominator else None,
                     'composed_projected_eligible_coverage': covered/denominator if denominator else None})
    for name, mask in (('composed', composed), ('eligible', eligible)):
        (folder / (name + '-mask.pgm')).write_bytes(f'P5\n{width} {height}\n255\n'.encode() +
                                                 (mask[::-1].astype(np.uint8)*255).tobytes())
    result = {'bands': rows, 'nearest_opaque_grass_pixels': int(opaque.sum()),
              'composed_grass_pixels': int(composed.sum()), 'character_pixels': int((stencil == 4).sum()),
              'scope': receipt['scope'], 'coverage_acceptance': False,
              'eligible_area_method': 'Live triangle raster derivatives times expected biome/rock retention; no Gaussian falloff',
              'root_visibility_method': receipt['root_visibility_method']+'; tolerance 0.15 m',
              'density_scope': 'Screen-visible generated root centers per expected eligible ground area; not physical blade count'}
    (folder / 'analysis.json').write_text(json.dumps(result, indent=2) + '\n')
    return result


def compare(cpu_folder, compute_folder, tolerance=.05):
    cpu_folder, compute_folder = Path(cpu_folder), Path(compute_folder)
    assert json.loads((cpu_folder / 'controls.json').read_text()) == json.loads((compute_folder / 'controls.json').read_text())
    cpu, compute = analyze(cpu_folder), analyze(compute_folder)
    rows = []
    for a, b in zip(cpu['bands'], compute['bands']):
        ratios = {key: b[key]/a[key] if a[key] and b[key] is not None else None
                  for key in ('eligible_visible_m2', 'visible_roots_per_eligible_m2',
                              'fade_per_eligible_m2', 'composed_projected_eligible_coverage')}
        passed = all(value is not None and abs(value-1) <= tolerance for value in ratios.values())
        rows.append({'band_m': a['band_m'], 'compute_over_cpu': ratios, 'within_declared_tolerance': passed})
    return {'relative_tolerance': tolerance, 'bands': rows, 'coverage_parity': all(r['within_declared_tolerance'] for r in rows),
            'timing_acceptance': False}
