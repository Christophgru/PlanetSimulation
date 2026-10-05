"""Create native flight starts from the public capture/replay contract."""
import copy
import json
from pathlib import Path
import subprocess
from NativeSession import write_json


def seeds(binary, root, output, scene):
    config = output / 'flight-scene.json'
    write_json(config, scene)
    capture = output / 'launch.png'
    command = [str(binary.resolve()), '--config', str(config), '--terrain-backend', 'compute',
               '--astronaut-capture', str(capture), '--render-size', '320', '180',
               '--benchmark-frames', '2', '--benchmark-step', '0',
               '--benchmark-character-step', '.04', '--benchmark-jump-frame', '0']
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
        # The fixture has no body rotation, so world-to-departure is translation.
        local = [a - b for a, b in zip(world, bodies[1]['position_m'])]
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
