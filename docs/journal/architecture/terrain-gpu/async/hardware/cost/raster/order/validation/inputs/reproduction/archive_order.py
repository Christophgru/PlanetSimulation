from pathlib import Path
import gzip
import hashlib
import json
import shutil

root=Path('/workspace');build=root/'build-resume'
here=root/'docs/journal/architecture/terrain-gpu/async/hardware/cost/raster/order'
data=here/'validation';data.mkdir(exist_ok=True);entries={}


def copyrun(source,target):
    target.mkdir(parents=True,exist_ok=True)
    for p in sorted(source.iterdir()):
        if p.is_dir():copyrun(p,target/p.name);continue
        if not p.is_file():continue
        destination=target/p.name
        if p.name.startswith(('detailed.','quads.')):
            destination=target/p.stem/p.suffix[1:];destination.parent.mkdir(exist_ok=True)
        if p.suffix=='.csv':
            destination=target/'trace'/p.name;destination.parent.mkdir(exist_ok=True)
        raw=p.read_bytes()
        if p.suffix in ('.csv','.jsonl','.input','.permuted','.indices','.depth-keys','.depth','.rgba'):
            destination=destination.with_suffix(destination.suffix+'.gz')
            destination.write_bytes(gzip.compress(raw,compresslevel=6,mtime=0))
            assert gzip.decompress(destination.read_bytes())==raw
        else:shutil.copy2(p,destination);assert destination.read_bytes()==raw
        entries[destination.relative_to(data).as_posix()]={'source':p.relative_to(root).as_posix(),
            'decoded_bytes':len(raw),'decoded_sha256':hashlib.sha256(raw).hexdigest()}


log=(build/'order-qualified-software.log').read_text()
line=next(l for l in log.splitlines() if 'Order output:' in l)
final=Path(line.split('Order output:',1)[1].strip());assert (final/'results.json').exists()
copyrun(final,data/'final/software')
copyrun(build/'terrain-blade-order',data/'preflight/coarse')
copyrun(build/'order-refined-software',data/'preflight/refined')
metadata_line=next(l for l in (build/'order-metadata-preflight-software.log').read_text().splitlines() if 'Order output:' in l)
copyrun(Path(metadata_line.split('Order output:',1)[1].strip()),data/'preflight/metadata')
copyrun(build/'order-quadro-fixture',data/'preflight/native-unavailable')
(data/'codec-receipts.json').write_text(json.dumps({'schema':1,'entries':entries,'entry_count':len(entries),
    'decoded_bytes':sum(e['decoded_bytes'] for e in entries.values()),
    'stored_bytes':sum((data/p).stat().st_size for p in entries)},indent=2)+'\n')
inputs=data/'inputs'
for name in ('NativeProbe.cpp','NativeSession.py','BenchmarkObservation.h','raster/Probe.cpp','raster/State.h','raster/Hooks.h',
             'raster/order/Probe.cpp','raster/order/Inspection.h','raster/order/analyze.py','raster/order/test_order.py'):
    target=inputs/'native'/name;target.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(root/'tests/app/terrain/native'/name,target)
shutil.copy2(root/'tests/CMakeLists.txt',inputs/'native/CMakeLists.txt')
pre=inputs/'preflight';pre.mkdir(exist_ok=True)
for name in ('order-coarse-preflight-test.py','order-refined-preflight-test.py','order-provenance.json',
             'order-preflight-Probe.cpp','order-preflight-Inspection.h','order-preflight-test.py','order-metadata-preflight-provenance.json'):
    shutil.copy2(build/name,pre/name)
repro=inputs/'reproduction';repro.mkdir(exist_ok=True);shutil.copy2(__file__,repro/'archive_order.py')
for source,name in [('order-before-provenance.json','before-provenance.json'),('order-final-provenance.json','provenance.json'),('order-device-access.json','device-access.json')]:
    shutil.copy2(build/source,data/name)
for section,names in [('qualification',['order-qualified-build.log','order-exclusion-build.log','order-qualified-software.log']),
                      ('preflight',['order-build.log','order-final-build.log','order-software.log','order-refined-software.log',
                                    'order-quadro-fixture.log','order-glx-context.log','order-egl-context.log','order-metadata-preflight-software.log'])]:
    folder=data/'logs'/section;folder.mkdir(parents=True,exist_ok=True)
    for name in names:shutil.copy2(build/name,folder/name)
(data/'.gitattributes').write_text('*.log -whitespace\ninputs/native/CMakeLists.txt -whitespace\n')
(data/'manifest.json').write_text(json.dumps({'schema':1,'final_cases':['software'],'hardware_complete':False,
    'final_snapshot_frames':18,'preflight_snapshot_frames':54,'timing_acceptance':False,'migration_acceptance':False,
    'probe_sha256':hashlib.sha256((build/'tests/terrain_order_probe').read_bytes()).hexdigest()},indent=2)+'\n')
files={p.relative_to(here).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(data.rglob('*')) if p.is_file()}
(here/'evidence.json').write_text(json.dumps({'schema':1,'artifact_sha256':files},indent=2)+'\n')
print('Archived',len(entries),'byte-verified payloads,',len(files),'total artifacts',flush=True)
