"""Optional NVIDIA production regression: fast steep traversal, without injected delays.

Run under native GLX/Xvfb; --all includes the startup and full-resolution routes.
Raw traces stay in the supplied ignored build directory. Not part of software CTest.
"""
import argparse
import csv
import json
import math
from pathlib import Path
import subprocess
import sys
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from NativeSession import Session, validate, write_json

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--probe', type=Path, required=True)
p.add_argument('--output-dir', type=Path, required=True)
p.add_argument('--all', action='store_true')
args = p.parse_args()
root = Path(__file__).resolve().parents[5]
out = args.output_dir.resolve()
out.mkdir(parents=True, exist_ok=True)
base = json.loads((root/'configs/scenarios/solar_system.json').read_text())
mountain = {'altitude': .002, 'direction_ned': [.7430543107717261, .15323081761658303, .6514526903590994],
    'fov': 80., 'latitude_deg': -18.726025155509635, 'longitude_deg': 113.23762488691663,
    'planet_index': 0, 'reference_frame': 'planet_spherical_ned', 'simulation_time_seconds': 180.34667306699998,
    'up_ned': [.6380276560948606, .13157248129530347, -.7586892593307851], 'walk_speed_mps': 80.}
cases = [('mountain', 1, [320, 180], 240)]
if args.all:
    cases += [('startup', 1, [320, 180], 240)] + [
        (view, mode, [1280, 720], 72 if mode == 3 else 240)
        for mode in [3, 1] for view in ['startup', 'mountain']]
results = []
for view, mode, size, distance in cases:
    scene = json.loads(json.dumps(base))
    if view == 'mountain': scene['surface_camera'] = mountain
    units = 1000 if scene['distance_unit'] == 'km' else 1
    scale = scene['planets'][0]['radius'] * units
    foliage = scene['planets'][0]['foliage']
    margin = foliage['draw_distance_m'] * foliage['rebuild_distance_fraction']
    folder = out/f'{view}-{mode}-{size[0]}'
    folder.mkdir(parents=True, exist_ok=True)
    replay = folder/'input.json'
    write_json(replay, {'scenario': scene, 'surface_camera': scene['surface_camera']})
    trace = folder/'performance.csv'
    s = Session(args.probe.resolve(), root, folder, ['--replay', str(replay), '--terrain-backend', 'compute',
        '--terrain-grass-planner', 'gpu', '--performance-trace', str(trace)],
        environment={'PLANET_NATIVE_BENCHMARK': '1'})
    try:
        s.focus()
        subprocess.run(['xdotool', 'windowsize', '--sync', s.window, *map(str, size)], check=True)
        s.key('4' if mode == 3 else '2')
        def ready(f):
            return f['mode'] == mode and not f['publication']['loading'] and (
                mode != 3 or f.get('pose') and not f['pose']['airborne'])
        warm = s.wait(ready, timeout=180)
        s.wait(lambda f: ready(f) and f['observed_ns'] >= warm['observed_ns']+3_000_000_000 and
               f['frame'] >= warm['frame']+30, timeout=90)
        if mode == 3: s.down('Shift_L')
        s.down('w')
        try:
            first = s.wait(lambda f: ready(f) and f['keys']['w'])
            def traveled(f):
                return (f['pose']['walked_m']-first['pose']['walked_m'] if mode == 3 else
                        math.dist(f['camera_local'], first['camera_local'])*units)
            last = s.wait(lambda f: ready(f) and traveled(f) >= distance, timeout=90)
        finally:
            s.up('w')
            if mode == 3: s.up('Shift_L')
        s.wait(lambda f: ready(f) and f['observed_ns'] >= last['observed_ns']+1_000_000_000, timeout=60)
        s.close()
        moving = [f for f in s.frames if first['frame'] <= f['frame'] <= last['frame']]
        offsets = [math.dist(f['benchmark']['surface_target_eye_body'],
                   f['benchmark']['bodies'][0]['foliage']['plan_eye'])*scale for f in moving]
        memory = list(csv.DictReader(Path(str(trace)+'.memory.csv').open()))
        assert memory and all(r['context_status'] == 'uuid_verified' and
            r['context_uuid'] == memory[0]['context_uuid'] for r in memory), 'Native NVIDIA context required'
        assert all(f['paused'] and f['benchmark']['quality_scale'] == 1 and
            f['benchmark']['viewport'] == f['benchmark']['scene_size'] == size for f in moving)
        summary = {'view': view, 'mode': mode, 'viewport': size, 'distance_m': traveled(last),
            'wall_mps': traveled(last)/((last['observed_ns']-first['observed_ns'])/1e9),
            'max_target_offset_m': max(offsets), 'frames_outside_margin': sum(x > margin for x in offsets),
            'placement_margin_m': margin, 'context_uuid': memory[0]['context_uuid'],
            'grass_only_publications': last['publication']['grass_only_published']-first['publication']['grass_only_published'],
            'audit': validate(s.frames)}
        results.append(summary)
        write_json(out/'results.json', results)
        print(summary, flush=True)
        assert max(offsets) <= margin, summary
        assert last['publication']['interactive_failures'] == 0
        assert summary['grass_only_publications'] >= 2
    finally:
        s.abort()
