#!/usr/bin/env python3
"""Qualify tiled GPU area against exact geometry before native route acceptance."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import numpy as np
from area import band_areas, differential_area
from cases import fixtures
from geometry import exact_bands, reference_checks


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--expected-uuid')
    args = parser.parse_args()
    out = args.output_dir.resolve();out.mkdir(parents=True, exist_ok=True)
    root = Path(__file__).resolve().parents[6]
    report = {'schema': 1, 'scope': 'Known-geometry area qualification; no route or timing acceptance',
              'declared_relative_tolerance': .01, 'declared_tile_area_tolerance': .0001,
              'minimum_band_samples': 100, 'cases': [], 'failures': []}
    save = lambda: (out/'results.json').write_text(json.dumps(report, indent=2)+'\n')
    save() # Declare tolerances before any GPU samples.
    reference_checks()
    for name, scene in fixtures().items():
        folder = out/name;folder.mkdir(exist_ok=True)
        source = folder/'scene.json';source.write_text(json.dumps(scene, indent=2)+'\n')
        exact = exact_bands(scene)
        assert min(exact) > 0
        entry = {'case': name, 'exact_band_m2': exact, 'samples': []}
        report['cases'].append(entry)
        for factor, edge in ((1, 256), (2, 256), (4, 256), (4, 191)):
            target = folder/f'{factor}x-tile-{edge}'
            command = [str(args.probe.resolve()), str(source), str(target), str(factor), str(edge)]
            completed = subprocess.run(command, cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            (folder/f'{factor}x-tile-{edge}.log').write_text(completed.stdout)
            assert completed.returncode == 0, completed.stdout
            receipt = json.loads((target/'receipt.json').read_text())
            if args.expected_uuid:
                assert receipt['context_uuid'] == args.expected_uuid, receipt
            width, height = receipt['viewport']
            maps = [np.fromfile(target/(n+'.rgba32f'), '<f4').reshape(height, width, 4) for n in ('eligible', 'plane')]
            control = {**scene, 'viewport': [width, height]}
            area, distance = differential_area(control, *maps)
            measured = band_areas(area, distance)
            errors = [a/b-1 for a, b in zip(measured, exact)]
            pixels = [int(np.count_nonzero((area > 0) & (distance >= lo) & (distance < hi))) for lo, hi in ((0, 5), (5, 15), (15, 30))]
            finite = band_areas(maps[0][..., 3].astype(float), np.linalg.norm(maps[0][..., :3].astype(float), axis=2))
            entry['samples'].append({'factor': factor, 'tile_edge': edge, 'analytic_band_m2': measured,
                                     'finite_quad_band_m2': finite, 'analytic_relative_error': errors,
                                     'eligible_pixels': pixels, 'receipt': receipt,
                                     'raw_sha256': {n: hashlib.sha256((target/(n+'.rgba32f')).read_bytes()).hexdigest() for n in ('eligible', 'plane')}})
            save()
        a, b, c = entry['samples'][1:]
        entry['two_to_four_relative_change'] = [y/x-1 for x, y in zip(a['analytic_band_m2'], b['analytic_band_m2'])]
        entry['tile_relative_change'] = [y/x-1 for x, y in zip(b['analytic_band_m2'], c['analytic_band_m2'])]
        entry['qualified'] = (all(abs(e) < .01 for e in b['analytic_relative_error']+entry['two_to_four_relative_change'])
                              and all(abs(e) < .0001 for e in entry['tile_relative_change'])
                              and min(b['eligible_pixels']) >= 100)
        if not entry['qualified']:
            report['failures'].append(name)
        print(name, '4x errors', b['analytic_relative_error'], '2x/4x', entry['two_to_four_relative_change'], flush=True)
        save()
    assert not report['failures'], report['failures']
    print('Qualified four exact plane/silhouette scenes, 2x/4x convergence and bounded tile changes')


if __name__ == '__main__':
    main()
