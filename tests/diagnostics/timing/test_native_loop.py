#!/usr/bin/env python3
"""Frame-linked full native wall receipts on real GLFW loading/render/wait paths."""
import argparse
import csv
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tests/app/terrain/native'))
from NativeSession import Session, validate, write_json
sys.path.insert(0, str(root / "tests/diagnostics/memory"))
from MemoryTrace import validate_memory

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--probe', type=Path, required=True)
p.add_argument('--output-dir', type=Path, required=True)
args = p.parse_args()
out = args.output_dir.resolve()
out.mkdir(parents=True, exist_ok=True)
scene = json.loads((root / 'tests/scenarios/foliage/surface.json').read_text())
for planet in scene['planets']:
    planet['terrain_lod'] = {**planet.get('terrain_lod', {}), 'max_triangle_budget': 10000}
    planet['atmosphere'] = {'enabled': False}
scene['lighting']['shadows']['resolution'] = 256
scene['skybox']['enabled'] = False
scene['planets'][0]['foliage']['max_blades'] = 128
config = out / 'scene.json'
write_json(config, scene)
results = {}
for backend in ('cpu', 'compute'):
    output = out / backend
    output.mkdir(exist_ok=True)
    trace = output / 'performance.csv'
    flags = ['--config', str(config), '--terrain-backend', backend, '--performance-trace', str(trace)]
    delays = {'poll_delay_ms': 15, 'present_delay_ms': 12}
    session = Session(args.probe, root, output, flags, control={**delays, **({'delay': True} if backend == 'compute' else {})})
    try:
        session.focus()
        if backend == 'compute':
            session.wait(lambda f: f['publication']['loading'] and f['gl']['polls'] > 0)
            session.key('4')
            session.wait(lambda f: f['mode'] == 3 and f['publication']['loading'])
            session.control(delays)
        session.wait(lambda f: not f['publication'].get('loading', False))
        session.control({**delays, 'minimized': True})
        minimized = session.wait(lambda f: f['waiting'])
        session.wait(lambda f: f['waiting'] and f['frame'] > minimized['frame'] + 3)
        session.control(delays)
        resumed = session.wait(lambda f: not f['waiting'])
        session.wait(lambda f: not f['waiting'] and f['frame'] > resumed['frame'] + 3)
        session.close()
        native = list(csv.DictReader(Path(str(trace) + '.native-loop.csv').open()))
        frames = {int(f['frame']): f for f in csv.DictReader(trace.open())}
        observed = {f['profile_frame']: f for f in session.frames}
        assert len(native) == len(frames) == len(observed), (len(native), len(frames), len(observed))
        assert [int(f['frame']) for f in native] == list(range(len(native)))
        counts = {}
        for receipt in native:
            number = int(receipt['frame'])
            frame = frames[number]
            probe = observed[number]
            wall, poll, present, wait = (float(receipt[k]) for k in ('wall_ms', 'event_poll_ms', 'presentation_ms', 'event_wait_ms'))
            assert wall >= float(frame['frame_ms']) - 1e-5
            assert wall >= poll + present + wait - 1e-5
            if probe['controls'].get('poll_delay_ms') == 15:
                assert poll >= 14, receipt
            outcome = receipt['outcome']
            counts[outcome] = counts.get(outcome, 0) + 1
            if probe['waiting']:
                assert outcome == 'minimized' and present == 0 and wait >= 0, receipt
            else:
                assert outcome in ('loading', 'rendered') and present >= 0 and wait == 0, receipt
                if probe['controls'].get('present_delay_ms') == 12:
                    assert present >= 11, receipt
                assert (outcome == 'loading') == bool(probe['publication'].get('loading', False)), (receipt, probe)
        assert counts['minimized'] >= 4 and counts['rendered'] >= 4, counts
        if backend == 'compute':
            assert counts['loading'] >= 2, counts
        publications = list(csv.DictReader(Path(str(trace) + '.publications.csv').open()))
        published = [row for row in publications if row['outcome'] == 'published']
        assert published and len({row['attempt'] for row in publications}) == len(publications)
        assert not any(row['outcome'] == 'trace_overflow' for row in publications)
        for row in publications:
            if row['outcome'] != 'published':
                assert not row['publication_ms'] and not row['publication_frames'], row
                continue
            number = int(row['end_frame'])
            probe = observed[number]
            assert native[number]['outcome'] == 'rendered' and not probe['waiting'], (row, probe)
            body = int(row['body'])
            geometry = probe['publication_geometry'][body]
            assert row['epoch'] == str(geometry['epoch']) and row['request_serial'] == str(geometry['serial'])
            for column in ('land_field', 'land_topology', 'land_revision', 'water_field', 'water_topology', 'water_revision'):
                assert row[column] == str(geometry[column]), (row, geometry)
            assert float(row['publication_ms']) >= float(row['prepared_ms'])
            assert int(row['publication_frames']) == number - int(row['start_frame'])
            if backend == 'compute':
                consumer = probe['publication']['consumers'][body]
                assert row['land_revision'] == str(consumer['main_revision']) == str(consumer['grass_revision'])
                assert consumer['land'] == consumer['grass'] == consumer['contacts']
                assert [float(row[f'grass_eye_{axis}']) for axis in 'xyz'] == consumer['grass_plan_eye']
        results[backend] = {**validate(session.frames, managed=backend == 'compute'),
                            'loop_outcomes': counts, 'joined_receipts': len(native),
                            'publication_receipts': len(publications), 'published_draws': len(published),
                            'renderer': session.frames[0]['renderer'],
                            'opengl_version': session.frames[0]['opengl_version'],
                            'memory': validate_memory(str(trace) + '.memory.csv', backend, session.frames[0]['renderer'])}
        write_json(out / 'results.json', results)
        print(f'Validated {backend} native full wall/poll/present/loading/minimized/resume: {counts}', flush=True)
    finally:
        session.abort()
