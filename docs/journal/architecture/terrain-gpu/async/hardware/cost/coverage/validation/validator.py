#!/usr/bin/env python3
"""Recompute live-generation roots/masks and their raw native-frame joins."""
import argparse
from array import array
import csv
import gzip
import hashlib
import json
import io
import math
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parent
DATA = ROOT / 'validation'


def read(path):
    return gzip.decompress(path.read_bytes()) if path.suffix == '.gz' else path.read_bytes()


def validate():
    manifest = json.loads((DATA / 'manifest.json').read_text())
    checks = []
    for entry in manifest['snapshots']:
        folder = DATA / entry['path']
        receipt = json.loads((folder / 'snapshot.json').read_text())
        result = json.loads((folder / 'analysis.json').read_text())
        assert receipt['view_identity'] == 'main' and receipt['body'] == 0
        assert receipt['inspection']['blocking'] and not receipt['inspection']['timing_acceptance']
        assert receipt['inspection']['pixel_reads'] == 2
        assert 'reflection_instances_after_main' in receipt
        assert receipt['trail_segments_m'] == receipt['astronaut']['grass_trail']
        native = [json.loads(line) for line in read(DATA / entry['frames']).splitlines()]
        frames = {f['profile_frame']: f for f in native}
        frame = frames[receipt['profile_frame']]
        assert frame['pose']['root'] == receipt['root_m'] and frame['pose']['walked_m'] == receipt['walked_m']
        assert frame['backend'] == receipt['backend']
        geometry = frame['publication_geometry'][0]
        for key in ('land_field', 'land_topology', 'land_revision'):
            assert geometry[key] == receipt[key]
        if receipt['backend'] == 'compute':
            consumer = receipt['publication_after_frame']['consumers'][0]
            assert consumer['land'] == consumer['grass'] == consumer['contacts']
            assert consumer['land_revision'] == consumer['grass_revision'] == receipt['land_revision']
        if entry.get('hardware'):
            memory = DATA / Path(entry['frames']).parent / 'performance.csv.memory.csv.gz'
            samples = list(csv.DictReader(io.StringIO(read(memory).decode())))
            assert samples and all(r['context_status'] == 'uuid_verified' and r['nvml_status'] == 'ok' and
                                   r['context_uuid'] == manifest['hardware_uuid'] for r in samples)
            assert 'Quadro M1000M' in frame['renderer']
        if entry.get('production'):
            workload = receipt['workload']
            assert receipt['viewport'] == [1280, 720] and workload['quality_scale'] == 1
            assert workload['bodies'][0]['triangles'] == 100000
            assert workload['bodies'][0]['foliage']['budget'] == 2000000
        assert all(frame['gl'][k] == 0 for k in ('blocking_polls', 'server_waits', 'bulk_reads', 'finishes', 'memory_queries'))
        bands, faded, counts = [0, 0, 0], [0., 0., 0.], []
        for queue, name in enumerate(('detailed', 'quads')):
            raw = read(folder / (name + '.blades.gz'))
            q = receipt['queues'][queue]
            assert len(raw) == 64 * q['instances'] and q['instances'] <= q['capacity']
            assert q['offset_bytes'] == queue * q['capacity'] * 64
            counts.append(q['instances'])
            for values in struct.iter_unpack('<16f', raw):
                assert all(math.isfinite(x) for x in values) and 0 < values[3] <= 1
                assert abs(math.sqrt(sum(x*x for x in values[4:7])) - 1) < 1e-4
                root = [x * receipt['meters_per_radius'] for x in values[:3]]
                distance = math.dist(root, receipt['root_m'])
                for index, (lo, hi) in enumerate(((0, 5), (5, 15), (15, 30))):
                    if lo <= distance < hi:
                        bands[index] += 1
                        faded[index] += values[3]
        assert counts == result['generated_instances'] and bands == result['root_band_counts']
        assert faded == result['root_band_fade_sum']
        depths = []
        for name in ('ground', 'grass'):
            values = array('f');values.frombytes(read(folder / (name + '.depth.gz')))
            depths.append(values)
        width, height = receipt['viewport']
        ground, grass = depths
        assert len(ground) == len(grass) == width * height
        assert all(math.isfinite(a) and math.isfinite(b) and 0 <= b <= a <= 1 for a, b in zip(ground, grass))
        mask = bytes(255 if b < a else 0 for a, b in zip(ground, grass))
        pixels = sum(x > 0 for x in mask)
        assert pixels == result['opaque_grass_pixels'] and pixels > 0
        rows = b''.join(mask[y*width:(y+1)*width] for y in range(height-1, -1, -1))
        assert (folder / 'grass-mask.pgm').read_bytes() == f'P5\n{width} {height}\n255\n'.encode() + rows
        assert not result['coverage_acceptance']
        checks.append({'path': entry['path'], 'native_frame': receipt['profile_frame'], 'instances': counts,
                       'root_bands': bands, 'opaque_grass_pixels': pixels, 'coverage_acceptance': False})
    return checks


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__);p.add_argument('--write', action='store_true');args = p.parse_args()
    checks = validate()
    if args.write:
        (DATA / 'checks.json').write_text(json.dumps(checks, indent=2) + '\n')
    else:
        assert checks == json.loads((DATA / 'checks.json').read_text())
        evidence = json.loads((DATA / 'evidence.json').read_text())
        for name, digest in evidence['artifacts'].items():
            assert hashlib.sha256((ROOT / name).read_bytes()).hexdigest() == digest, name
    print(f'Independently verified {len(checks)} native root/depth snapshots; matched coverage acceptance remains pending')
