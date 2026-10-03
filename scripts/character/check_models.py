"""Validate delivered GLBs, source hashes, skin chains, clips and preview PNGs.

Standard library only. --record writes the manifest after renders/docs exist;
without it, additionally verify every recorded artifact hash.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'USER_IO/astronaut_vis'
KEYS = ('astrodev', 'polygonal-astronaut', 'polygonal-cosmonaut')
COMPONENTS = {5121: ('B', 1), 5123: ('H', 2), 5125: ('I', 4), 5126: ('f', 4)}
WIDTH = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def glb(path):
    raw = path.read_bytes()
    magic, version, length = struct.unpack_from('<III', raw)
    assert magic == 0x46546c67 and version == 2 and length == len(raw)
    chunks, offset = {}, 12
    while offset < len(raw):
        size, kind = struct.unpack_from('<II', raw, offset)
        assert size % 4 == 0 and offset + 8 + size <= len(raw)
        assert kind not in chunks
        chunks[kind] = raw[offset + 8:offset + 8 + size]
        offset += 8 + size
    assert offset == len(raw)
    doc = json.loads(chunks[0x4e4f534a])
    binary = chunks[0x004e4942]
    assert len(doc['buffers']) == 1 and 'uri' not in doc['buffers'][0]
    assert doc['buffers'][0]['byteLength'] <= len(binary)
    for view in doc['bufferViews']:
        assert view.get('buffer', 0) == 0
        assert view.get('byteOffset', 0) + view['byteLength'] <= len(binary)
    assert not any('uri' in image for image in doc['images'])
    for image in doc['images']:
        view = doc['bufferViews'][image['bufferView']]
        start = view.get('byteOffset', 0)
        assert binary[start:start + 8] == b'\x89PNG\r\n\x1a\n'

    def accessor(index):
        a = doc['accessors'][index]
        assert 'sparse' not in a
        view = doc['bufferViews'][a['bufferView']]
        code, size = COMPONENTS[a['componentType']]
        count = WIDTH[a['type']]
        stride = view.get('byteStride', size * count)
        local = a.get('byteOffset', 0)
        assert stride >= size * count
        assert local + (a['count'] - 1) * stride + size * count <= view['byteLength']
        start = view.get('byteOffset', 0) + local
        rows = [struct.unpack_from('<' + code * count, binary, start + i * stride)
                for i in range(a['count'])]
        if a.get('normalized'):
            assert a['componentType'] in (5121, 5123)
            divisor = 255 if size == 1 else 65535
            rows = [tuple(x / divisor for x in row) for row in rows]
        assert all(math.isfinite(x) for row in rows for x in row)
        return rows

    return doc, accessor


def check(key):
    folder = OUT / 'models' / key
    source = json.loads((folder / 'source.json').read_text())
    for extension, digest in source['source_sha256'].items():
        path = folder / ('palette.png' if extension == 'palette_png' else 'source.' + extension)
        assert sha(path) == digest, path
    assert source['creator'] and source['license'] and source['license_url']
    assert (folder / 'LICENSE.txt').stat().st_size > 100
    assert (folder / 'astronaut.blend').read_bytes().startswith(b'BLENDER')
    report = json.loads((folder / 'inspection.json').read_text())
    assert report['backpack_present'] is False
    assert report['finite_pose_samples'] == 31
    doc, read = glb(folder / 'astronaut.glb')
    assert len(doc['meshes']) == len(doc['skins']) == 1
    skin = doc['skins'][0]
    joint_count = len(skin['joints'])
    assert joint_count >= 20
    assert len(read(skin['inverseBindMatrices'])) == joint_count
    joint_by_name = {doc['nodes'][node]['name']: node for node in skin['joints']}
    parent = {}
    for node, item in enumerate(doc['nodes']):
        for child in item.get('children', []):
            assert child not in parent
            parent[child] = node
    for node in range(len(doc['nodes'])):
        seen, p = set(), node
        while p in parent:
            assert p not in seen
            seen.add(p)
            p = parent[p]
    semantic = report['semantic_bones']
    for side in ('left', 'right'):
        thigh, shin, foot = [joint_by_name[semantic[side + '_' + part]]
                             for part in ('thigh', 'shin', 'foot')]
        assert parent[shin] == thigh and parent[foot] == shin
    vertices, triangles = 0, 0
    for primitive in doc['meshes'][0]['primitives']:
        assert primitive.get('mode', 4) == 4
        positions = read(primitive['attributes']['POSITION'])
        joints = read(primitive['attributes']['JOINTS_0'])
        weights = read(primitive['attributes']['WEIGHTS_0'])
        assert len(positions) == len(joints) == len(weights)
        assert len(read(primitive['attributes']['NORMAL'])) == len(positions)
        assert len(read(primitive['attributes']['TEXCOORD_0'])) == len(positions)
        for js, ws in zip(joints, weights):
            assert all(0 <= j < joint_count for j in js)
            assert all(0 <= w <= 1 for w in ws)
            assert abs(sum(ws) - 1) < 2e-5
        indices = read(primitive['indices'])
        assert len(indices) % 3 == 0
        assert all(0 <= row[0] < len(positions) for row in indices)
        vertices += len(positions)
        triangles += len(indices) // 3
    assert triangles == report['triangles']
    assert [a['name'] for a in doc['animations']] == ['InspectionWalk']
    animation = doc['animations'][0]
    animated = {channel['target']['node'] for channel in animation['channels']}
    for side in ('left', 'right'):
        for part in ('thigh', 'shin', 'foot'):
            assert joint_by_name[semantic[side + '_' + part]] in animated
    for sampler in animation['samplers']:
        times = [row[0] for row in read(sampler['input'])]
        assert all(a < b for a, b in zip(times, times[1:]))
        assert abs(times[-1] - times[0] - 1) < 1e-5
        values = read(sampler['output'])
        assert len(values) == len(times)
    for path in [OUT / (key + '.png'), OUT / 'views' / (key + '-back.png'),
                 OUT / 'views' / (key + '-walk.png')]:
        raw = path.read_bytes()
        assert raw[:8] == b'\x89PNG\r\n\x1a\n' and len(raw) > 30000
        assert struct.unpack_from('>II', raw, 16) == (900, 1080)
    assert (OUT / (key + '.png')).read_bytes() != (OUT / 'views' / (key + '-walk.png')).read_bytes()
    print('PASS', key, triangles, 'triangles;', joint_count, 'skin joints;', vertices,
          'weighted exported vertices; embedded texture and animated leg chains')
    return {'id': key, 'source': source, 'prepared_triangles': triangles,
            'skin_joints': joint_count, 'exported_vertices': vertices,
            'preview': key + '.png', 'back_preview': 'views/' + key + '-back.png',
            'walk_preview': 'views/' + key + '-walk.png', 'model': 'models/' + key + '/astronaut.glb',
            'blend': 'models/' + key + '/astronaut.blend', 'backpack_present': False}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--record', action='store_true')
    args = parser.parse_args()
    rows = [check(key) for key in KEYS]
    roundtrip = json.loads((OUT / 'validation/roundtrip.json').read_text())
    assert {r['model'] for r in roundtrip} == set(KEYS)
    for r in roundtrip:
        assert r['finite_pose_samples'] == 17 and r['loop_closure_m'] < 1e-4
        assert all(d > .05 for d in r['ankle_travel_m'].values())
    for folder in [OUT, *OUT.rglob('*')]:
        if folder.is_dir():
            assert len([p for p in folder.iterdir() if p.is_file()]) < 10, folder
    manifest = OUT / 'manifest.json'
    if args.record:
        artifacts = [p for p in OUT.rglob('*') if p.is_file()
                     and p != manifest and 'validation' not in p.relative_to(OUT).parts]
        artifacts += list((ROOT / 'scripts/character').glob('*.py'))
        doc = {'date_utc': datetime.now(timezone.utc).isoformat(timespec='seconds'),
               'renderer': 'Blender 3.4.1 / Eevee / Mesa llvmpipe / 8 workers / 48 samples / 900x1080',
               'scope': 'Three downloaded stylized humanoid candidates, prepared without backpacks; asset study only',
               'models': rows, 'roundtrip': roundtrip,
               'artifact_sha256': {str(p.relative_to(ROOT)): sha(p) for p in sorted(artifacts)}}
        manifest.write_text(json.dumps(doc, indent=2) + '\n')
    else:
        doc = json.loads(manifest.read_text())
        for name, digest in doc['artifact_sha256'].items():
            assert sha(ROOT / name) == digest, name
    print('PASS three GLBs, original source hashes, nine preview PNGs, 51 imported pose samples, folder limits and artifact manifest')


if __name__ == '__main__':
    main()
