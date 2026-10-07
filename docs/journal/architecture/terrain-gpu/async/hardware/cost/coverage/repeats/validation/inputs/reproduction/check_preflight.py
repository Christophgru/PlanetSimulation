"""Recover preflight build/test inputs from the immutable base plus saved edits."""
import hashlib
import json
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[12]
data=Path(__file__).resolve().parents[2]
base='ee13ada'
git=['git','-c',f'safe.directory={root}','-C',str(root)]
paths=subprocess.check_output([*git,'ls-tree','-r','--name-only',base],text=True).splitlines()
paths=[p for p in paths if (p=='CMakeLists.txt' or p.split('/')[0] in ('tests','configs','scripts')) and '__pycache__' not in Path(p).parts]
stream=subprocess.check_output([*git,'cat-file','--batch'],input=''.join(base+':'+p+'\n' for p in paths).encode())
files={};offset=0
for path in paths:
 end=stream.index(b'\n',offset);header=stream[offset:end].split();assert header[1]==b'blob'
 size=int(header[2]);offset=end+1;files[path]=stream[offset:offset+size];offset+=size+1
assert offset==len(stream)
for name in ('projection','storage'):
 recovered=dict(files)
 for leaf in ('matched.py','crossing.py'):
  recovered['scripts/benchmarks/terrain_cost/coverage/'+leaf]=(data/'inputs/driver'/leaf).read_bytes()
 for leaf in ('storage.py','test_routes.py'):
  recovered['scripts/benchmarks/terrain_cost/coverage/'+leaf]=(data/'inputs/preflight'/leaf).read_bytes()
 recovered['tests/app/terrain/native/coverage/matched/Render.h']=(data/('inputs/preflight/Render.h' if name=='projection' else 'inputs/native/coverage/matched/Render.h')).read_bytes()
 h=hashlib.sha256()
 for path in sorted(recovered):h.update(path.encode()+b'\0'+recovered[path]+b'\0')
 provenance=json.loads((data/'inputs/preflight'/(name+'-provenance.json')).read_text())
 assert h.hexdigest()==provenance['build_test_input_sha256'],name
 print(name,'preflight tree verified:',h.hexdigest())
