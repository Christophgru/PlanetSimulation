"""Validate published render receipts and hashes; optionally check local originals."""
import argparse
import hashlib
import json
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'USER_IO/astronaut_vis'
KEYS = {'nasa-z2', 'nasa-emu', 'nasa-mark-iii', 'nasa-aces', 'nasa-gemini'}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def check_png(path, size):
    blob = path.read_bytes()
    assert blob[:8] == b'\x89PNG\r\n\x1a\n'
    assert struct.unpack_from('>II', blob, 16) == size
    assert len(blob) > 20_000, path
    subprocess.run(['identify', '-regard-warnings', str(path)],
                   stdout=subprocess.DEVNULL, check=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--record', action='store_true')
    parser.add_argument('--with-models', action='store_true')
    args = parser.parse_args()
    rows = json.loads((OUT / 'sources.json').read_text())['models']
    assert len(rows) == 5 and {r['key'] for r in rows} == KEYS
    for row in rows:
        report = json.loads((OUT / 'validation' / (row['key'] + '.json')).read_text())
        assert report['source_sha256'] == row['sha256']
        assert report['display_yaw_degrees'] == row['yaw_degrees']
        assert report['source_triangles'] == row['triangles']
        assert report['triangles'] == row['triangles'] - row.get('degenerate_triangles', 0)
        assert report['skins'] == report['animations'] == 0
        assert report['normalized_height_m'] == 2
        assert report['render_engine'] == 'CYCLES'
        assert report['blender_version'].startswith('4.5.')
        assert set(report['views']) == {'front', 'back', 'detail'}
        for view in report['views'].values():
            path = OUT / view['file']
            assert digest(path) == view['sha256'], path
            check_png(path, (900, 1080))
        if args.with_models:
            source = ROOT / 'build-f5/astronaut-next' / (row['name'] + '.glb')
            assert source.stat().st_size == row['bytes'] and digest(source) == row['sha256']
            if row.get('degenerate_triangles'):
                from fetch_realistic import unpack
                doc, binary = unpack(source.read_bytes())
                count = 0
                for mesh in doc['meshes']:
                    for primitive in mesh['primitives']:
                        accessor = doc['accessors'][primitive['indices']]
                        view = doc['bufferViews'][accessor['bufferView']]
                        start = view.get('byteOffset', 0) + accessor.get('byteOffset', 0)
                        code = {5121: 'B', 5123: 'H', 5125: 'I'}[accessor['componentType']]
                        indices = struct.unpack_from('<' + str(accessor['count']) + code, binary, start)
                        count += sum(len(set(indices[i:i + 3])) < 3
                                     for i in range(0, len(indices), 3))
                assert count == row['degenerate_triangles']
    for path in (OUT / 'comparison.png', OUT / 'views/back-comparison.png',
                 OUT / 'views/detail-comparison.png'):
        check_png(path, (2250, 604))
    check_png(OUT / 'references/comparison.png', (900, 928))
    files = sorted(p for p in OUT.rglob('*') if p.is_file() and p.name != 'manifest.json')
    files += sorted((ROOT / 'scripts/character').glob('*_realistic.py'))
    current = {str(p.relative_to(ROOT)): digest(p) for p in files}
    manifest_path = OUT / 'manifest.json'
    if args.record:
        manifest_path.write_text(json.dumps({'schema': 2, 'artifacts': current}, indent=2) + '\n')
    assert json.loads(manifest_path.read_text())['artifacts'] == current, 'Published artifacts changed'
    print('PASS: five original meshes, 15 renders, three contact sheets and artifact hashes')


if __name__ == '__main__':
    main()
