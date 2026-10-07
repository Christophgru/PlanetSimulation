from pathlib import Path
import json,gzip,hashlib,shutil
root=Path('/workspace');build=root/'build-resume'
here=root/'docs/journal/architecture/terrain-gpu/async/hardware/cost/raster';data=here/'validation'
data.mkdir(exist_ok=True)
entries={}
def copyrun(source,target):
 target.mkdir(parents=True,exist_ok=True);trace=target/'trace';trace.mkdir(exist_ok=True)
 for p in sorted(source.iterdir()):
  if not p.is_file():continue
  dest=(trace if p.suffix=='.csv' else target)/p.name
  if p.suffix in ('.jsonl','.csv'):
   raw=p.read_bytes();encoded=gzip.compress(raw,compresslevel=6,mtime=0)
   assert gzip.decompress(encoded)==raw
   dest=dest.with_suffix(dest.suffix+'.gz');dest.write_bytes(encoded)
  else:shutil.copy2(p,dest)
  raw=p.read_bytes();stored=dest.read_bytes()
  assert (gzip.decompress(stored) if dest.suffix=='.gz' else stored)==raw
  entries[dest.relative_to(data).as_posix()]={'source':p.relative_to(root).as_posix(),'decoded_bytes':len(raw),'decoded_sha256':hashlib.sha256(raw).hexdigest()}
 for p in source.iterdir():
  if p.is_dir():copyrun(p,target/p.name)
for wind in ('native','fixed','indexed'):
 for raster in ('full','discard','suppress'):
  name=f'raster-{wind}-{raster}';source=build/name
  report=json.loads((source/'results.json').read_text());assert len(report['runs'])==6 and len(report['pairs'])==3
  copyrun(source,data/'final'/wind/raster)
  print('Archived',wind,raster,flush=True)
for source,kind in [('terrain-raster-controls','software'),('raster-quadro-qualification','quadro')]:
 copyrun(build/source,data/'qualification'/kind)
for source,kind in [('raster-software-preflight','labels'),('raster-count-preflight','counts')]:
 copyrun(build/source,data/'preflight'/kind)
(data/'codec-receipts.json').write_text(json.dumps({'schema':1,'entries':entries,'entry_count':len(entries),
 'decoded_bytes':sum(e['decoded_bytes'] for e in entries.values()),
 'stored_bytes':sum((data/p).stat().st_size for p in entries)},indent=2)+'\n')
inputs=data/'inputs';(inputs/'native/raster').mkdir(parents=True,exist_ok=True);(inputs/'driver').mkdir(exist_ok=True);(inputs/'standard').mkdir(exist_ok=True);(inputs/'preflight').mkdir(exist_ok=True);(inputs/'reproduction').mkdir(exist_ok=True)
for p in (root/'tests/app/terrain/native/raster').iterdir():
 if p.is_file():shutil.copy2(p,inputs/'native/raster'/p.name)
for name in ('NativeProbe.cpp','BenchmarkObservation.h','NativeSession.py'):shutil.copy2(root/'tests/app/terrain/native'/name,inputs/'native'/name)
for p in (root/'scripts/benchmarks/terrain_cost/raster').iterdir():
 if p.is_file():shutil.copy2(p,inputs/'driver'/p.name)
for name in ('receipts.py','run.py'):shutil.copy2(root/'scripts/benchmarks/terrain_cost'/name,inputs/'standard'/name)
shutil.copy2(root/'tests/CMakeLists.txt',inputs/'native/CMakeLists.txt')
for name in ('raster-preflight-Hooks.h','raster-preflight-State.h','raster-preflight-test.py','raster-preflight-probe.json','raster-count-preflight-probe.json'):shutil.copy2(build/name,inputs/'preflight'/name)
for name in ('run_raster_study.py','archive_raster_study.py'):shutil.copy2(build/name,inputs/'reproduction'/name)
for source,name in [('raster-before-provenance.json','before-provenance.json'),('raster-final-provenance.json','provenance.json')]:shutil.copy2(build/source,data/name)
logs=data/'logs'
for section in ('qualification','routes','preflight'):(logs/section).mkdir(parents=True,exist_ok=True)
for name in ('raster-qualified-software.log','raster-quadro-qualification.log','raster-frozen-build.log'):shutil.copy2(build/name,logs/'qualification'/name)
for wind in ('native','fixed','indexed'):
 folder=logs/'routes'/wind;folder.mkdir(exist_ok=True)
 for raster in ('full','discard','suppress'):shutil.copy2(build/f'raster-{wind}-{raster}.log',folder/f'{raster}.log')
shutil.copy2(build/'raster-study-final.log',logs/'routes/study.log')
for name in ('raster-build.log','raster-final-build.log','raster-qualified-build.log','raster-software.log','raster-final-software.log'):shutil.copy2(build/name,logs/'preflight'/name)
(data/'.gitattributes').write_text('*.log -whitespace\ninputs/native/CMakeLists.txt -whitespace\n')
(data/'manifest.json').write_text(json.dumps({'schema':1,'uuid':'GPU-2cefee61-6b3b-a670-c390-c7449dad3f79','wind_modes':['native','fixed','indexed'],'raster_modes':['full','discard','suppress'],'frames_per_run':240,'pairs_per_case':3,'native_p95_limit':1.05,'timing_acceptance':False,'migration_acceptance':False},indent=2)+'\n')
print('Lossless raw archive complete',flush=True)
