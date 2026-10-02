#!/usr/bin/env python3
"""Exercise the actual astronaut in HDR, reflections, walking and exact replay."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--binary', type=Path, required=True)
p.add_argument('--output-dir', type=Path, required=True)
args = p.parse_args()
root = Path(__file__).resolve().parents[2]
out = args.output_dir.resolve()
out.mkdir(parents=True, exist_ok=True)
scene = json.loads((root / 'tests/scenarios/foliage/surface.json').read_text())
# Keep the atmosphere, terrain and water paths; isolate the character pixels.
scene['planets'][0]['foliage']['enabled'] = False
config = out / 'scene.json'
config.write_text(json.dumps(scene, indent=2) + '\n')

def capture(name, frames=1, walk=0, replay=None, extra=()):
    path = out / (name + '.png')
    command = [str(args.binary.resolve()), '--config', str(config), '--astronaut-capture', str(path),
               '--render-size', '640', '360', '--benchmark-frames', str(frames),
               '--benchmark-step', '0', '--benchmark-walk-step', str(walk)]
    command += list(extra)
    if replay:
        command += ['--replay', str(replay)]
    result = subprocess.run(command, cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (out / (name + '.log')).write_text(result.stdout)
    assert result.returncode == 0, result.stdout
    metadata = json.loads(Path(str(path) + '.json').read_text())
    assert metadata['render']['astronaut_pixels'] > 150, metadata['render']
    assert metadata['render']['terrain_pixels'] > 1000
    assert metadata['render']['camera_mode'] == 'third_person'
    assert all(f['reached'] for f in metadata['astronaut_pose']['feet']), metadata['astronaut_pose']
    return hashlib.sha256(path.read_bytes()).hexdigest(), metadata

idle, first = capture('idle')
walking, moved = capture('walking', frames=9, walk=.06)
assert walking != idle, 'Walking did not change the image'
assert moved['astronaut_pose']['root'] != first['astronaut_pose']['root']
assert any(f['progress'] < 1 for f in moved['astronaut_pose']['feet']), 'No lifted walking foot'
assert moved['astronaut_pose']['grass_trail'], 'Grounded walking left no persistent marks'
replayed, restored = capture('walking-replay', replay=out / 'walking.png.json')
assert replayed == walking, 'Replay did not restore the saved astronaut gait and camera'
assert restored['astronaut_pose'] == moved['astronaut_pose']
paused, _ = capture('paused', frames=3)
assert paused == idle, 'Stationary explicit-time captures must remain deterministic'
flight, flying = capture('jetpack', frames=8, extra=('--benchmark-character-step','.04',
    '--benchmark-jump-frame','0','--benchmark-boost-frame','3'))
assert flying['astronaut_pose']['airborne'] and flying['astronaut_pose']['boosting']
assert flying['astronaut_pose']['height_m'] > 1
assert flying['astronaut_pose']['grass_trail'] == [], 'Airborne motion left grass marks'
assert flight != idle
flight_replay, restored = capture('jetpack-replay', replay=out / 'jetpack.png.json')
assert flight_replay == flight, 'Airborne pose and bubble phase did not replay exactly'
assert restored['astronaut_pose'] == flying['astronaut_pose']
steered, steering = capture('jetpack-steering', frames=8, walk=.06,
    extra=('--benchmark-character-step','.04','--benchmark-jump-frame','0','--benchmark-boost-frame','3'))
pose = steering['astronaut_pose']
assert pose['arm_swing'] == 0 and pose['thrust_n'] > 0
assert sum(a*b for a,b in zip(pose['suit_up'],pose['up'])) < .999, 'Jetpack did not tilt to accelerate'
assert sum(v*v for v in pose['velocity_mps']) > 0
assert pose['flight_physics']['mass_kg'] == 100
assert pose['flight_physics']['air_pressure_pa'] > 0
steered_replay, saved = capture('jetpack-steering-replay', replay=out / 'jetpack-steering.png.json')
assert steered_replay == steered, 'Tilted thrust/velocity/camera did not replay exactly'
assert saved['astronaut_pose'] == pose
print('Validated visible HDR astronaut, grounded IK, walking, jump/jetpack bubbles and exact gait/flight replay')

# Exercise grass plus HDR/reflections and retain the entire trail in replay.
scene['planets'][0]['foliage']['enabled'] = True
scene['planets'][0]['foliage']['max_blades'] = 4096
config.write_text(json.dumps(scene, indent=2) + '\n')
trail_image, marks = capture('grass-trail', frames=9, walk=.06)
assert marks['astronaut_pose']['grass_trail']
trail_replay, replay_marks = capture('grass-trail-replay', replay=out / 'grass-trail.png.json')
assert trail_replay == trail_image, 'Persistent grass trail did not replay exactly'
assert replay_marks['astronaut_pose'] == marks['astronaut_pose']
print('Validated grounded trail recording, no airborne marks and exact grass/HDR/reflection replay')
