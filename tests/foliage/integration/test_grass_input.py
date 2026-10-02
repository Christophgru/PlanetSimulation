#!/usr/bin/env python3
"""Verify live wind continues under T pause and JSON speed zero freezes it."""
import argparse
from collections import Counter
import csv
import ctypes as C
import hashlib
import json
from pathlib import Path
import subprocess
import time

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--binary', type=Path, required=True)
p.add_argument('--output-dir', type=Path, required=True)
args = p.parse_args()
root = Path(__file__).resolve().parents[3]
out = args.output_dir.resolve()
out.mkdir(parents=True, exist_ok=True)
scene = json.loads((root / 'tests/scenarios/foliage/surface.json').read_text())
scene['planets'] = scene['planets'][:1]
planet = scene['planets'][0]
planet['color'] = [.2, .6, .1]
planet['surface_noise'] = []
planet['terrain_landscape'] = {'enabled': False}
planet['terrain_lod'].update(max_triangle_budget=10000, shoreline_edge_m=0, sink_depth_m=0)
planet['water'] = {'enabled': False}
planet['atmosphere'] = {'enabled': False}
planet['foliage'].update(max_blades=4096, draw_distance_m=30, wind_noise={'speed_multiplier': 4})
scene['lighting'].update(ambient_light=.2, auto_exposure={'enabled': False},
                         shadows={'enabled': False}, reflections_enabled=False)
scene['skybox']['enabled'] = False
scene['surface_camera'].update(altitude=.002, direction_ned=[1,0,.12])
scene['surface_camera'].pop('up_ned', None)
config = out / 'scene.json'
config.write_text(json.dumps(scene, indent=2) + '\n')
(out / 'animated-scene.json').write_text(config.read_text())

def command(*values):
    return subprocess.check_output([str(v) for v in values], stderr=subprocess.STDOUT)

def close_window(window):
    # Send GLFW's WM_DELETE_WINDOW event for a normal shutdown, so performance
    # traces flush. xdotool windowclose destroys the drawable directly.
    class Data(C.Union):
        _fields_ = [('longs', C.c_long * 5)]
    class Client(C.Structure):
        _fields_ = [('type', C.c_int), ('serial', C.c_ulong), ('send_event', C.c_int),
                    ('display', C.c_void_p), ('window', C.c_ulong), ('message_type', C.c_ulong),
                    ('format', C.c_int), ('data', Data)]
    class Event(C.Union):
        _fields_ = [('client', Client), ('padding', C.c_long * 24)]
    x = C.CDLL('libX11.so.6')
    x.XOpenDisplay.argtypes = [C.c_char_p]; x.XOpenDisplay.restype = C.c_void_p
    x.XInternAtom.argtypes = [C.c_void_p, C.c_char_p, C.c_int]; x.XInternAtom.restype = C.c_ulong
    x.XSendEvent.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_long, C.POINTER(Event)]
    x.XFlush.argtypes = x.XCloseDisplay.argtypes = [C.c_void_p]
    display = x.XOpenDisplay(None)
    assert display, 'Cannot open the X11 display'
    try:
        event = Event()
        event.client.type = 33  # ClientMessage, defined in X11/X.h.
        event.client.send_event = 1; event.client.display = display
        event.client.window = int(window); event.client.format = 32
        event.client.message_type = x.XInternAtom(display, b'WM_PROTOCOLS', 0)
        event.client.data.longs[0] = x.XInternAtom(display, b'WM_DELETE_WINDOW', 0)
        assert x.XSendEvent(display, int(window), 0, 0, C.byref(event))
        x.XFlush(display)
    finally:
        x.XCloseDisplay(display)

logpath = out / 'run.log'
with logpath.open('w') as log:
    process = subprocess.Popen([str(args.binary.resolve()), '--config', str(config),
        '--performance-trace', str(out / 'frames.csv')], cwd=root, stdout=log, stderr=subprocess.STDOUT)
    try:
        def wait_for(predicate, message):
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                assert process.poll() is None, logpath.read_text()
                if predicate(): return
                time.sleep(.1)
            raise AssertionError(message + '\n' + logpath.read_text())
        windows = []
        def find_window():
            found = subprocess.run(['xdotool','search','--onlyvisible','--pid',str(process.pid)], capture_output=True)
            if found.returncode == 0: windows[:] = found.stdout.decode().splitlines()
            return bool(windows)
        wait_for(find_window, 'No native GLFW window')
        window = windows[0]
        wait_for(lambda: 'Scene render resolution:' in logpath.read_text(), 'Startup did not finish')
        command('xdotool','windowfocus','--sync',window)
        command('xdotool','windowsize',window,480,270)
        command('xdotool','key','t')
        wait_for(lambda: 'Simulation paused' in logpath.read_text(), 'T did not pause the planet')
        command('xdotool','key','2')
        # Telemetry is emitted only after the wall-clock camera descent ends.
        # A second report also allows its first surface frame/terrain to finish.
        wait_for(lambda: logpath.read_text().count('surface_camera start value:') >= 2,
                 'Surface camera did not finish its transition and warm up')
        command('xdotool','key','Escape')

        def snapshot(name):
            png = command('import','-window',window,'png:-')
            (out / (name + '.png')).write_bytes(png)
            return subprocess.check_output(['convert','png:-','-depth','8','rgb:-'], input=png)
        def changed(a,b):
            assert len(a) == len(b), 'The native render resolution changed during a comparison'
            return sum(x!=y for x,y in zip(a,b))

        # No HUD, camera input, orbit/spin, atmosphere or water animation can
        # account for the pixels changing in this controlled scene.
        animated_a = snapshot('animated-a')
        green_pixels = sum(g > r * 1.1 and g > b * 1.1
                           for r,g,b in zip(animated_a[::3], animated_a[1::3], animated_a[2::3]))
        assert green_pixels > len(animated_a) // 15, 'Wind baseline is not a completed grassy surface view'
        time.sleep(1)
        animated_b = snapshot('animated-b')
        animated_delta = changed(animated_a, animated_b)
        assert animated_delta > 200, 'Grass stopped moving when T paused planetary motion'
        command('xdotool','key','u'); command('xdotool','key','y')
        wait_for(lambda: logpath.read_text().count('Simulation speed:') >= 2,
                 'Both speed controls were not processed')

        def reload_scene(name):
            count = logpath.read_text().count('Reloaded ')
            config.write_text(json.dumps(scene, indent=2) + '\n')
            wait_for(lambda: logpath.read_text().count('Reloaded ') > count, 'Scene did not reload')
            time.sleep(1)
            (out / (name + '.json')).write_text(json.dumps(scene, indent=2) + '\n')
        planet['foliage']['wind_noise']['speed_multiplier'] = 0
        reload_scene('frozen-scene')
        frozen_a = snapshot('frozen-a'); time.sleep(1); frozen_b = snapshot('frozen-b')
        assert frozen_a == frozen_b, 'Zero JSON wind speed did not freeze the rendered grass'
        planet['foliage']['enabled'] = False
        reload_scene('bare-scene')
        bare = snapshot('bare')
        assert changed(frozen_b, bare) > 200, 'The wind check had no visible grass'
        close_window(window)
        assert process.wait(timeout=10) == 0, logpath.read_text()
    finally:
        if process.poll() is None:
            process.terminate()
            try: process.wait(timeout=5)
            except subprocess.TimeoutExpired: process.kill(); process.wait()

rows = list(csv.DictReader((out / 'frames.csv').open()))
assert rows, 'No performance trace'
# Reload preserves pause and resets the epoch to the configured start time.
# Both frozen and bare phases must contain completed frames at that epoch.
epoch = scene['surface_camera']['simulation_time_seconds']
late = [r for r in rows if float(r['simulation_s']) == epoch]
assert len(late) > 3, 'The paused clock did not remain fixed after reload'
initial_times = Counter(r['simulation_s'] for r in rows if float(r['simulation_s']) > epoch)
assert initial_times, 'No pre-reload frames were recorded'
paused_time, paused_frames = initial_times.most_common(1)[0]
assert paused_frames > 3, 'Orbital time advanced while paused wind was rendered'
text = logpath.read_text()
assert 'Shader compilation error' not in text and 'OpenGL error' not in text, text
evidence = {'animated_changed_channels': animated_delta, 'frozen_changed_channels': 0,
    'animated_paused_frames': paused_frames, 'animated_paused_simulation_s': float(paused_time),
    'paused_reload_frames': len(late), 'paused_reload_simulation_s': epoch,
    'png_sha256': {p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in out.glob('*.png')}}
(out / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
print('Validated T-paused live wind, orbital speed keys, zero-speed wind freeze, visible grass and paused reload')
