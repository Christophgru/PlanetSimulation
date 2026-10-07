from pathlib import Path
import gzip,hashlib,json,lzma,shutil
from concurrent.futures import ThreadPoolExecutor, Future
from threading import Lock
root=Path('/workspace');study=root/'docs/journal/architecture/terrain-gpu/async/hardware/cost/coverage/live';data=study/'validation';data.mkdir(parents=True,exist_ok=True)
pool=ThreadPoolExecutor(max_workers=8);pending=[]
cache={};lock=Lock();completed=0
def write(p,v):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(v,indent=2)+'\n')
def encode(f,p):
 global completed
 raw=f.read_bytes();digest=hashlib.sha256(raw).hexdigest()
 use_xz='live' in f.parts and f.suffix in ('.rgba32f','.stencil')
 p=p.with_suffix(p.suffix+('.xz' if use_xz else '.gz'))
 key=(digest,use_xz)
 with lock:
  owner=key not in cache
  if owner:cache[key]=Future()
  shared=cache[key]
 if owner:
  try:
   encoded=p.read_bytes() if p.exists() else None
   try:decoded=(lzma.decompress(encoded) if use_xz else gzip.decompress(encoded)) if encoded else None
   except (lzma.LZMAError,EOFError,OSError):decoded=None
   if decoded!=raw:
    encoded=lzma.compress(raw,format=lzma.FORMAT_XZ,filters=[{'id':lzma.FILTER_DELTA,'dist':16},{'id':lzma.FILTER_LZMA2,'preset':3}]) if use_xz else gzip.compress(raw,compresslevel=6,mtime=0)
   shared.set_result(encoded)
  except BaseException as error:
   shared.set_exception(error);raise
 encoded=shared.result()
 if not p.exists() or p.read_bytes()!=encoded:
  temporary=p.with_suffix(p.suffix+'.tmp');temporary.write_bytes(encoded);temporary.replace(p)
 with lock:
  completed+=1
  if completed%100==0:print('Verified/encoded',completed,'raw files; unique payloads',len(cache),flush=True)
 return {'path':str(p.relative_to(data)),'encoded_bytes':len(encoded),'decoded_bytes':len(raw),'decoded_sha256':digest}

def archive(source,target):
 for f in sorted(source.rglob('*')):
  if not f.is_file() or '__pycache__' in f.parts:continue
  p=target/f.relative_to(source);p.parent.mkdir(parents=True,exist_ok=True)
  if f.suffix in ('.rgba32f','.depth','.rgba','.stencil','.blades'):pending.append(pool.submit(encode,f,p))
  else:shutil.copyfile(f,p)
final=[{'path':'final/software/live','kind':'fixtures','device':'software'},
       {'path':'final/software/matched','kind':'compatibility','device':'software'},
       {'path':'final/software/ordinary','kind':'compatibility','device':'software'},
       {'path':'final/quadro','kind':'fixtures','device':'quadro'},
       {'path':'final/production/walking25','kind':'routes','device':'quadro'},
       {'path':'final/production/sprint350','kind':'routes','device':'quadro'}]
for source,batch in zip(('terrain-live-area','terrain-matched-coverage','terrain-coverage-inspection','live-final-quadro','live-production-walking25','live-production-sprint350'),final):
 archive(root/'build-resume'/source,data/batch['path']);print('Archived',batch['path'],flush=True)
preflight=['coarse','schema','close','passing','matched','ordinary']
for name in preflight:
 source=root/'build-resume'/('live-preflight-'+name)
 if source.exists():archive(source,data/'preflight'/name)
for f in sorted((root/'build-resume').glob('live-*.log')):
 kind='build' if 'build' in f.name or 'frozen' in f.name else 'tests'
 p=data/'logs'/kind/f.name;p.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(f,p)
# Subdivide the retained preflight logs to keep each folder navigable.
for p in list((data/'logs/tests').glob('*')):
 if any(s in p.name for s in ('first','refined','preflight')):
  target=data/'logs/preflight'/p.name;target.parent.mkdir(exist_ok=True);p.rename(target)
for source,target in [('tests/app/terrain/native/coverage','inputs/native/coverage'),('scripts/benchmarks/terrain_cost','inputs/scripts/terrain_cost')]:
 archive(root/source,data/target)
for name in ('NativeSession.py','NativeProbe.cpp'):
 p=data/'inputs/native'/name;p.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(root/'tests/app/terrain/native'/name,p)
p=data/'inputs/build/CMakeLists.txt';p.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(root/'tests/CMakeLists.txt',p)
(p.parent/'.gitattributes').write_text('CMakeLists.txt -whitespace\n')
(data/'.gitattributes').write_text('*.log -whitespace\n')
shutil.copyfile(root/'build-resume/area-provenance.json',data/'before-provenance.json');shutil.copyfile(root/'build-resume/live-provenance.json',data/'provenance.json')
entries=[f.result() for f in pending];pool.shutdown()
write(data/'storage.json',{'encoding':'Lossless standard XZ (16-byte delta, LZMA2 preset 3) for live float/stencil maps; deterministic gzip for compatibility payloads','files':entries,'decoded_bytes':sum(e['decoded_bytes'] for e in entries),'encoded_bytes':sum(e['encoded_bytes'] for e in entries)})
counts={'native_snapshots':0,'controlled_snapshots':0,'live_snapshots':0,'live_grids':0,'biome_cases':0};failures=[]
for batch in final:
 for p in (data/batch['path']).rglob('snapshot.json'):
  counts['native_snapshots']+=1
  if p.parent.name=='grass':continue
  matched=p.parent/'matched'
  if not (matched/'receipt.json').exists():continue
  counts['controlled_snapshots']+=1
  if not json.loads((matched/'receipt.json').read_text()).get('live'):continue
  r=json.loads((matched/'live-analysis.json').read_text());counts['live_snapshots']+=1;counts['live_grids']+=len(r['samples']);counts['biome_cases']+=len(r['biome_diagnostics'])
  if not r['qualified_area']:failures.append(str(matched.relative_to(data)))
write(data/'manifest.json',{'schema':1,'scope':'Live area qualification; no cost or fresh three-pair coverage acceptance','base_revision':'07f8563','expected_uuid':'GPU-2cefee61-6b3b-a670-c390-c7449dad3f79','final':final,'preflight':preflight,'expected_counts':counts,'area_failures':sorted(failures),'input_sha256':{str(p.relative_to(data)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted((data/'inputs').rglob('*')) if p.is_file()}})
print('Final counts',counts,'failures',failures,flush=True)
print('Stored',len(entries),'raw files;',sum(e['decoded_bytes'] for e in entries)/2**20,'decoded MiB;',sum(e['encoded_bytes'] for e in entries)/2**20,'encoded MiB',flush=True)
