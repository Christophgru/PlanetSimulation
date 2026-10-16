"""Moving centimeter detail: shared consumers, parent replay and supported fallbacks."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'quality'))
from read_png import read_png

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--binary', type=Path, required=True)
parser.add_argument('--output-dir', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
out = args.output_dir.resolve()
out.mkdir(parents=True, exist_ok=True)
scene = json.loads((root / 'tests/scenarios/foliage/surface.json').read_text())
for planet in scene['planets']:
    planet.setdefault('atmosphere', {})['enabled'] = False
    planet.setdefault('water', {})['enabled'] = False
    planet.setdefault('terrain_lod', {})['max_triangle_budget'] = 10000
    planet.setdefault('foliage', {})['max_blades'] = 4096
planet = scene['planets'][0]
planet['surface_noise'] = [{'amplitude_m': .01, 'wavelength_m': .25, 'octaves': 1, 'seed': -71}]
planet['terrain_landscape']['elevation_offset_m'] = 1.5
planet['terrain_lod'].update(relief_sinking=True, geometric_error_m=.05,
    local_detail_streaming=True, local_detail_radius_m=.25, local_edge_m=.05,
    local_error_m=.01, local_transition_m=.5, max_triangle_budget=30000, shoreline_edge_m=0)
config = out / 'scene.json'
config.write_text(json.dumps(scene))

def run(name, flags=(), replay=None, env=None, fail=False):
    image = out / (name + '.png')
    image.unlink(missing_ok=True)
    command = [str(args.binary.resolve()), '--config', str(config), '--surface-capture', str(image),
        '--render-size', '480', '270', *flags]
    if replay:
        command += ['--replay', str(replay)]
    result = subprocess.run(command, cwd=root, env={**os.environ, **(env or {})},
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=120)
    (out / (name + '.log')).write_text(result.stdout)
    if fail:
        assert result.returncode != 0 and not image.exists(), result.stdout
        assert 'initialized' not in result.stdout
        return result.stdout
    assert result.returncode == 0, result.stdout
    data = json.loads(Path(str(image) + '.json').read_text())
    contract = data['render']['terrain_contract']
    assert contract['topology_version'] == 4 and contract['local_detail_triangles'] > 0
    assert not contract['local_detail_limited'] and contract['local_max_edge_m'] <= .05
    assert len(data['render']['terrain_parent_eyes_world_units']) == len(scene['planets'])
    assert data['render']['terrain_pixels'] > 100
    return image, data

moving, state = run('moving', ('--benchmark-frames', '8', '--benchmark-step', '.016666666667',
    '--benchmark-walk-step', '.06'))
replay, saved = run('replay', replay=Path(str(moving) + '.json'))
assert moving.read_bytes() == replay.read_bytes()
assert state['render']['terrain_contract']['topology_fingerprint'] == saved['render']['terrain_contract']['topology_fingerprint']
cpu, cpu_state = run('cpu', ('--terrain-backend', 'cpu'), Path(str(moving) + '.json'))
legacy, legacy_state = run('gl33', ('--terrain-backend', 'cpu'), Path(str(moving) + '.json'),
    {'LIBGL_ALWAYS_SOFTWARE': '1', 'MESA_GL_VERSION_OVERRIDE': '3.3', 'MESA_GLSL_VERSION_OVERRIDE': '330'})
assert cpu.read_bytes() == legacy.read_bytes()
aw, ah, ac, ap = read_png(moving)
bw, bh, bc, bp = read_png(cpu)
assert (aw, ah, ac) == (bw, bh, bc)
mean = sum(abs(a-b) for a, b in zip(ap, bp)) / len(ap)
assert mean < .1, mean
bad = json.loads(Path(str(moving) + '.json').read_text())
bad['render']['terrain_parent_eyes_world_units'][0] = [0, 0, 0]
invalid = out / 'invalid.json'
invalid.write_text(json.dumps(bad))
assert 'Terrain planning anchors' in run('invalid', replay=invalid, fail=True)
(out / 'results.json').write_text(json.dumps({'moving_replay_exact': True,
    'cpu_gl33_exact': True, 'cpu_gpu_mean_channel_error': mean,
    'contract': state['render']['terrain_contract']}, indent=2) + '\n')
print('PASS moving terrain v4, exact parent replay, CPU/GL 3.3, corrupt anchor rejection')
