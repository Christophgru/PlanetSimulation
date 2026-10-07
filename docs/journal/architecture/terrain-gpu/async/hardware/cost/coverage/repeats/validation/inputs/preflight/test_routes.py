"""Crossing rejection and verified lossless storage checks, without GL."""
import gzip
import json
import lzma
from pathlib import Path
import tempfile
import unittest
from crossing import bracket
from storage import compress


class RouteChecks(unittest.TestCase):
    def setUp(self):
        self.frames = [{'frame': i+1, 'profile_frame': i,
                        'pose': {'walked_m': d, 'root': [d, 0, 0]}}
                       for i, d in enumerate((0, 24.7, 25.2))]
        self.snapshot = {'profile_frame': 2, 'root_m': [25.2, 0, 0], 'walked_m': 25.2,
                         'body': 0, 'meters_per_radius': 1000,
                         'land_field': 'a', 'land_topology': 'b', 'land_revision': 3,
                         'publication_before_grass': {}, 'workload': {'bodies': [{
                             'terrain_plan_eye': [0, 0, 0], 'foliage': {
                                 'plan_eye': [.025, 0, 0], 'density': 47, 'patches': 20}}]}}

    def test_adjacent_first_crossing(self):
        result = bracket(self.frames, self.snapshot, 25)
        self.assertEqual(result['walked_interval_m'], [24.7, 25.2])
        self.assertAlmostEqual(result['overshoot_m'], .2)
        self.assertAlmostEqual(result['grass_anchor_distance_m'], .2)

    def test_reject_missed_or_stale_crossing(self):
        for field, value in [('walked_m', 25.3), ('root_m', [25.3, 0, 0])]:
            with self.assertRaises(AssertionError):
                bracket(self.frames, {**self.snapshot, field: value}, 25)
        self.frames[1]['pose']['walked_m'] = 25.1
        with self.assertRaises(AssertionError):
            bracket(self.frames, self.snapshot, 25)
        self.frames[1]['pose']['walked_m'] = 24.7
        self.frames[0]['pose']['walked_m'] = 25.1
        with self.assertRaises(AssertionError):
            bracket(self.frames, self.snapshot, 25)
        self.frames[0]['pose']['walked_m'] = 0
        self.frames[1]['frame'] = 1
        with self.assertRaises(AssertionError):
            bracket(self.frames, self.snapshot, 25)

    def test_storage_roundtrip(self):
        with tempfile.TemporaryDirectory() as name:
            folder = Path(name)
            expected = {'live/1x/eligible.rgba32f': bytes(range(256))*64,
                        'grass/detailed.blades': b'blade'*256,
                        'ground.depth': b'depth'*256}
            for relative, payload in expected.items():
                path = folder/relative; path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(payload)
            summary = compress(folder)
            self.assertEqual(summary['files'], 3)
            for entry in json.loads((folder/'storage.json').read_text())['files']:
                path = folder/entry['path']
                decoded = lzma.decompress(path.read_bytes()) if path.suffix == '.xz' else gzip.decompress(path.read_bytes())
                self.assertEqual(decoded, expected[str(path.relative_to(folder))[:-3]])
                self.assertFalse(Path(str(path)[:-3]).exists())


if __name__ == '__main__':
    unittest.main()
