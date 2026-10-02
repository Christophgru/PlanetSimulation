#!/usr/bin/env python3
"""Select camera 4, walk while paused, switch views and reload via real GLFW input."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--binary', type=Path, required=True)
args = p.parse_args()
root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='planet-astronaut-input-') as directory:
    directory = Path(directory)
    scene = json.loads((root / 'tests/fixtures/development/configs/scenarios/solar_system.json').read_text())
    for planet in scene['planets']:
        planet['surface_noise'] = []
        planet['terrain_landscape'] = {'enabled': False}
        planet['atmosphere'] = {'enabled': False}
        planet['water'] = {'enabled': False}
        planet['foliage'] = {'enabled': False}
        planet.setdefault('terrain_lod', {})['max_triangle_budget'] = 10000
        planet['rotation'] = {'period_seconds': 0}
        planet['mass_kg'] = 9.81 * (planet['radius'] * 1000)**2 / 6.67430e-11
    scene['skybox']['enabled'] = False
    scene['surface_camera']['direction_ned'] = [1,0,0]
    scene['surface_camera'].pop('up_ned', None)
    config = directory / 'scene.json'
    config.write_text(json.dumps(scene))
    with (directory / 'run.log').open('w') as log:
        process = subprocess.Popen([str(args.binary.resolve()), '--config', str(config)],
                                   cwd=root, stdout=log, stderr=subprocess.STDOUT)
        try:
            window = None
            for _ in range(200):
                assert process.poll() is None, (directory / 'run.log').read_text()
                found = subprocess.run(['xdotool','search','--onlyvisible','--pid',str(process.pid)],capture_output=True)
                if found.returncode == 0 and found.stdout.strip():
                    window = found.stdout.splitlines()[0].decode(); break
                time.sleep(.1)
            assert window, 'No renderer window'
            # The native window is visible before shader/scene initialization
            # and callback binding finish. Wait for the interactive loop.
            for _ in range(200):
                assert process.poll() is None, (directory / 'run.log').read_text()
                if 'Scene render resolution:' in (directory / 'run.log').read_text(): break
                time.sleep(.1)
            else: raise AssertionError((directory / 'run.log').read_text())
            subprocess.run(['xdotool','windowfocus','--sync',window],check=True)
            subprocess.run(['xdotool','windowsize',window,'640','360'],check=True)
            def key(value):
                subprocess.run(['xdotool','key',value],check=True)
            def wait_for_input(value, count=1):
                deadline = time.monotonic() + 8
                while time.monotonic() < deadline:
                    text = (directory / 'run.log').read_text()
                    assert process.poll() is None, text
                    if text.count(value) >= count: return
                    time.sleep(.05)
                raise AssertionError((directory / 'run.log').read_text())
            def pixels():
                png = subprocess.check_output(['import','-window',window,'png:-'])
                (directory / 'last-frame.png').write_bytes(png)
                return subprocess.check_output(['convert','png:-','-depth','8','rgb:-'],input=png)
            def changed_frame(before, minimum, message):
                # A native frame can take longer than a fixed wall-time sleep.
                # Observe the submitted input and its rendered result instead.
                deadline = time.monotonic() + 8
                while time.monotonic() < deadline:
                    after = pixels()
                    if sum(a!=b for a,b in zip(before,after)) > minimum: return after
                    assert process.poll() is None, (directory / 'run.log').read_text()
                    time.sleep(.1)
                raise AssertionError(message + '\n' + (directory / 'run.log').read_text())
            key('t'); key('4'); time.sleep(1.5)
            idle = pixels()
            assert 'Camera 4: following the astronaut' in (directory / 'run.log').read_text(), (directory / 'run.log').read_text()
            key('Escape')
            # WASD remains available after releasing the pointer.
            subprocess.run(['xdotool','keydown','w'],check=True)
            time.sleep(.45)
            subprocess.run(['xdotool','keyup','w'],check=True)
            time.sleep(.5)
            moved = pixels()
            assert sum(a!=b for a,b in zip(idle,moved)) > 500, 'Paused astronaut did not walk'
            key('space'); wait_for_input('Astronaut Space:', 1); time.sleep(.15)
            subprocess.run(['xdotool','keydown','space'],check=True)
            wait_for_input('Astronaut Space:', 2)
            boosted = changed_frame(moved, 500, 'Jump and jetpack did not change the frame')
            subprocess.run(['xdotool','keyup','space'],check=True)
            key('2'); time.sleep(.4)
            surface = pixels()
            assert sum(a!=b for a,b in zip(surface,moved)) > 1000, 'Camera 2 did not replace chase view'
            key('4'); time.sleep(.4); key('r'); time.sleep(1)
            assert process.poll() is None, (directory / 'run.log').read_text()
            text = (directory / 'run.log').read_text()
            assert 'Reloaded ' in text, text
            assert 'OpenGL error' not in text and 'Shader compilation error' not in text, text
            assert text.count('Astronaut Space:') >= 2, text
            print('Validated camera-4 input, paused walking/jump/jetpack, cursor release, switching and reload')
        except Exception:
            shutil.copytree(directory, args.binary.resolve().parent / 'astronaut-input-failure', dirs_exist_ok=True)
            raise
        finally:
            process.terminate()
            try: process.wait(timeout=5)
            except subprocess.TimeoutExpired: process.kill(); process.wait()
