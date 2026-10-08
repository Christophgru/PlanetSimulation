"""Create native flight starts from the public capture/replay contract."""
import copy
import json
import math
from pathlib import Path
import subprocess
from NativeSession import write_json


def seeds(binary, root, output, scene, boosted_launch=False):
    config = output / 'flight-scene.json'
    write_json(config, scene)
    capture = output / 'launch.png'
    command = [str(binary.resolve()), '--config', str(config), '--terrain-backend', 'compute',
               '--astronaut-capture', str(capture), '--render-size', '320', '180',
               '--benchmark-frames', '2', '--benchmark-step', '0',
               '--benchmark-character-step', '.04', '--benchmark-jump-frame', '0']
    if boosted_launch:
        # Dense production foliage can obscure the initial jump. Lift the actor
        # using public benchmark controls; keep visibility validation intact.
        command[command.index('--benchmark-frames')+1] = '10'
        command[command.index('--benchmark-character-step')+1] = '.1'
        command += ['--benchmark-boost-frame', '1']
    result = subprocess.run(command, cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=60)
    (output / 'launch.log').write_text(result.stdout)
    assert result.returncode == 0, result.stdout
    launch = json.loads(Path(str(capture) + '.json').read_text())
    bodies = {body['index']: body for body in launch['astronaut_pose']['navigation']['gravity_bodies']}
    paths = {}
    for name, body, height in [('space', 1, 4), ('moon', 2, 1.5)]:
        world = [c + delta for c, delta in zip(bodies[body]['position_m'], [0, 0, height * bodies[body]['radius_m']])]
        data = copy.deepcopy(launch)
        pose = data['astronaut_pose']
        nav = pose['navigation']
        nav.update({'position_m': world, 'velocity_mps': [0, 0, 0], 'outer_space': True, 'reference_body': 1})
        # Inverse of the public body's Rx(tilt)*Rz(phase+spin) orientation.
        # Zero-rotation fixtures retain the original translation-only result.
        x, y, z = [a - b for a, b in zip(world, bodies[1]['position_m'])]
        rotation = scene['planets'][0].get('rotation', {})
        tilt = math.radians(rotation.get('axial_tilt_deg', 0))
        period = rotation.get('period_seconds', 0)
        seconds = launch['surface_camera']['simulation_time_seconds']
        angle = math.radians(rotation.get('phase_deg', 0)) + (2*math.pi*math.remainder(seconds,abs(period))/period if period else 0)
        y, z = math.cos(tilt)*y+math.sin(tilt)*z, -math.sin(tilt)*y+math.cos(tilt)*z
        local = [math.cos(angle)*x+math.sin(angle)*y, -math.sin(angle)*x+math.cos(angle)*y, z]
        delta = [a - b for a, b in zip(local, pose['root'])]
        pose.update({'root': local, 'velocity_mps': [0, 0, 0], 'boosting': False, 'jetpack_armed': False, 'thrust_n': 0})
        for foot in pose['feet']:
            for key in ('position', 'start', 'target', 'hip', 'knee', 'ankle'):
                foot[key] = [a + b for a, b in zip(foot[key], delta)]
        for key in ('grass_plan_eye', 'terrain_plan_eyes_world_units', 'terrain_face_zones'):
            pose.pop(key, None)
        path = output / f'{name}-input.json'
        write_json(path, data)
        paths[name] = (path, world)
    return paths, launch
