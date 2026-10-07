#!/usr/bin/env python3
"""Losslessly reduce large archived ground maps; never touch live probe inputs."""
from concurrent.futures import ThreadPoolExecutor
import gzip
import hashlib
import json
import lzma
from pathlib import Path

ROOT=Path(__file__).resolve().parent/'validation'


def convert(path):
    original=gzip.decompress(path.read_bytes())
    digest=hashlib.sha256(original).hexdigest();before=path.stat().st_size
    target=path.with_suffix('.xz')
    encoded=lzma.compress(original,filters=[{'id':lzma.FILTER_DELTA,'dist':16},{'id':lzma.FILTER_LZMA2,'preset':3}])
    assert hashlib.sha256(lzma.decompress(encoded)).hexdigest()==digest
    temporary=Path(str(target)+'.tmp');temporary.write_bytes(encoded);temporary.replace(target);path.unlink()
    return {'path':str(target.relative_to(ROOT)),'decoded_sha256':digest,'decoded_bytes':len(original),
            'previous_gzip_bytes':before,'xz_bytes':len(encoded),'format':'standard XZ, lossless byte-delta 16 then LZMA2 preset 3'}


if __name__=='__main__':
    paths=sorted(p for p in ROOT.rglob('*.rgba32f.gz') if p.stat().st_size>1_000_000)
    assert paths,'No new large ground maps; archive transcoding is a one-time operation'
    with ThreadPoolExecutor(max_workers=8) as pool: entries=list(pool.map(convert,paths))
    report={'scope':'Storage encoding only; decoded bytes equal original inspection readbacks',
            'files':entries,'previous_gzip_bytes':sum(e['previous_gzip_bytes'] for e in entries),
            'xz_bytes':sum(e['xz_bytes'] for e in entries)}
    (ROOT/'storage.json').write_text(json.dumps(report,indent=2)+'\n')
    print('Losslessly transcoded',len(entries),'ground maps; saved',round((report['previous_gzip_bytes']-report['xz_bytes'])/2**20,1),'MiB')
