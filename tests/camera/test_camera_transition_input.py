#!/usr/bin/env python3
"""Enter the surface camera through real GLFW input and inspect the intermediate frame."""
import argparse
from pathlib import Path
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--binary', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
process = subprocess.Popen([str(args.binary.resolve()), '--config',
    'tests/fixtures/development/configs/scenarios/solar_system.json'], cwd=root,
    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    window = None
    for _ in range(200):
        if process.poll() is not None:
            raise AssertionError('Application exited before opening its window')
        found = subprocess.run(['xdotool','search','--onlyvisible','--pid',str(process.pid)],
                               capture_output=True)
        if found.returncode == 0 and found.stdout.strip():
            window = found.stdout.splitlines()[0].decode()
            break
        time.sleep(0.1)
    assert window, 'Window did not open'
    subprocess.run(['xdotool','windowfocus','--sync',window], check=True)
    subprocess.run(['xdotool','key','t'], check=True)
    time.sleep(0.5)
    def pixels():
        png = subprocess.check_output(['import','-window',window,'png:-'])
        return subprocess.check_output(['convert','png:-','-depth','8','rgb:-'],input=png)
    orbit = pixels()
    subprocess.run(['xdotool','key','2'], check=True)
    time.sleep(0.35)
    middle = pixels()
    time.sleep(1.2)
    surface = pixels()
    assert len(orbit) == len(middle) == len(surface)
    early = sum(a != b for a,b in zip(orbit,middle))
    remaining = sum(a != b for a,b in zip(middle,surface))
    assert early > 3000, f'No visible camera movement during transition: {early}'
    assert remaining > 3000, f'Camera jumped directly to final surface pose: {remaining}'
    print(f'Validated intermediate orbital-to-surface frame ({early} and {remaining} changed channels)')
finally:
    process.terminate()
    try: process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill(); process.wait()
