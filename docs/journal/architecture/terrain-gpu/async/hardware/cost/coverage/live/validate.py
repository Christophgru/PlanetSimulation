#!/usr/bin/env python3
"""Reconstruct frozen live inspection evidence from losslessly archived raw maps."""
import csv
import gzip
import hashlib
import importlib.util
import json
import lzma
from pathlib import Path
import sys
from concurrent.futures import Future, ThreadPoolExecutor
from threading import Lock
sys.dont_write_bytecode = True

HERE = Path(__file__).resolve().parent
DATA = HERE/'validation'

def read(path): return json.loads(path.read_text())
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec); spec.loader.exec_module(value)
    return value

def main():
    manifest = read(DATA/'manifest.json'); evidence = read(HERE/'evidence.json')
    for path, expected in evidence['artifact_sha256'].items(): assert sha(HERE/path) == expected, path
    before, after = [read(DATA/(name+'provenance.json')) for name in ('before-', '')]
    assert before['source_sha256'] == after['source_sha256']
    assert before['driver_fixtures'] == after['driver_fixtures']
    changed = [p for p, h in before['binaries'].items() if after['binaries'].get(p) != h]
    assert changed == ['build-resume/tests/terrain_coverage_probe'], changed
    assert len(after['binaries']) == 44
    storage = read(DATA/'storage.json')
    decoded_cache = {}; lock = Lock()
    def verify_payload(entry):
        path = DATA/entry['path']; encoded = path.read_bytes()
        digest = hashlib.sha256(encoded).hexdigest()
        with lock:
            owner = digest not in decoded_cache
            if owner: decoded_cache[digest] = Future()
            shared = decoded_cache[digest]
        if owner:
            try:
                decoded = lzma.decompress(encoded) if path.suffix == '.xz' else gzip.decompress(encoded)
                shared.set_result((len(decoded), hashlib.sha256(decoded).hexdigest()))
            except BaseException as error:
                shared.set_exception(error); raise
        assert len(encoded) == entry['encoded_bytes']
        assert shared.result() == (entry['decoded_bytes'], entry['decoded_sha256']), path
    with ThreadPoolExecutor(max_workers=8) as pool:
        list(pool.map(verify_payload, storage['files']))
    print('Verified all payload lengths/hashes, including identical encoded copies', flush=True)
    coverage = DATA/'inputs/native/coverage'
    live = module('frozen_live', coverage/'live/measure.py')
    ordinary = module('frozen_ordinary', coverage/'analyze.py')
    matched = module('frozen_matched', coverage/'matched/measure.py')
    session = module('frozen_session', coverage.parent/'NativeSession.py')
    counts = {'native_snapshots': 0, 'controlled_snapshots': 0, 'live_snapshots': 0, 'live_grids': 0, 'biome_cases': 0}
    max_change = 0.; max_mismatch = 0.; failures = []
    for batch in manifest['final']:
        base = DATA/batch['path']
        for path in sorted(base.rglob('snapshot.json')):
            folder = path.parent; native = read(path)
            expected = read(folder/'analysis.json'); actual = ordinary.analyze(folder)
            assert actual == expected, path
            counts['native_snapshots'] += 1
            if folder.name == 'grass': continue
            run = folder.parents[1]
            frames = [json.loads(line) for line in (run/'frames.jsonl').read_text().splitlines()]
            session.validate(frames, managed=native['backend'] == 'compute') if 'backend' in native else session.validate(frames, managed=frames[0]['backend'] == 'compute')
            frame = next(f for f in frames if f['profile_frame'] == native['profile_frame'])
            assert frame['pose']['root'] == native['root_m'] and frame['pose']['walked_m'] == native['walked_m']
            log = (run/'run.log').read_text()
            assert 'Native audit:' in log and 'Native probe failed:' not in log
            if batch['device'] == 'quadro':
                rows = list(csv.DictReader((run/'performance.csv.memory.csv').open()))
                assert rows and all(r['context_uuid'] == manifest['expected_uuid'] and r['context_status'] == 'uuid_verified' and r['nvml_status'] == 'ok' for r in rows)
            else:
                assert all('llvmpipe' in f['renderer'] for f in frames)
            controlled = folder/'matched'
            if not (controlled/'receipt.json').exists(): continue
            expected = read(controlled/'analysis.json'); actual = matched.analyze(controlled)
            assert actual == expected, controlled
            counts['controlled_snapshots'] += 1
            receipt = read(controlled/'receipt.json')
            if not receipt.get('live'): continue
            expected = read(controlled/'live-analysis.json'); actual = live.analyze(controlled)
            assert actual == expected, controlled
            if not actual['qualified_area']: failures.append(str(controlled.relative_to(DATA)))
            counts['live_snapshots'] += 1; counts['live_grids'] += len(actual['samples'])
            counts['biome_cases'] += len(actual['biome_diagnostics'])
            max_change = max(max_change, max(abs(v) for v in actual['two_to_four_relative_change'] if v is not None))
            max_mismatch = max(max_mismatch, actual['samples'][0]['native_opaque_tag_mismatch_fraction'])
        print('Reconstructed', batch['path'], flush=True)
        if batch['kind'] == 'routes':
            report = read(base/'results.json')
            for pair in report['pairs']:
                paths = [base/f'pair-{pair["pair"]}'/backend/'coverage/1/matched' for backend in ('cpu', 'compute')]
                comparison = live.compare(*paths, report['declared_relative_tolerance'])
                for key, value in comparison.items(): assert pair[key] == value, key
    assert counts == manifest['expected_counts'], counts
    assert failures == manifest['area_failures'], failures
    result = {**counts, 'maximum_two_to_four_relative_change': max_change,
              'maximum_native_tag_mismatch_fraction': max_mismatch, 'area_failures': failures,
              'preflight_storage_verified': True, 'coverage_acceptance': False, 'timing_acceptance': False}
    print(json.dumps(result, indent=2))

if __name__ == '__main__': main()
