#!/usr/bin/env python3
"""Capture a dense production-start view and audit flare/replay/column costs.

Run under Xvfb or an existing display. Publishes only into --output-dir;
gallery publication remains a separate review step.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tests/quality'))
from read_png import read_png


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--width', type=int, default=1920)
    parser.add_argument('--height', type=int, default=1080)
    parser.add_argument('--resume', action='store_true', help='Reuse completed captures after validation')
    parser.add_argument('--capture', choices=('baseline','dense','no-flare','replay','narrow'),
                        help='Run one capture; use --resume without this option for the final audit')
    args = parser.parse_args()
    out = args.output_dir.resolve()
    for name in ('images', 'replay', 'logs', 'validation'):
        (out / name).mkdir(parents=True, exist_ok=True)
    production_path = ROOT / 'configs/scenarios/solar_system.json'
    production = json.loads(production_path.read_text())
    scene = json.loads(json.dumps(production))
    scene['planets'][scene['surface_camera']['planet_index']]['foliage'].update(
        max_blades=12000000, gaussian_sigma_fraction=.05,
        max_candidates_per_triangle=65536)
    dense_scene = out / 'scene.json'
    dense_scene.write_text(json.dumps(scene, indent=2) + '\n')
    rows = {}

    def capture(name, config=dense_scene, flags=(), replay=None, width=None):
        image = out / 'images' / (name + '.png')
        command = [str(args.binary.resolve()), '--config', str(config),
                   '--offline-render', str(image), '--render-size',
                   str(width or args.width), str(args.height), *flags]
        if replay:
            command.extend(['--replay', str(replay)])
        started = datetime.now(timezone.utc).isoformat(timespec='seconds')
        saved_path = out / 'replay' / (name + '.png.json')
        if args.resume and saved_path.exists() and image.exists():
            started = datetime.fromtimestamp(image.stat().st_mtime, timezone.utc).isoformat(timespec='seconds')
        else:
            with (out / 'logs' / (name + '.log')).open('w') as log:
                subprocess.run(command, cwd=ROOT, stdout=log,
                               stderr=subprocess.STDOUT, check=True)
            Path(str(image) + '.json').replace(saved_path)
        log_text = (out / 'logs' / (name + '.log')).read_text()
        assert 'OpenGL error' not in log_text and 'completed successfully' in log_text
        saved = json.loads(saved_path.read_text())
        if replay:
            original = json.loads(replay.read_text())
            expected = original['scenario']
            expected['surface_camera'] = original['surface_camera']
        else:
            expected = json.loads(config.read_text())
        assert saved['scenario'] == expected, 'Cached scene differs; rerun without --resume'
        assert saved['render']['offline']['lens_flare'] == ('--no-lens-flare' not in flags)
        assert saved['render']['width'] == (width or args.width)
        assert saved['render']['height'] == args.height
        # Keep the exact production location, orientation, clearance and time.
        for field in ('latitude_deg', 'longitude_deg', 'altitude', 'planet_index',
                      'simulation_time_seconds', 'direction_ned', 'fov'):
            assert saved['surface_camera'][field] == production['surface_camera'][field], field
        assert saved['render']['foliage_gpu_count_view'] == 'main'
        rows[name] = {'command': shlex.join(command), 'generated_utc': started,
                      'png_sha256': sha(image), 'render': saved['render']}
        print('PASS', name, saved['render']['foliage_gpu_drawn_blades'],
              'main-view blades;', saved['render']['foliage_gpu_working_bytes'],
              'queue bytes', flush=True)
        return saved

    saved = {}
    for name, kwargs in (
        ('baseline', {'config': production_path}), ('dense', {}),
        ('no-flare', {'flags': ['--no-lens-flare']}),
        ('replay', {'replay': out / 'replay/dense.png.json'}),
        ('narrow', {'width': max(64, args.width // 10)})):
        if args.capture and name != args.capture:
            continue
        saved[name] = capture(name, **kwargs)
    if args.capture:
        return
    baseline, dense, disabled, restored, narrow = [saved[name] for name in
        ('baseline', 'dense', 'no-flare', 'replay', 'narrow')]
    r, b = dense['render'], baseline['render']
    assert r['foliage_candidates'] > b['foliage_candidates']
    assert r['foliage_gpu_drawn_blades'] > b['foliage_gpu_drawn_blades']
    assert r['lens_flare_visible_sun_pixels'] > 100 and r['lens_flare_strength'] > 0
    assert disabled['render']['lens_flare_strength'] == 0
    assert rows['dense']['png_sha256'] == rows['replay']['png_sha256']
    assert restored['render']['effective_foliage_distance_m'] == r['effective_foliage_distance_m']
    for field in ('foliage_candidates', 'foliage_gpu_working_bytes', 'body_mesh_triangles'):
        assert narrow['render'][field] == r[field], field
    w, h, c, on = read_png(out / 'images/dense.png')
    fw, fh, fc, off = read_png(out / 'images/no-flare.png')
    assert (w, h, c) == (fw, fh, fc)
    changed = sum(max(on[p*c+i]-off[p*c+i] for i in range(3)) >= 8
                  for p in range(w*h))
    assert changed > w*h*.001, changed
    report = {'date_utc': datetime.now(timezone.utc).isoformat(timespec='seconds'),
              'production_config_sha256': sha(production_path),
              'base_revision': subprocess.check_output(['git', '-c', f'safe.directory={ROOT}',
                  'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
              'scope': 'Exact production start camera; explicit dense foliage study profile',
              'flare_pixels_above_8': changed,
              'column_evaluation': 'A central 1/10-width view reduces submitted draws but retains '
                  'the same candidate queues and terrain meshes. It cannot load more geometry '
                  'without frustum-aware allocation. Per-strip highlight metering and reflection '
                  'projection also require a shared full-frame solution before assembly.',
              'captures': rows}
    source = hashlib.sha256()
    paths = [ROOT / 'CMakeLists.txt']
    for name in ('src', 'shaders', 'configs', 'tests', 'scripts'):
        paths.extend(p for p in (ROOT / name).rglob('*')
                     if p.is_file() and '__pycache__' not in p.parts)
    for p in sorted(paths):
        source.update(str(p.relative_to(ROOT)).encode() + b'\0' + p.read_bytes() + b'\0')
    report['source_sha256'] = source.hexdigest()
    (out / 'validation/results.json').write_text(json.dumps(report, indent=2) + '\n')
    print('PASS production camera, denser main-view foliage, apparent gated flare, '
          'exact replay and measured column-allocation limits', flush=True)


if __name__ == '__main__':
    main()
