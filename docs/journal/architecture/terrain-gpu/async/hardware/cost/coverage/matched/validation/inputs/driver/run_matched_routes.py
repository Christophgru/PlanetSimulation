from pathlib import Path
import subprocess
import sys
root=Path('/workspace');out=root/'build-resume/matched-routes';out.mkdir(exist_ok=True)
for case,distance in (('sprint',25),('walking',25),('sprint',350),('walking',350)):
    name=f'{case}-{distance}';log=out/(name+'.log')
    command=[sys.executable,str(root/'scripts/benchmarks/terrain_cost/coverage/matched.py'),
        '--probe',str(root/'build-resume/tests/terrain_coverage_probe'),'--output-dir',str(out/name),
        '--expected-uuid','GPU-2cefee61-6b3b-a670-c390-c7449dad3f79',
        '--distance',str(distance),'--case',case,'--pairs','3']
    print('Starting '+name,flush=True)
    with log.open('w') as stream:
        result=subprocess.run(command,cwd=root,stdout=stream,stderr=subprocess.STDOUT)
    print(log.read_text(),flush=True)
    if result.returncode:
        raise SystemExit(result.returncode)
print('All 24 excluded routes complete',flush=True)
