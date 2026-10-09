"""Grass must refresh on installed terrain while its CPU replacement is held."""
import argparse
import json
import math
from pathlib import Path
import subprocess
import sys
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from NativeSession import Session, validate, write_json
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'coverage'))
from analyze import analyze

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--probe', type=Path, required=True)
p.add_argument('--output-dir', type=Path, required=True)
args = p.parse_args()
root = Path(__file__).resolve().parents[5]
out = args.output_dir.resolve()
out.mkdir(parents=True, exist_ok=True)
(out/'results.json').unlink(missing_ok=True)
scene = json.loads((root/'tests/scenarios/foliage/surface.json').read_text())
for planet in scene['planets']:
    planet['rotation'] = {'period_seconds': 0, 'axial_tilt_deg': 0}
    planet['surface_noise'] = []
    planet['terrain_landscape'] = {'enabled': False}
    planet['terrain_lod'] = {**planet.get('terrain_lod', {}), 'max_triangle_budget': 10000}
    planet['atmosphere'] = {'enabled': False}
scene['planets'][0]['color'] = [.2, .8, .1]
scene['planets'][0]['water']['level_m'] = -10
scene['planets'][0]['foliage'].update({'density_per_m2': 1, 'max_blades': 100000,
    'draw_distance_m': 150, 'rebuild_distance_fraction': .15, 'height_m': .6})
scene['lighting']['shadows']['resolution'] = 256
scene['skybox']['enabled'] = False
scene['surface_camera'].update({'direction_ned': [1, 0, 0], 'simulation_time_seconds': 0})
scene['surface_camera'].pop('up_ned', None)
replay = out/'input.json'
write_json(replay, {'scenario': scene, 'surface_camera': scene['surface_camera']})
results = []
for backend, mode in [('compute', 3), ('compute', 1), ('cpu', 3)]:
    folder = out/f'{backend}-{mode}'
    environment = {'PLANET_NATIVE_BENCHMARK': '1'}
    if mode == 3:
        environment['PLANET_NATIVE_COVERAGE'] = str(folder/'coverage')
        (folder/'coverage/1/snapshot.json').unlink(missing_ok=True)
    session = Session(args.probe, root, folder,
        ['--replay', str(replay), '--terrain-backend', backend, '--terrain-grass-planner',
         'gpu' if backend == 'compute' else 'cpu', '--performance-trace', str(folder/'performance.csv')],
        environment=environment)
    try:
        session.focus()
        subprocess.run(['xdotool', 'windowsize', '--sync', session.window, '320', '180'], check=True)
        session.key('4' if mode == 3 else '2')
        def ready(f):
            return f['mode'] == mode and not f['publication'].get('loading', False) and (
                mode != 3 or f.get('pose') and not f['pose']['airborne'])
        warm = session.wait(ready, timeout=90)
        session.wait(lambda f: ready(f) and f['observed_ns'] > warm['observed_ns']+2_000_000_000, timeout=60)
        control = {'hold_terrain': True}
        if mode == 3: control.update({'coverage_request': 1, 'coverage_distance_m': 80})
        session.control(control)
        if mode == 3: session.down('Shift_L')
        session.down('w')
        try:
            first = session.wait(lambda f: ready(f) and f['keys']['w'])
            def traveled(f):
                return (f['pose']['walked_m']-first['pose']['walked_m'] if mode == 3 else
                        math.dist(f['camera_local'], first['camera_local'])*1000)
            last = session.wait(lambda f: ready(f) and f['keys']['w'] and traveled(f) >= 90,
                                'Grass refresh route did not finish', timeout=25)
        finally:
            session.up('w')
            if mode == 3: session.up('Shift_L')
        moving = [f for f in session.frames if first['frame'] <= f['frame'] <= last['frame']]
        held = [f for f in moving if f['worker']['running']]
        assert held and 'Terrain refresh test: worker held' in session.text()
        offsets = [math.dist(f['benchmark']['surface_target_eye_body'],
                    f['benchmark']['bodies'][0]['foliage']['plan_eye'])*1000 for f in held]
        changes = [b for a, b in zip(held, held[1:]) if
                   a['benchmark']['bodies'][0]['foliage']['plan_eye'] != b['benchmark']['bodies'][0]['foliage']['plan_eye']]
        assert len(changes) >= 2, 'Grass starved behind the held terrain builder'
        assert all(a['benchmark']['bodies'][0]['terrain_plan_eye'] == held[0]['benchmark']['bodies'][0]['terrain_plan_eye'] for a in held)
        assert all(f['publication_geometry'][0] == held[0]['publication_geometry'][0] for f in held)
        margin = 150*.15
        if backend == 'compute':
            assert max(offsets) < margin, (mode, offsets)
            assert last['publication']['grass_only_published'] >= first['publication']['grass_only_published']+2
            assert last['publication']['interactive_failures'] == 0
        assert all(f['benchmark']['bodies'][0]['foliage']['density'] == 1 and
                   not f['benchmark']['bodies'][0]['foliage']['policy']['near_infeasible'] for f in held)
        # Let a forward prediction finish after a stop, then reverse while the
        # terrain builder is still held. Publication must never install a patch
        # that has lost coverage of the actual camera after its forecast changed.
        stopped = session.control({'hold_terrain': True, 'phase': 'stopped'})
        session.wait(lambda f: ready(f) and f['observed_ns'] >= stopped['observed_ns']+1_000_000_000)
        reverse_first = session.control({'hold_terrain': True, 'phase': 'reversing'})
        session.down('s')
        try:
            def reversed_distance(f):
                return (f['pose']['walked_m']-reverse_first['pose']['walked_m'] if mode == 3 else
                        math.dist(f['camera_local'], reverse_first['camera_local'])*1000)
            reverse_last = session.wait(lambda f: ready(f) and reversed_distance(f) >= 25, timeout=20)
        finally:
            session.up('s')
        assert sum((reverse_last['camera_local'][k]-reverse_first['camera_local'][k]) *
                   (last['camera_local'][k]-first['camera_local'][k]) for k in range(3)) < 0, 'Route did not reverse'
        turning = [f for f in session.frames if stopped['frame'] <= f['frame'] <= reverse_last['frame']]
        turn_offset = max(math.dist(f['benchmark']['surface_target_eye_body'],
            f['benchmark']['bodies'][0]['foliage']['plan_eye'])*1000 for f in turning)
        if backend == 'compute': assert turn_offset < margin, (mode, turn_offset)
        session.control({'hold_terrain': False})
        session.wait(lambda f: ready(f) and not f['worker']['running'] and not f['worker']['queued'] and not f['worker']['ready'], timeout=60)
        session.close()
        coverage = None
        if mode == 3:
            coverage = analyze(folder/'coverage/1')
            receipt = json.loads((folder/'coverage/1/snapshot.json').read_text())
            assert receipt['land_revision'] == held[0]['publication_geometry'][0]['land_revision']
            assert receipt['profile_frame'] in {f['profile_frame'] for f in held}
            assert coverage['opaque_grass_pixels'] > 0
            # This airless, green, flat fixture has no water/cliff/snow rejection.
            # Inspect actual main-view generated roots, independently of scalar
            # policy receipts. The uniform-density reference allows seeded
            # sampling and camera-centered falloff at the outer astronaut band.
            for count, area in zip(coverage['root_band_counts'][:2], [math.pi*25, math.pi*200]):
                assert .6*area < count < 1.4*area, coverage
        results.append({'backend': backend, 'mode': mode, 'renderer': first['renderer'],
            'distance_m': traveled(last), 'configured_speed_mps': 12 if mode == 3 else 80,
            'wall_speed_mps': traveled(last)/((last['observed_ns']-first['observed_ns'])/1e9),
            'held_frames': len(held), 'grass_refreshes_while_held': len(changes),
            'maximum_plan_offset_m': max(offsets), 'placement_margin_m': margin,
            'stop_reverse_maximum_offset_m': turn_offset,
            'coverage': coverage, 'audit': validate(session.frames, managed=backend == 'compute')})
        write_json(out/'results.json', results)
        print(results[-1], flush=True)
    finally:
        session.abort()
