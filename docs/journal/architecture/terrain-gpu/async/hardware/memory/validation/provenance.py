from pathlib import Path
import hashlib,json,os,sys
root=Path('/workspace')
def tree(names):
    files=[]
    for name in names:
        p=root/name
        files.extend([p] if p.is_file() else [f for f in p.rglob('*') if f.is_file() and '__pycache__' not in f.parts])
    h=hashlib.sha256()
    for p in sorted(files,key=lambda f:f.relative_to(root).as_posix()):
        h.update(p.relative_to(root).as_posix().encode()+b'\0'+p.read_bytes()+b'\0')
    return h.hexdigest()
def inputs():
    binaries=[root/'build-resume/PlanetSimulation']+[p for p in (root/'build-resume/tests').iterdir() if p.is_file() and os.access(p,os.X_OK)]
    return {'source_sha256':tree(['src','shaders']),'build_test_input_sha256':tree(['tests','configs','scripts','CMakeLists.txt']),
            'binaries':{p.relative_to(root).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(binaries)},
            'driver_fixtures':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [root/'build-resume/tests/fake-nvml/libnvidia-ml.so.1',root/'build-resume/tests/fake-memory-nvml/libnvidia-ml.so.1']}}
p=root/'build-resume/memory-frozen-inputs.json'
if sys.argv[1]=='freeze':
    current=inputs();p.write_text(json.dumps(current,indent=2)+'\n');print('Frozen source, build/test inputs and',len(current['binaries']),'executables')
else:
    assert inputs()==json.loads(p.read_text()),'Frozen inputs changed'
    print('Final source, build/test input and all executable hashes match')
