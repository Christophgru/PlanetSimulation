"""Excluded live-plane analysis with independently sampled opaque occlusion."""
import gzip
import importlib.util
import json
import lzma
from pathlib import Path
import numpy as np

HERE = Path(__file__).resolve().parent

def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec); spec.loader.exec_module(value)
    return value

analytic = module('qualified_area', HERE.parent/'area/area.py')
matched = module('matched_area', HERE.parent/'matched/measure.py')
BANDS = analytic.BANDS

def raw(path):
    if path.exists(): return path.read_bytes()
    if path.with_suffix(path.suffix+'.xz').exists(): return lzma.decompress(path.with_suffix(path.suffix+'.xz').read_bytes())
    return gzip.decompress(path.with_suffix(path.suffix+'.gz').read_bytes())

def array(path, shape, dtype='<f4'):
    return np.frombuffer(raw(path), dtype).reshape(shape)

def smooth(lo, hi, x):
    t = np.clip((x-lo)/(hi-lo), 0, 1)
    return t*t*(3-2*t)

def biome(parameters, ground, normals, material, selected):
    """Double precision CPU predicate from raw positions/attributes, not GPU retention."""
    scale = parameters['meters_per_radius']
    pos = ground[..., :3].astype(float)/scale + np.asarray(parameters['root_body'])
    height = (np.linalg.norm(pos, axis=2)-1)*scale
    # GPU length is single precision. Keep that measured rounding bound explicit.
    h_gpu = normals[..., 3].astype(float)
    error = float(np.max(np.abs(height[selected]-h_gpu[selected]))) if selected.any() else 0
    assert error < 8*np.finfo(np.float32).eps*scale, error
    tint = material[..., :3].astype(float)*parameters['planet_color']
    water, beach, maximum = parameters['landscape_levels']
    if parameters['landscape']:
        beach_top = water+beach; beach_fade = max(.15, .15*beach)
        relief = max(1, maximum-water)
        start = max(beach_top+1, water+.25*relief)
        end = max(start+1, water+.40*relief)
        weight = 1-smooth(beach_top, beach_top+beach_fade, h_gpu)
        tint = np.array([1.1, 1.3, .18])*(1-weight[..., None])+np.array([4.2, 1.9, .18])*weight[..., None]
        snow = smooth(start, end, h_gpu)
        tint = (tint*(1-snow[..., None])+np.array([4.6, 2.3, .92])*snow[..., None])*parameters['planet_color']
    norm = normals[..., :3].astype(float)
    norm /= np.maximum(np.linalg.norm(norm, axis=2)[..., None], 1e-30)
    up = pos/np.maximum(np.linalg.norm(pos, axis=2)[..., None], 1e-30)
    slope = 1-np.sum(norm*up, axis=2)
    green = tint[..., 1]-parameters['green_ratio']*np.maximum(tint[..., 0], tint[..., 2])
    expected = 1-smooth(*parameters['rock_range'], slope)
    expected[green <= 0] = 0
    if parameters['water']: expected[h_gpu <= water+parameters['water_clearance_m']] = 0
    if parameters['landscape']: expected[h_gpu >= end] = 0
    # Only discontinuous float threshold ties are reported separately; no area
    # or convergence samples are removed from the actual measurements.
    boundary = np.abs(green) < 2e-6
    if parameters['water']: boundary |= np.abs(h_gpu-water-parameters['water_clearance_m']) < 2e-6
    if parameters['landscape']: boundary |= np.abs(h_gpu-end) < 2e-6
    check = selected & ~boundary
    discrepancy = float(np.max(np.abs(expected[check]-material[..., 3][check]))) if check.any() else 0
    assert discrepancy < .002, discrepancy
    return {'height_rounding_max_m': error, 'cpu_retention_max_error': discrepancy,
            'threshold_tie_pixels': int((selected & boundary).sum()), 'checked_pixels': int(check.sum()),
            'eligible_pixels': int((selected & (material[..., 3] > 0)).sum()),
            'ineligible_pixels': int((selected & (material[..., 3] == 0)).sum())}

def edge(mask):
    result = np.zeros_like(mask)
    change = mask[1:] != mask[:-1]; result[1:] |= change; result[:-1] |= change
    change = mask[:, 1:] != mask[:, :-1]; result[:, 1:] |= change; result[:, :-1] |= change
    return result

def roots(folder, common, ground, plane, tags):
    height, width = tags.shape
    scale = common['meters_per_radius']
    vm = np.array(common['view']).reshape(4, 4, order='F') @ np.array(common['model']).reshape(4, 4, order='F')
    clip_matrix = np.array(common['projection']).reshape(4, 4, order='F') @ vm
    root = np.asarray(common['astronaut']['root']); root_body = (root/scale).astype(np.float32).astype(float)
    visible, faded = np.zeros(3, int), np.zeros(3)
    for name in ('detailed', 'quads'):
        blades = array(folder/'grass'/(name+'.blades'), (-1, 16))
        points = blades[:, :3].astype(float)
        clip = np.column_stack((points, np.ones(len(points)))) @ clip_matrix.T
        valid = (clip[:, 3] > 0) & np.all(np.abs(clip[:, :3]) <= clip[:, 3:4], axis=1)
        indices = np.flatnonzero(valid); ndc = clip[indices, :2]/clip[indices, 3:4]
        x = np.minimum(width-1, ((ndc[:, 0]+1)*width/2).astype(int))
        y = np.minimum(height-1, ((ndc[:, 1]+1)*height/2).astype(int))
        sampled = ground[y, x, :3].astype(float)/scale+root_body
        depth, ray_valid = matched.occlusion_depth(points[indices], sampled, plane[y, x, :3].astype(float), vm, scale)
        seen = np.zeros(len(points), bool)
        seen[indices] = (tags[y, x] == 2) & (ground[y, x, 3] > 0) & ray_valid & (depth <= .15)
        distance = np.linalg.norm(points*scale-root, axis=1)
        for i, (lo, hi) in enumerate(BANDS):
            selected = seen & (distance >= lo) & (distance < hi)
            visible[i] += int(selected.sum()); faded[i] += float(blades[selected, 3].sum(dtype=float))
    return visible, faded

def analyze(folder):
    folder = Path(folder)
    common = json.loads((folder/'controls.json').read_text()); receipt = json.loads((folder/'receipt.json').read_text())
    live = receipt['live']; assert live['schema'] == 1
    assert receipt['native_workload_before'] == receipt['native_workload_after']
    assert receipt['native_publication_before'] == receipt['native_publication_after']
    assert live['gpu_target_bytes'] == 256*256*68 and live['cpu_read_tile_bytes'] == 256*256*17
    width, height = common['viewport']
    rgba = [array(folder/'images'/(name+'.rgba'), (height, width, 4), np.uint8) for name in ('grass', 'bare')]
    grass_tags, bare = [array(folder/'images'/(name+'.stencil'), (height, width), np.uint8) for name in ('grass', 'bare')]
    composed = (grass_tags == 5) & (np.max(np.abs(rgba[0][..., :3].astype(np.int16)-rgba[1][..., :3]), axis=2) >= receipt['composed_rgb_threshold'])
    boundary = edge(composed)
    result = {'schema': 1, 'coverage_acceptance': False, 'timing_acceptance': False,
              'composed_mask_grid': common['viewport'], 'composed_grass_pixels': int(composed.sum()),
              'edge_bound_scope': 'Eligible area touching a native 4-neighbour composed-mask boundary; not a proof of supersampled compositing',
              'samples': []}
    for sample in live['samples']:
        factor = sample['factor']; w, h = sample['viewport']; base = folder/'live'/f'{factor}x'
        assert [w, h] == [width*factor, height*factor]
        count = 4 if factor == 1 else 2
        assert sample['pixel_bytes'] == w*h*(count*16+1)
        assert sample['tiles'] == ((w+255)//256)*((h+255)//256)
        assert sample['pixel_reads'] == sample['tiles']*(count+1)
        ground, plane = [array(base/(n+'.rgba32f'), (h, w, 4)) for n in ('eligible', 'plane')]
        tags = array(base/'objects.stencil', (h, w), np.uint8)
        assert set(np.unique(tags)) <= {0, 1, 2, 3, 4}
        assert not np.any(ground[tags != 2]) and not np.any(plane[tags != 2])
        control = {**common, **live['parameters'], 'viewport': [w, h]}
        area, distance = analytic.differential_area(control, ground, plane)
        visible, faded = roots(folder, common, ground, plane, tags)
        # Only the displayed color mask is expanded. Surface eligibility and
        # opaque occlusion are measured afresh on each GPU sampling grid.
        mask = composed.repeat(factor, axis=0).repeat(factor, axis=1)
        edges = boundary.repeat(factor, axis=0).repeat(factor, axis=1)
        bands = []
        for i, (lo, hi) in enumerate(BANDS):
            selected = (area > 0) & (distance >= lo) & (distance < hi)
            denominator = float(area[selected].sum()); covered = float(area[selected & mask].sum())
            uncertainty = float(area[selected & edges].sum())
            bands.append({'band_m': [lo, hi], 'eligible_visible_m2': denominator,
                          'eligible_pixels': int(selected.sum()), 'ground_visible_roots': int(visible[i]),
                          'visible_fade_sum': float(faded[i]),
                          'visible_roots_per_eligible_m2': float(visible[i]/denominator) if denominator else None,
                          'fade_per_eligible_m2': float(faded[i]/denominator) if denominator else None,
                          'composed_projected_eligible_coverage': covered/denominator if denominator else None,
                          'composed_boundary_m2': uncertainty,
                          'composed_boundary_fraction': uncertainty/denominator if denominator else None})
        entry = {'factor': factor, 'bands': bands, 'character_pixels': int((tags == 4).sum()),
                 'other_body_pixels': int(((tags == 1) | (tags == 3)).sum())}
        if factor == 1:
            mismatch = int((tags != bare).sum())
            entry['native_opaque_tag_mismatches'] = mismatch
            entry['native_opaque_tag_mismatch_fraction'] = mismatch/(w*h)
            assert mismatch/(w*h) <= live['native_tag_mismatch_tolerance'], entry
            attrs = [array(base/(n+'.rgba32f'), (h, w, 4)) for n in ('normal-height', 'material')]
            entry['biome'] = biome(live['parameters'], ground, *attrs, tags == 2)
        result['samples'].append(entry)
        del area, distance, mask, edges, ground, plane, tags
    result['biome_diagnostics'] = []
    for case in live['biomes']['cases']:
        w, h = case['viewport']; base = folder/'live/biomes'/case['name']
        assert case['pixel_bytes'] == w*h*48 and case['pixel_reads'] == 3
        values = [array(base/(name+'.rgba32f'), (h, w, 4)) for name in ('position', 'normal-height', 'material')]
        selected = np.linalg.norm(values[1][..., :3], axis=2) > 0
        checked = biome(case['parameters'], *values, selected)
        assert checked['checked_pixels'] > 100
        if case['name'] in ('water-reject', 'green-reject', 'beach', 'snow'):
            assert checked['eligible_pixels'] == 0, checked
        if case['name'] in ('water-pass', 'green-pass', 'meadow', 'rock-half'):
            assert checked['ineligible_pixels'] == 0, checked
        result['biome_diagnostics'].append({'name': case['name'], **checked})
    a, b = result['samples'][1:]
    result['two_to_four_relative_change'] = [y['eligible_visible_m2']/x['eligible_visible_m2']-1
                                            if x['eligible_visible_m2'] else None for x, y in zip(a['bands'], b['bands'])]
    result['area_converged'] = all(v is not None and abs(v) < live['convergence_tolerance'] for v in result['two_to_four_relative_change'])
    result['sample_minimums_met'] = all(b['eligible_pixels'] >= 100 for b in result['samples'][-1]['bands'])
    result['qualified_area'] = result['area_converged'] and result['sample_minimums_met']
    result['bands'] = result['samples'][-1]['bands']
    (folder/'live-analysis.json').write_text(json.dumps(result, indent=2)+'\n')
    return result


def compare(cpu_folder, compute_folder, tolerance=.05):
    assert json.loads((Path(cpu_folder)/'controls.json').read_text()) == json.loads((Path(compute_folder)/'controls.json').read_text())
    results = [analyze(p) for p in (cpu_folder, compute_folder)]
    rows = []
    for a, b in zip(results[0]['bands'], results[1]['bands']):
        ratios = {key: b[key]/a[key] if a[key] and b[key] is not None else None
                  for key in ('eligible_visible_m2', 'visible_roots_per_eligible_m2',
                              'fade_per_eligible_m2', 'composed_projected_eligible_coverage')}
        rows.append({'band_m': a['band_m'], 'compute_over_cpu': ratios,
                     'within_declared_tolerance': all(v is not None and abs(v-1) <= tolerance for v in ratios.values())})
    return {'relative_tolerance': tolerance, 'bands': rows,
            'coverage_parity': all(r['within_declared_tolerance'] for r in rows),
            'qualified_area': all(r['qualified_area'] for r in results),
            'coverage_acceptance': False, 'timing_acceptance': False}
