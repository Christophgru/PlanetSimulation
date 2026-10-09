"""Fetch and verify pinned NASA comparison assets locally.

Original model downloads stay in ignored build-f5. Blender 4.5 handles Draco.
This is an inspection utility, not a general-purpose glTF conversion pipeline.
"""
import hashlib
import json
import struct
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'USER_IO/astronaut_vis'
CACHE = ROOT / 'build-f5/astronaut-next'


def unpack(blob):
    magic, version, length = struct.unpack_from('<4sII', blob)
    assert magic == b'glTF' and version == 2 and length == len(blob)
    size, kind = struct.unpack_from('<II', blob, 12)
    assert kind == 0x4E4F534A
    doc = json.loads(blob[20:20 + size])
    count, kind = struct.unpack_from('<II', blob, 20 + size)
    assert kind == 0x004E4942 and 28 + size + count == length
    return doc, bytearray(blob[28 + size:])


def main():
    CACHE.mkdir(parents=True, exist_ok=True)
    for row in json.loads((OUT / 'sources.json').read_text())['models']:
        source = CACHE / (row['name'] + '.glb')
        if not source.exists():
            with urllib.request.urlopen(row['url'], timeout=60) as response:
                blob = response.read(row['bytes'] + 1)
            assert len(blob) == row['bytes']
            assert hashlib.sha256(blob).hexdigest() == row['sha256']
            source.write_bytes(blob)
        blob = source.read_bytes()
        assert hashlib.sha256(blob).hexdigest() == row['sha256'], source
        doc, _ = unpack(blob)
        assert len(doc.get('skins', [])) == row['skins']
        assert len(doc.get('animations', [])) == row['animations']
        print('Verified original', row['key'], flush=True)


if __name__ == '__main__':
    main()
