#!/usr/bin/env python3
"""Exercise real GLFW I/Tab input and inspect the panel border in an X11 window."""
import argparse
from pathlib import Path
import subprocess
import time

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--binary', type=Path, required=True)
args = p.parse_args()
root = Path(__file__).resolve().parents[2]

def command(*args):
    return subprocess.check_output([str(a) for a in args], stderr=subprocess.DEVNULL)

def wait_for(predicate):
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise AssertionError('Application exited unexpectedly')
        try:
            if predicate(): return
        except subprocess.CalledProcessError:
            pass
        time.sleep(0.1)
    raise AssertionError('Timed out waiting for window/panel state')

process = subprocess.Popen([str(args.binary.resolve()), '--config',
    'tests/fixtures/development/configs/scenarios/solar_system.json'], cwd=root,
    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    windows = []
    def find_window():
        windows[:] = command('xdotool', 'search', '--onlyvisible', '--pid', process.pid).splitlines()
        return bool(windows)
    wait_for(find_window)
    window = windows[0].decode()
    command('xdotool', 'windowfocus', '--sync', window)
    # Pixel (12,12) is the exact white border; (14,14) is the black fill.
    def panel_visible():
        png = command('import', '-window', window, 'png:-')
        rgb = subprocess.check_output(['convert', 'png:-', '-crop', '3x3+12+12',
                                      '-depth', '8', 'rgb:-'], input=png)
        return rgb[:3] == b'\xff\xff\xff' and rgb[-3:] == b'\0\0\0'
    wait_for(lambda: not panel_visible())
    command('xdotool', 'key', 'i')
    wait_for(panel_visible)
    # The toggle persists after key release and across a camera switch.
    command('xdotool', 'key', '3')
    wait_for(panel_visible)
    command('xdotool', 'keydown', 'i')
    wait_for(lambda: not panel_visible())
    time.sleep(1.5)  # X keyboard autorepeat must not toggle repeatedly.
    assert not panel_visible(), 'Holding I toggled the panel repeatedly'
    command('xdotool', 'keyup', 'i')
    command('xdotool', 'keydown', 'Tab')
    wait_for(panel_visible)
    command('xdotool', 'keyup', 'Tab')
    wait_for(lambda: not panel_visible())
    command('xdotool', 'key', 'i')
    wait_for(panel_visible)
    command('xdotool', 'key', 'Tab')
    wait_for(panel_visible)
    print('Validated I toggle/repeat, camera switch, Tab hold/release and border pixels')
finally:
    process.terminate()
    try: process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()
