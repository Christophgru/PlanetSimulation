from pathlib import Path
import os, subprocess, json, shutil
root=Path('/workspace');build=root/'build-resume';study=root/'docs/journal/architecture/terrain-gpu/async/hardware/cost/coverage/repeats';data=study/'validation'
env={**os.environ,'__NV_PRIME_RENDER_OFFLOAD':'1','__GLX_VENDOR_LIBRARY_NAME':'nvidia','PYTHONDONTWRITEBYTECODE':'1','OPENBLAS_NUM_THREADS':'1'}
uuid='GPU-2cefee61-6b3b-a670-c390-c7449dad3f79'
def run(name,args):
 print('Starting',name,flush=True)
 with (build/(name+'.log')).open('w') as log:subprocess.run(['xvfb-run','-a','python3',*args],cwd=root,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
 print('Finished',name,flush=True)

for case,distance in [('walking',25),('walking',350),('sprint',25),('sprint',350)]:
 name=f'repeats-{case}{distance}'
 run(name,['scripts/benchmarks/terrain_cost/coverage/matched.py','--probe','build-resume/tests/terrain_coverage_probe','--live','--compress-raw','--pairs','3','--expected-uuid',uuid,'--case',case,'--distance',str(distance),'--output-dir',f'build-resume/{name}'])
 target=data/'final'/f'{case}{distance}';target.parent.mkdir(parents=True,exist_ok=True)
 shutil.copytree(build/name,target)
 report=json.loads((target/'results.json').read_text());print('Archived',name,'passing pairs',sum(p['coverage_parity'] for p in report['pairs']),'/3',flush=True)
print('All 24 fresh routes retained',flush=True)
