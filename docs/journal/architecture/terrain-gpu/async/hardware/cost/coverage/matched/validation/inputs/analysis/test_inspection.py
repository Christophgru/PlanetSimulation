#!/usr/bin/env python3
"""Verify blocking coverage inspection and untouched native audit separately."""
import argparse
import copy
import json
from pathlib import Path
import sys
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from NativeSession import Session, validate, write_json
from analyze import analyze

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--probe', type=Path, required=True)
p.add_argument('--output-dir', type=Path, required=True)
p.add_argument('--matched', action='store_true')
args = p.parse_args()
root = Path(__file__).resolve().parents[5]
out = args.output_dir.resolve()
out.mkdir(parents=True, exist_ok=True)
scene = json.loads((root / 'tests/scenarios/foliage/surface.json').read_text())
for planet in scene['planets']:
    planet['terrain_lod'] = {**planet.get('terrain_lod', {}), 'max_triangle_budget': 10000}
    planet['rotation'] = {'period_seconds': 0, 'axial_tilt_deg': 0}
    planet['surface_noise'] = []
    planet['terrain_landscape'] = {'enabled': False}
    planet.setdefault('atmosphere', {'enabled': False})
scene['planets'][0]['color'] = [.2, .8, .1]
scene['planets'][0]['water'] = {**scene['planets'][0]['water'], 'level_m': -10}
scene['planets'][0]['foliage'].update({'max_blades': 10000, 'height_m': .6, 'draw_distance_m': 40})
scene['lighting']['shadows']['resolution'] = 256
scene['skybox']['enabled'] = False
scene['surface_camera']['direction_ned'] = [1, 0, 0]
scene['surface_camera'].pop('up_ned', None)
scene['surface_camera']['simulation_time_seconds'] = 0
results = []
for atmosphere in (False, True):
    case_scene = copy.deepcopy(scene)
    for planet in case_scene['planets']:
        planet['atmosphere']['enabled'] = atmosphere
    config = out / ('hdr.json' if atmosphere else 'airless.json')
    write_json(config, {'scenario': case_scene, 'surface_camera': case_scene['surface_camera']} if args.matched else case_scene)
    for backend in ('cpu', 'compute'):
        folder = out / f'{backend}-{"hdr" if atmosphere else "airless"}'
        flags = ['--replay' if args.matched else '--config', str(config), '--terrain-backend', backend, '--terrain-grass-planner',
                 'gpu' if backend == 'compute' else 'cpu', '--performance-trace', str(folder / 'performance.csv')]
        session = Session(args.probe, root, folder, flags, environment={'PLANET_NATIVE_COVERAGE': str(folder / 'coverage')})
        try:
            captured = []
            session.focus()
            session.key('4')
            session.wait(lambda f: f['mode'] == 3 and f.get('pose') and not f['pose']['airborne'] and
                         not f['publication'].get('loading', False), timeout=90)
            for token, distance in ((1, 0), (2, 3)):
                snapshot = folder / 'coverage' / str(token) / 'snapshot.json'
                snapshot.unlink(missing_ok=True)
                matched = snapshot.parent / 'matched'
                (matched / 'receipt.json').unlink(missing_ok=True)
                control = {'coverage_request': token, 'coverage_distance_m': distance,
                           'coverage_matched': args.matched}
                if args.matched and backend == 'compute':
                    control['coverage_reference'] = str(out / f'cpu-{"hdr" if atmosphere else "airless"}' /
                        'coverage' / str(token) / 'matched/controls.json')
                session.control(control)
                if distance:
                    session.down('w')
                try:
                    session.wait(lambda f: snapshot.exists() and (not args.matched or (matched / 'receipt.json').exists()) and f['profile_frame'] >=
                                 json.loads(snapshot.read_text())['profile_frame'], timeout=90)
                finally:
                    if distance:
                        session.up('w')
                result = analyze(folder / 'coverage' / str(token))
                receipt = json.loads((folder / 'coverage' / str(token) / 'snapshot.json').read_text())
                assert sum(result['generated_instances']) > 0 and result['opaque_grass_pixels'] > 0
                assert receipt['inspection']['pixel_reads'] == 2
                assert 'reflection_instances_after_main' in receipt
                assert receipt['wind_s'] >= 0 and receipt['walked_m'] >= distance
                assert receipt['trail_segments_m'] == receipt['astronaut']['grass_trail']
                if backend == 'compute':
                    consumer = receipt['publication_after_frame']['consumers'][0]
                    assert consumer['land_revision'] == consumer['grass_revision'] == receipt['land_revision']
                    assert consumer['land'] == consumer['grass'] == consumer['contacts']
                results.append({'backend': backend, 'hdr': atmosphere, 'request': token, **result})
                captured.append(receipt)
                if args.matched:
                    import importlib.util
                    spec = importlib.util.spec_from_file_location('matched_analysis', Path(__file__).parent / 'matched/measure.py')
                    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
                    controlled = module.analyze(matched)
                    controlled_receipt = json.loads((matched / 'receipt.json').read_text())
                    assert controlled['composed_grass_pixels'] > 0 and controlled['character_pixels'] > 0
                    assert sum(b['eligible_visible_m2'] for b in controlled['bands']) > 0
                    assert controlled_receipt['cpu_preparations_skipped'] == (0 if backend == 'compute' else 4)
                    if backend == 'compute':
                        reference = Path(control['coverage_reference']).parent
                        assert json.loads((matched / 'controls.json').read_text()) == json.loads((reference / 'controls.json').read_text())
                        results[-1]['matched_comparison'] = module.compare(reference, matched)
                    results[-1]['matched_analysis'] = controlled
            session.close()
            validate(session.frames, managed=backend == 'compute')
            frames = {f['profile_frame']: f for f in session.frames}
            for receipt in captured:
                frame = frames[receipt['profile_frame']]
                assert frame['pose']['root'] == receipt['root_m']
                assert frame['pose']['walked_m'] == receipt['walked_m']
        finally:
            session.abort()
(out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
print('Verified eight live main-view snapshots, queue bounds, depth masks, reflection separation and normal-frame audits')
