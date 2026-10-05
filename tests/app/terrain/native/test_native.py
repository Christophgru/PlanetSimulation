#!/usr/bin/env python3
"""Native compute input, reload, space/Moon contacts and compatibility acceptance."""
import argparse
import copy
import json
import math
import os
from pathlib import Path
import statistics
import subprocess
from NativeSession import Session, validate, write_json

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--probe', type=Path, required=True)
p.add_argument('--binary', type=Path, required=True)
p.add_argument('--output-dir', type=Path, required=True)
args = p.parse_args()
root = Path(__file__).resolve().parents[4]
out = args.output_dir.resolve()
out.mkdir(parents=True, exist_ok=True)
scene = json.loads((root / 'tests/scenarios/foliage/surface.json').read_text())
for planet in scene['planets']:
    planet['terrain_lod'] = {**planet.get('terrain_lod', {}), 'max_triangle_budget': 10000}
    planet['rotation'] = {'period_seconds': 0, 'axial_tilt_deg': 0}
    planet['mass_kg'] = 9.81 * (planet['radius'] * 1000)**2 / 6.67430e-11
    planet['atmosphere'] = {'enabled': False}
scene['sun']['mass_kg'] = 1e12
scene['lighting']['shadows']['resolution'] = 256
scene['skybox']['enabled'] = False
scene['planets'][0]['foliage']['max_blades'] = 128
scene['planets'][0]['foliage']['rebuild_distance_fraction'] = .01
scene['surface_camera']['direction_ned'] = [1, 0, 0]
scene['surface_camera'].pop('up_ned', None)
config = out / 'scene.json'
write_json(config, scene)
results = {}


def ready(frame):
    return frame['publication'].get('managed') and not frame['publication']['loading']


def standing(frame):
    return ready(frame) and frame['mode'] == 3 and frame.get('pose') and not frame['pose']['airborne']


def movement(session, sprint, distance):
    start = session.wait(standing)
    if sprint:
        session.down('Shift_L')
    session.down('w')
    try:
        last = session.wait(lambda f: standing(f) and f['keys']['w'] and f['keys']['shift'] == sprint and
                            f['pose']['walked_m'] - start['pose']['walked_m'] >= distance,
                            'Native walking did not cover the required distance', timeout=60)
    finally:
        session.up('w')
        if sprint:
            session.up('Shift_L')
    frames = [f for f in session.frames if start['frame'] < f['frame'] <= last['frame']]
    rates = []
    for a, b in zip(frames, frames[1:]):
        if standing(a) and standing(b) and a['keys']['w'] and b['keys']['w'] and a['keys']['shift'] == b['keys']['shift'] == sprint:
            dt = b['pose']['effect_s'] - a['pose']['effect_s']
            if dt > 1e-5:
                rates.append((b['pose']['walked_m'] - a['pose']['walked_m']) / dt)
    assert len(rates) >= 4, rates
    speed = statistics.median(rates)
    expected = 12 if sprint else 6
    assert expected * .85 < speed < expected * 1.15, (expected, speed)
    return speed, last


session = Session(args.probe, root, out / 'input', ['--config', str(config), '--terrain-backend', 'compute'], control={'delay': True})
try:
    session.focus()
    first = session.wait(lambda f: f['publication'].get('loading') and f['gl']['polls'] > 0)
    session.key('4')
    session.down('w')
    loading = session.wait(lambda f: f['mode'] == 3 and f['keys']['w'] and f['publication']['loading'])
    session.up('w')
    assert 'pose' not in loading and math.dist(first['camera_local'], loading['camera_local']) < 1e-10, loading
    session.control({})
    session.wait(standing)
    session.key('t')
    session.wait(lambda f: f['paused'])
    session.key('Escape')
    walk, moved = movement(session, False, 12)
    sprint, moved = movement(session, True, 5)
    moved = session.wait(lambda f: standing(f) and any(c['land_revision'] > 1 for c in f['publication']['consumers']))
    assert moved['pose']['trail_segments'] > 0
    assert any(c['land_revision'] > 1 for c in moved['publication']['consumers']), moved
    assert any(c['grass_draw_revision'] > 0 for c in moved['publication']['consumers']), moved
    session.key('space')
    session.wait(lambda f: f.get('pose', {}).get('airborne'))
    session.down('w')
    try:
        burning = session.wait(lambda f: f.get('pose', {}).get('boosting') and f['keys']['w'] and not f['keys']['space'])
    finally:
        session.up('w')
    session.down('space')
    try:
        session.wait(lambda f: f.get('pose', {}).get('boosting') and f['keys']['space'] and f['pose']['exhaust_particles'] > 0)
    finally:
        session.up('space')
    session.control({'delay': True})
    replacement = copy.deepcopy(scene)
    replacement['scenario_name'] = 'Native delayed reload'
    write_json(config, replacement)
    session.key('r')
    pending = session.wait(lambda f: f['reload']['pending'])
    session.down('w')
    try:
        live = session.wait(lambda f: f['reload']['pending'] and f['frame'] > pending['frame'] + 6)
    finally:
        session.up('w')
    assert live['reload']['epoch'] == pending['reload']['epoch'] == 1
    assert math.dist(live['pose']['root'], pending['pose']['root']) > .01
    session.control({'delay_retirement': True})
    committed = session.wait(lambda f: f['scenario'] == 'Native delayed reload' and f['reload']['published'] == 1)
    assert committed['reload']['retiring'] and committed['reload']['external_bytes'] > 0, committed
    session.control({'delay_retirement': True, 'fail_poll': 1})
    retirement = session.wait(lambda f: f['gl']['injected_poll_failures'] == 1 and f['reload']['retiring'])
    assert retirement['reload']['epoch'] == committed['reload']['epoch'], retirement
    session.control({})
    invalid = copy.deepcopy(replacement)
    invalid['planets'][0]['radius'] = -1
    write_json(config, invalid)
    failed = session.wait(lambda f: f['reload']['failed'] > committed['reload']['failed'])
    assert failed['reload']['epoch'] == committed['reload']['epoch']
    session.control({'fail_fence': 1})
    write_json(config, replacement)
    session.key('r')
    gpu_failed = session.wait(lambda f: f['reload']['failed'] > failed['reload']['failed'])
    assert gpu_failed['reload']['epoch'] == committed['reload']['epoch']
    session.control({})
    replacement['scenario_name'] = 'Native watcher recovery'
    write_json(config, replacement)
    session.wait(lambda f: f['scenario'] == 'Native watcher recovery' and f['reload']['published'] == 2)
    session.control({'delay': True})
    replacement['scenario_name'] = 'Superseded native request'
    write_json(config, replacement)
    session.key('r')
    waiting = session.wait(lambda f: f['reload']['pending'])
    replacement['scenario_name'] = 'Latest native request'
    replacement['planets'].reverse()
    write_json(config, replacement)
    session.key('r')
    session.wait(lambda f: f['reload']['superseded'] > waiting['reload']['superseded'])
    session.control({})
    session.wait(lambda f: f['scenario'] == 'Latest native request' and f['reload']['published'] == 3)
    session.key('2')
    session.wait(lambda f: f['mode'] == 1 and ready(f))
    session.key('4')
    session.wait(standing)
    session.close()
    results['input'] = {**validate(session.frames), 'walking_mps': walk, 'sprinting_mps': sprint}
    print('Validated native loading/standing/6-12 m/s walking/jump/WASD-Space thrust/trail/reload/watch/supersession', flush=True)
finally:
    session.abort()

# Retain completed acceptance results even if a later scenario fails.
write_json(out / 'results.json', results)

from FlightSeeds import seeds
paths, launch = seeds(args.binary, root, out, scene)
for name in ('space', 'moon'):
    path, world = paths[name]
    session = Session(args.probe, root, out / name, ['--replay', str(path)])
    try:
        session.focus()
        arrived = session.wait(lambda f: ready(f) and f.get('pose', {}).get('navigation') and
                               (f['pose']['navigation']['outer_space'] if name == 'space' else
                                f['pose']['navigation']['reference_body'] == 2))
        nav = arrived['pose']['navigation']
        assert len(nav['gravity_indices']) == 3, nav
        if name == 'space':
            assert nav['reference_body'] == 1 and nav['outer_space'], nav
            assert math.dist(nav['up'], launch['astronaut_pose']['navigation']['up']) < 1e-8, nav
        else:
            assert arrived['selected'] == 1 and not nav['outer_space'], arrived
            assert math.dist(nav['position_m'], world) < 5, nav
        session.down('w')
        try:
            thrust = session.wait(lambda f: f.get('pose', {}).get('boosting') and f['keys']['w'] and not f['keys']['space'])
        finally:
            session.up('w')
        assert thrust['pose']['thrust_n'] > 0, thrust
        session.key('r')
        session.wait(lambda f: f['reload']['published'] == 1 and f.get('pose', {}).get('navigation'))
        session.close()
        results[name] = validate(session.frames)
        print(f'Validated native {name} navigation, directional input, matching contacts and replay reload', flush=True)
    finally:
        session.abort()
    write_json(out / 'results.json', results)

old_gl = {'MESA_GL_VERSION_OVERRIDE': '3.3', 'MESA_GLSL_VERSION_OVERRIDE': '330'}
for name, flags, environment in [('default-cpu', ['--config', str(config)], {}),
                                  ('gl33-fallback', ['--config', str(config), '--terrain-backend', 'compute'], old_gl)]:
    session = Session(args.probe, root, out / name, flags, environment)
    try:
        session.focus()
        session.key('4')
        start = session.wait(lambda f: f['mode'] == 3 and f.get('pose'))
        session.down('w')
        try:
            session.wait(lambda f: f.get('pose') and f['pose']['walked_m'] > start['pose']['walked_m'] + .2)
        finally:
            session.up('w')
        session.close()
        results[name] = validate(session.frames, managed=False)
        if name == 'gl33-fallback':
            assert 'Terrain CPU fallback:' in session.text() and '4.3' in session.text(), session.text()
        print(f'Validated native {name} startup and walking', flush=True)
    finally:
        session.abort()

# These use the shipping executable and must reject before entering its event loop.
def reject(name, flags, required, environment=None):
    result = subprocess.run([str(args.binary.resolve()), *flags], cwd=root, env={**os.environ, **(environment or {})},
                            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30)
    (out / f'{name}.log').write_text(result.stdout)
    assert result.returncode != 0 and required in result.stdout, result.stdout
    results[name] = {'returncode': result.returncode, 'required_error': required}

reject('gl33-locked-replay', ['--replay', str(paths['space'][0])], 'Locked compute replay', old_gl)
reject('legacy-interactive-planner', ['--config', str(config), '--terrain-backend', 'compute', '--terrain-grass-planner', 'cpu'],
       'Interactive compute terrain requires')
legacy = copy.deepcopy(launch)
legacy['render']['terrain_grass_planner'] = 'cpu'
legacy_path = out / 'legacy-replay.json'
write_json(legacy_path, legacy)
reject('legacy-interactive-replay', ['--replay', str(legacy_path)], 'Interactive compute terrain requires GPU grass planning')
incompatible = copy.deepcopy(scene)
incompatible['planets'][0]['foliage']['compute_placement'] = False
incompatible_path = out / 'incompatible.json'
write_json(incompatible_path, incompatible)
reject('nonresident-interactive-grass', ['--config', str(incompatible_path), '--terrain-backend', 'compute'],
       'Interactive compute terrain requires foliage compute_placement')
write_json(out / 'results.json', results)
print('Validated native compute opt-in, five input scenarios and four startup rejection contracts', flush=True)
