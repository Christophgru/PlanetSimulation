#!/usr/bin/env python3
"""Retain an identical-pose grass trail/control pair from the real renderer."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--binary', type=Path, required=True)
parser.add_argument('--output-dir', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
out = args.output_dir.resolve()
out.mkdir(parents=True, exist_ok=True)
scene = json.loads((root / 'tests/scenarios/foliage/surface.json').read_text())
scene['planets'][0]['foliage'].update(height_m=.8, draw_distance_m=12, max_blades=100000)
config = out / 'scene.json'
config.write_text(json.dumps(scene, indent=2) + '\n')

def capture(name, extra):
    target = out / (name + '.png')
    command = [str(args.binary.resolve()), '--config', str(config), '--astronaut-capture',
               str(target), '--render-size', '960', '540', '--benchmark-step', '0', *extra]
    result = subprocess.run(command, cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (out / (name + '.log')).write_text(result.stdout)
    assert result.returncode == 0, result.stdout
    metadata = json.loads(Path(str(target) + '.json').read_text())
    assert metadata['render']['astronaut_pixels'] > 150
    assert metadata['render']['foliage_gpu_drawn_blades'] > 0
    return hashlib.sha256(target.read_bytes()).hexdigest(), metadata, command

walk_hash, walked, command = capture('walking', ['--benchmark-frames', '21', '--benchmark-walk-step', '.3'])
# Resolve the picture through a fresh replay before comparing deformation,
# so both controls use the same terrain/grass planning state and actor pose.
pressed_hash, pressed, pressed_command = capture('pressed', ['--replay', str(out / 'walking.png.json')])
assert pressed_hash == walk_hash, 'Walking trail capture did not replay exactly'
assert pressed['astronaut_pose']['grass_trail']
control = json.loads(json.dumps(pressed))
control['astronaut_pose']['grass_trail'] = []
control_path = out / 'control-input.json'
control_path.write_text(json.dumps(control, indent=2) + '\n')
control_hash, bare, control_command = capture('control', ['--replay', str(control_path)])
assert control_hash != pressed_hash, 'Trail did not change the actual grass image'
assert bare['astronaut_pose']['grass_trail'] == []
for key, value in pressed['astronaut_pose'].items():
    if key != 'grass_trail':
        assert bare['astronaut_pose'][key] == value, key
replay_hash, replay, replay_command = capture('pressed-replay', ['--replay', str(out / 'pressed.png.json')])
assert replay_hash == pressed_hash
assert replay['astronaut_pose'] == pressed['astronaut_pose']
evidence = {
    'commands': [command, pressed_command, control_command, replay_command],
    'images': {'walking': walk_hash, 'pressed': pressed_hash, 'control': control_hash, 'pressed-replay': replay_hash},
    'walking_replays_exactly': walk_hash == pressed_hash,
    'segments': len(pressed['astronaut_pose']['grass_trail']),
    'walked_m': pressed['astronaut_pose']['walked_m'],
    'visible_blades': pressed['render']['foliage_gpu_drawn_blades'],
    'renderer': pressed['render']['renderer'],
    'normal_speed_mps': 6, 'sprint_speed_mps': 12,
}
(out / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
print(json.dumps(evidence, indent=2))
