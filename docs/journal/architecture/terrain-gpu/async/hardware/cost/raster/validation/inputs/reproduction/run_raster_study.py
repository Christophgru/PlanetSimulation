from pathlib import Path
import os,subprocess,json
root=Path('/workspace');build=root/'build-resume'
uuid='GPU-2cefee61-6b3b-a670-c390-c7449dad3f79'
env={**os.environ,'__NV_PRIME_RENDER_OFFLOAD':'1','__GLX_VENDOR_LIBRARY_NAME':'nvidia','PYTHONDONTWRITEBYTECODE':'1'}
for wind in ('native','fixed','indexed'):
 for raster in ('full','discard','suppress'):
  name=f'raster-{wind}-{raster}'
  print('Starting',name,flush=True)
  with (build/(name+'.log')).open('w') as log:
   subprocess.run(['xvfb-run','-a','python3','-B','scripts/benchmarks/terrain_cost/raster/run.py',
    '--probe','build-resume/tests/terrain_raster_probe','--output-dir',f'build-resume/{name}',
    '--expected-uuid',uuid,'--wind-mode',wind,'--raster-mode',raster],cwd=root,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
  report=json.loads((build/name/'results.json').read_text())
  print('Finished',name,'ratios',[round(p['wall_p95_ratio'],6) for p in report['pairs']],flush=True)
print('All 54 runs retained',flush=True)
