#!/usr/bin/env python3
"""Smoke-test a memory-limited interactive scene with the real GL renderer."""
import argparse
from pathlib import Path
import subprocess
import time

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--binary', type=Path, required=True)
args = p.parse_args()
root = Path(__file__).resolve().parents[2]
process = subprocess.Popen([str(args.binary.resolve()), '--config',
    'configs/scenarios/solar_system.json', '--simulation-time', '20',
    '--video-memory-mb', '32'], cwd=root, stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT, text=True)
try:
    deadline = time.monotonic() + 20
    window = None
    while time.monotonic() < deadline:
        if process.poll() is not None: raise AssertionError(process.stdout.read())
        found = subprocess.run(['xdotool','search','--onlyvisible','--pid',str(process.pid)],
                               capture_output=True, text=True)
        if found.returncode == 0:
            window = found.stdout.splitlines()[0]
            break
        time.sleep(.1)
    assert window, 'No interactive window'
    subprocess.check_call(['xdotool','windowfocus','--sync',window])
    subprocess.check_call(['xdotool','key','2'])
    # Wait for a completed GL frame; the software renderer can take seconds.
    image = None
    for _ in range(40):
        image = subprocess.check_output(['import','-window',window,'png:-'])
        mean = float(subprocess.check_output(['convert','png:-','-format','%[fx:mean]','info:'],input=image))
        if mean > .005: break
        time.sleep(.25)
    else: raise AssertionError('Scene stayed black')
    print('Rendered nonblack memory-limited interactive scene', flush=True)
finally:
    process.terminate()
    try: output,_ = process.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill(); output,_ = process.communicate()
assert 'Scene render resolution:' in output, output
assert '(100%)' not in output, output
assert 'Shader compilation error' not in output and 'framebuffer is incomplete' not in output, output
print([line for line in output.splitlines() if 'Scene render resolution:' in line][0])
