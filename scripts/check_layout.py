#!/usr/bin/env python3
"""Keep project folders navigable and README captures/metadata consistent."""
import json
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
for folder in ('src', 'tests', 'shaders', 'docs', 'scripts', 'configs'):
    for directory in [root / folder, *(root / folder).rglob('*')]:
        if not directory.is_dir() or '__pycache__' in directory.parts:
            continue
        files = [p for p in directory.iterdir() if p.is_file()]
        if directory == root / 'docs/screenshots':
            assert all(p.suffix == '.png' for p in files), 'Screenshot folder must contain images only'
        else:
            assert len(files) < 10, f'{directory}: {len(files)} files; split by subsystem'
readme = (root / 'README.md').read_text()
for target in set(re.findall(r'docs/(?:screenshots|captures)/[\w/.-]+', readme)):
    assert (root / 'docs' / Path(target).relative_to('docs')).exists(), target
manifest = json.loads((root / 'docs/captures/generation.json').read_text())
assert {r['image'] for r in manifest['images']} == set(re.findall(r'docs/screenshots/([\w-]+\.png)', readme))
print('Validated folder limits, image-only gallery and README capture links')
