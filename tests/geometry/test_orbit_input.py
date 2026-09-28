#!/usr/bin/env python3
"""Toggle real O input and check rendered paths/labels, including planet-camera hiding."""
import argparse
from pathlib import Path
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--binary', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]

def command(*argv):
    return subprocess.check_output([str(a) for a in argv], stderr=subprocess.DEVNULL)

process = subprocess.Popen([str(args.binary.resolve()), '--config',
    'tests/fixtures/development/configs/scenarios/solar_system.json'], cwd=root,
    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    deadline = time.monotonic() + 20
    window = None
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise AssertionError('Application exited before opening its window')
        try:
            matches = command('xdotool', 'search', '--onlyvisible', '--pid', process.pid).splitlines()
        except subprocess.CalledProcessError:
            matches = []
        if matches:
            window = matches[0].decode()
            break
        time.sleep(0.1)
    assert window, 'Window did not open'
    command('xdotool', 'windowfocus', '--sync', window)
    command('xdotool', 'key', 't')
    time.sleep(0.8)
    def pixels():
        png = command('import', '-window', window, 'png:-')
        return subprocess.check_output(['convert', 'png:-', '-depth', '8', 'rgb:-'], input=png)
    before = pixels()
    command('xdotool', 'key', 'o')
    time.sleep(0.8)
    shown = pixels()
    assert len(before) == len(shown)
    changed = sum(a != b for a, b in zip(before, shown))
    assert changed > 3000, f'Orbit toggle changed only {changed} color channels'
    command('xdotool', 'key', 'o')
    time.sleep(0.8)
    hidden = pixels()
    remaining = sum(a != b for a, b in zip(before, hidden))
    assert remaining < changed / 3, f'Orbits remained after toggling off: {remaining} versus {changed}'
    command('xdotool', 'key', 'o')
    command('xdotool', 'key', '3')
    time.sleep(0.8)
    planetCamera = pixels()
    command('xdotool', 'key', 'o')
    time.sleep(0.8)
    planetCameraOff = pixels()
    cameraDifference = sum(a != b for a, b in zip(planetCamera, planetCameraOff))
    assert cameraDifference < changed / 3, f'Orbit display appeared in planet camera: {cameraDifference}'
    print(f'Validated O paths/labels and planet-camera hiding ({changed} changed channels)')
finally:
    process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()
