"""Lossless per-run compression, verified before removing duplicate raw bytes."""
from concurrent.futures import ThreadPoolExecutor
import gzip
import hashlib
import json
import lzma


def compress(folder):
    def encode(path):
        raw = path.read_bytes()
        live = 'live' in path.parts and path.suffix in ('.rgba32f', '.stencil')
        encoded = (lzma.compress(raw, format=lzma.FORMAT_XZ, filters=[
            {'id': lzma.FILTER_DELTA, 'dist': 16}, {'id': lzma.FILTER_LZMA2, 'preset': 3}])
            if live else gzip.compress(raw, compresslevel=6, mtime=0))
        target = path.with_suffix(path.suffix+('.xz' if live else '.gz'))
        assert not target.exists()
        temporary = target.with_suffix(target.suffix+'.tmp')
        temporary.write_bytes(encoded)
        decoded = lzma.decompress(temporary.read_bytes()) if live else gzip.decompress(temporary.read_bytes())
        assert decoded == raw
        temporary.replace(target)
        path.unlink()
        return {'path': str(target.relative_to(folder)), 'encoded_bytes': len(encoded),
                'decoded_bytes': len(raw), 'decoded_sha256': hashlib.sha256(raw).hexdigest()}
    paths = sorted(p for p in folder.rglob('*') if p.is_file() and p.suffix in
                   ('.rgba32f', '.depth', '.rgba', '.stencil', '.blades'))
    with ThreadPoolExecutor(max_workers=4) as pool:
        files = list(pool.map(encode, paths))
    (folder/'storage.json').write_text(json.dumps({'schema': 1, 'files': files}, indent=2)+'\n')
    return {'files': len(files), 'decoded_bytes': sum(f['decoded_bytes'] for f in files),
            'encoded_bytes': sum(f['encoded_bytes'] for f in files)}
