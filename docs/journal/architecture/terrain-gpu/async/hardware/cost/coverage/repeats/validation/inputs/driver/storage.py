"""Lossless per-run compression, verified before removing duplicate raw bytes."""
from concurrent.futures import ThreadPoolExecutor, Future
import gzip
import hashlib
import json
import lzma
from threading import Lock

_cache = {}
_lock = Lock()


def compress(folder):
    def encode(path):
        raw = path.read_bytes()
        live = 'live' in path.parts and path.suffix in ('.rgba32f', '.stencil')
        digest = hashlib.sha256(raw).hexdigest()
        with _lock:
            owner = (digest, live) not in _cache
            if owner: _cache[digest, live] = Future()
            shared = _cache[digest, live]
        if owner:
            try:
                encoded = (lzma.compress(raw, format=lzma.FORMAT_XZ, filters=[
                    {'id': lzma.FILTER_DELTA, 'dist': 16}, {'id': lzma.FILTER_LZMA2, 'preset': 0}])
                    if live else gzip.compress(raw, compresslevel=6, mtime=0))
                # Keep artifacts below the repository's 100 MiB blob limit.
                if live and len(encoded) >= 100*2**20:
                    encoded = lzma.compress(raw, format=lzma.FORMAT_XZ, filters=[
                        {'id': lzma.FILTER_DELTA, 'dist': 16}, {'id': lzma.FILTER_LZMA2, 'preset': 3}])
                assert len(encoded) < 100*2**20
                shared.set_result(encoded)
            except BaseException as error:
                shared.set_exception(error)
                raise
        encoded = shared.result()
        target = path.with_suffix(path.suffix+('.xz' if live else '.gz'))
        assert not target.exists()
        temporary = target.with_suffix(target.suffix+'.tmp')
        temporary.write_bytes(encoded)
        decoded = lzma.decompress(temporary.read_bytes()) if live else gzip.decompress(temporary.read_bytes())
        assert decoded == raw
        temporary.replace(target)
        path.unlink()
        return {'path': str(target.relative_to(folder)), 'encoded_bytes': len(encoded),
                'decoded_bytes': len(raw), 'decoded_sha256': digest}
    paths = sorted(p for p in folder.rglob('*') if p.is_file() and p.suffix in
                   ('.rgba32f', '.depth', '.rgba', '.stencil', '.blades'))
    with ThreadPoolExecutor(max_workers=4) as pool:
        files = list(pool.map(encode, paths))
    (folder/'storage.json').write_text(json.dumps({'schema': 1, 'files': files}, indent=2)+'\n')
    return {'files': len(files), 'decoded_bytes': sum(f['decoded_bytes'] for f in files),
            'encoded_bytes': sum(f['encoded_bytes'] for f in files)}
