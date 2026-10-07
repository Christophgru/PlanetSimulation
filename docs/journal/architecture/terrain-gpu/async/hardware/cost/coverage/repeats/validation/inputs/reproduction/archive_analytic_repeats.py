from pathlib import Path
import importlib.util, json, shutil, hashlib
root=Path('/workspace');build=root/'build-resume';study=root/'docs/journal/architecture/terrain-gpu/async/hardware/cost/coverage/repeats';data=study/'validation'
def copy(source,target):
 target.parent.mkdir(parents=True,exist_ok=True)
 if source.is_dir():shutil.copytree(source,target,ignore=shutil.ignore_patterns('__pycache__'))
 else:shutil.copyfile(source,target)
def module(path):
 spec=importlib.util.spec_from_file_location('storage',path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
storage=module(root/'scripts/benchmarks/terrain_cost/coverage/storage.py')
for p in (data/'final').glob('*/pair-*/*/storage.json'):
 p.rename(p.parent/'coverage/storage.json')
for kind,name in [('software','terrain-live-area'),('quadro','repeats-quadro-qualification')]:
 target=data/'qualification'/kind
 copy(build/name,target)
 print('Compressing qualification',kind,flush=True);print(storage.compress(target),flush=True)
for kind in ('projection','storage'):
 target=data/'preflight'/kind
 copy(build/('repeats-preflight-'+kind),target)
 storage.compress(target)
 # Include already-encoded payloads from the interrupted storage attempt.
 receipt=json.loads((target/'storage.json').read_text());known={e['path'] for e in receipt['files']}
 import gzip,lzma
 for p in target.rglob('*'):
  if p.is_file() and p.suffix in ('.xz','.gz') and str(p.relative_to(target)) not in known:
   encoded=p.read_bytes();raw=lzma.decompress(encoded) if p.suffix=='.xz' else gzip.decompress(encoded)
   receipt['files'].append({'path':str(p.relative_to(target)),'encoded_bytes':len(encoded),'decoded_bytes':len(raw),'decoded_sha256':hashlib.sha256(raw).hexdigest()})
 (target/'storage.json').write_text(json.dumps(receipt,indent=2)+'\n')
 print('Retained preflight',kind,flush=True)

for source,target in [('tests/app/terrain/native/coverage','inputs/native/coverage'),('tests/app/terrain/native/NativeSession.py','inputs/native/NativeSession.py'),('scripts/benchmarks/terrain_cost/coverage','inputs/driver')]:copy(root/source,data/target)
copy(root/'shaders/foliage/placement.comp',data/'inputs/seeding/placement.comp')
copy(root/'shaders/foliage/planning/allocation.comp',data/'inputs/seeding/allocation.comp')
copy(build/'live-provenance.json',data/'before-provenance.json');copy(build/'repeats-final-provenance.json',data/'provenance.json')
for p in build.glob('repeats-*.log'):
 kind='preflight' if 'preflight' in p.name or p.name=='repeats-all.log' else 'routes' if any(x in p.name for x in ('walking','sprint')) else 'qualification'
 copy(p,data/'logs'/kind/p.name)
(data/'.gitattributes').write_text('*.log -whitespace\n')
manifest={'schema':1,'expected_uuid':'GPU-2cefee61-6b3b-a670-c390-c7449dad3f79','cases':['walking25','walking350','sprint25','sprint350'],'qualification':['software','quadro'],'scope':'Fresh excluded coverage comparisons, no timing/migration acceptance'}
(data/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Qualification/preflight archive and frozen inputs complete',flush=True)
