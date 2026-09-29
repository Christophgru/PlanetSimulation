#!/usr/bin/env python3
"""Run under Xvfb after building PlanetSimulation and terrain_shadow_render_tests."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-dir', type=Path, required=True)
args = parser.parse_args()
build = args.build_dir.resolve()
for executable in (build / 'PlanetSimulation', build / 'tests/terrain_shadow_render_tests'):
    if not executable.is_file():
        parser.error(f'Build the required target first: {executable}')
out = build / 'readme-captures'
out.mkdir(exist_ok=True)
dest = ROOT / 'docs/screenshots'
metadata = ROOT / 'docs/captures'
replays = metadata / 'replay'
replays.mkdir(parents=True, exist_ok=True)
version = re.search(r'project\(PlanetSimulation VERSION ([\d.]+)', (ROOT / 'CMakeLists.txt').read_text())[1]
records = []

def run(command, name, cwd=ROOT):
    command = [str(x) for x in command]
    with (out / (name + '.log')).open('w') as log:
        subprocess.run(command, cwd=cwd, stdout=log, stderr=subprocess.STDOUT, check=True)
    return shlex.join(command)

def record(name, source, command):
    records.append((name, source, command, datetime.now(timezone.utc).isoformat(timespec='seconds')))

def capture(name, arguments):
    target = out / name
    command = run([build / 'PlanetSimulation', *arguments, '--surface-capture' if name != 'solar-view.png' and name != 'planet-orbit.png' else ('--render-test' if name == 'solar-view.png' else '--planet-render-test'), target], name)
    assert f'PlanetSimulation v{version} initialized' in (out / (name + '.log')).read_text()
    record(name, target, command)

scene = ['--config', 'configs/scenarios/solar_system.json']
capture('solar-view.png', ['--config', 'tests/fixtures/development/configs/scenarios/solar_system.json'])
capture('surface-view.png', [*scene, '--simulation-time', '20'])
capture('planet-orbit.png', [*scene, '--simulation-time', '3000'])
# Preserve the documented airless night controls, while using today's renderer.
for name in ('moonlit-night.png', 'moonless-night.png'):
    capture(name, ['--replay', str(replays / (name + '.json'))])
for name in ('refraction-extreme-on.png', 'refraction-extreme-off.png'):
    capture(name, ['--replay', str(replays / 'refraction' / (name + '.json'))])
capture('terrain-detail.png', ['--replay', str(replays / 'terrain' / 'terrain-detail.png.json')])
capture('grass-detail.png', ['--replay', str(replays / 'foliage' / 'grass-detail.png.json')])
capture('performance-overlay.png', [*scene, '--simulation-time', '20', '--render-size', '1280', '720', '--benchmark-frames', '20', '--benchmark-overlay'])
for group, mapping in (
    ('atmosphere', {'standard_air': 'atmosphere-day.png', 'sunset': 'atmosphere-sunset.png', 'mist': 'atmosphere-mist.png', 'heavy_dust': 'atmosphere-dust.png'}),
    ('twilight', {'twilight_reported': 'twilight.png'}),
):
    for case, name in mapping.items():
        case_out = out / case
        command = run(['python3', 'tests/lighting/render_lighting_scenarios.py', '--binary', build / 'PlanetSimulation', '--manifest', f'tests/scenarios/{group}/manifest.json', '--case', case, '--output-dir', case_out], case)
        record(name, case_out / (case + '.png'), command)
command = run(['ctest', '--test-dir', build, '--output-on-failure', '-R', '^TerrainShadowRenderIntegration$'], 'terrain-shadows')
record('terrain-shadows.png', build / 'terrain-shadow-test.png', command)
# Publish only after every capture and scenario check has succeeded.
rows = []
for name, source, command, timestamp in records:
    shutil.copy2(source, dest / name)
    sidecar = Path(str(source) + '.json')
    if sidecar.exists():
        replay_dir = replays / 'refraction' if name.startswith('refraction-extreme-') else replays
        if name == 'terrain-detail.png':
            replay_dir = replays / 'terrain'
        if name == 'grass-detail.png':
            replay_dir = replays / 'foliage'
        shutil.copy2(sidecar, replay_dir / (name + '.json'))
    rows.append({'image': name, 'version': version, 'generated_utc': timestamp,
                 'sha256': hashlib.sha256(source.read_bytes()).hexdigest(), 'command': command})
revision = subprocess.check_output(['git', '-c', f'safe.directory={ROOT}', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
source_hash = hashlib.sha256()
source_files = [ROOT / 'CMakeLists.txt']
for directory in ('src', 'shaders', 'configs', 'tests', 'scripts'):
    source_files.extend(p for p in (ROOT / directory).rglob('*') if p.is_file() and '__pycache__' not in p.parts)
for path in sorted(source_files):
    source_hash.update(str(path.relative_to(ROOT)).encode() + b'\0' + path.read_bytes() + b'\0')
manifest = {'source_sha256': source_hash.hexdigest(), 'version': version, 'base_revision': revision, 'source_state': 'working tree at capture time; see source_sha256',
            'renderer': subprocess.check_output(['glxinfo', '-B'], text=True), 'images': rows}
(metadata / 'generation.json').write_text(json.dumps(manifest, indent=2) + '\n')
log = ['# README image generation log', '', f'## Version {version} — {datetime.now(timezone.utc).date()} (UTC)', '',
       f'All {len(rows)} README images were regenerated with the current renderer. Earlier capture dates and versions were not recorded.', '',
       f'Base source revision: `{revision}` plus the working-tree changes identified by the source fingerprint.',
       'Build: RelWithDebInfo. Display: Xvfb. OpenGL: Mesa llvmpipe (software rendering).',
       'The performance panel reports this capture environment, not hardware GPU performance.', '',
       '| Image | Version | Last generated (UTC) |', '| --- | --- | --- |']
log += [f'| [{r["image"]}](../screenshots/{r["image"]}) | {version} | {r["generated_utc"]} |' for r in rows]
log += ['', 'Exact commands, image SHA-256 hashes and renderer details are in [generation.json](generation.json).',
        'Available [replay sidecars](replay/) preserve resolved scenes and cameras.', '',
        'The solar overview uses the frozen compact fixture; the surface and orbit gallery use the current working scene.',
        'Night images preserve their airless lighting controls. Atmosphere, shadow and twilight images use regression fixtures.', '',
       'To regenerate after building with `BUILD_TESTING=ON`:', '', '~~~bash',
        'cmake --build build --target PlanetSimulation terrain_shadow_render_tests',
        'LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a -s "-screen 0 1280x720x24" python3 scripts/generate_readme_images.py --build-dir build',
       '~~~', '', 'Regenerate after changes to rendering, shaders or pictured scenarios; the version alone does not prove freshness.',
        'Commit the images, sidecars and both generation records together.']
(metadata / 'GENERATION.md').write_text('\n'.join(log) + '\n')
print(f'Published {len(rows)} images for {version}', flush=True)
