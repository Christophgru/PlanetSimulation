"""Validate raw main-view roots/depth and export an opaque grass pixel mask.

Counts describe generated queues and visible opaque grass fragments, not eligible
ground area or final atmospheric/water/character compositing. No density parity
or cost acceptance is inferred here.
"""
from array import array
import gzip
import json
import math
from pathlib import Path
import sys


def floats(path):
    data = array('f')
    data.frombytes(path.read_bytes() if path.exists() else gzip.decompress(path.with_suffix(path.suffix + '.gz').read_bytes()))
    assert data.itemsize == 4 and sys.byteorder == 'little'
    assert all(math.isfinite(x) for x in data), path
    return data


def analyze(folder):
    folder = Path(folder)
    receipt = json.loads((folder / 'snapshot.json').read_text())
    assert receipt['schema'] == 1 and receipt['view_identity'] == 'main'
    assert receipt['inspection']['blocking'] and not receipt['inspection']['timing_acceptance']
    assert len(receipt['queues']) == 2
    counts, bands, weighted = [], [0, 0, 0], [0., 0., 0.]
    queues = []
    for queue, name in enumerate(('detailed', 'quads')):
        values = floats(folder / (name + '.blades'))
        info = receipt['queues'][queue]
        assert info['queue'] == queue and info['record_bytes'] == 64
        assert len(values) == info['instances'] * 16 and info['instances'] <= info['capacity']
        assert info['offset_bytes'] == queue * info['capacity'] * 64
        local_bands = [0, 0, 0]
        for start in range(0, len(values), 16):
            record = values[start:start + 16]
            assert 0 < record[3] <= 1
            assert abs(math.sqrt(sum(x*x for x in record[4:7])) - 1) < 1e-4
            root = [x * receipt['meters_per_radius'] for x in record[:3]]
            distance = math.dist(root, receipt['root_m'])
            for band, (lo, hi) in enumerate(((0, 5), (5, 15), (15, 30))):
                if lo <= distance < hi:
                    bands[band] += 1
                    local_bands[band] += 1
                    weighted[band] += record[3]
        counts.append(info['instances'])
        queues.append({'queue': name, 'instances': info['instances'], 'root_bands': local_bands})
    width, height = receipt['viewport']
    ground, grass = [floats(folder / name) for name in ('ground.depth', 'grass.depth')]
    assert len(ground) == len(grass) == width * height
    assert all(0 <= b <= a <= 1 for a, b in zip(ground, grass))
    mask = bytes(255 if b < a else 0 for a, b in zip(ground, grass))
    flipped = b''.join(mask[y*width:(y+1)*width] for y in range(height-1, -1, -1))
    (folder / 'grass-mask.pgm').write_bytes(f'P5\n{width} {height}\n255\n'.encode() + flipped)
    result = {'generated_instances': counts, 'queues': queues, 'root_bands_m': [[0, 5], [5, 15], [15, 30]],
              'root_band_counts': bands, 'root_band_fade_sum': weighted,
              'opaque_grass_pixels': sum(x > 0 for x in mask),
              'scope': receipt['depth_scope'], 'coverage_acceptance': False}
    (folder / 'analysis.json').write_text(json.dumps(result, indent=2) + '\n')
    return result
