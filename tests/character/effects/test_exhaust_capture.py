#!/usr/bin/env python3
"""Render lifetime/release-tail checkpoints and require exact replay."""
import argparse,hashlib,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--binary',type=Path,required=True); p.add_argument('--output-dir',type=Path,required=True)
a=p.parse_args(); root=Path(__file__).resolve().parents[3]; out=a.output_dir.resolve(); out.mkdir(parents=True,exist_ok=True)
scene=json.loads((root/'tests/scenarios/foliage/surface.json').read_text())
for body in scene['planets']:
 body.setdefault('foliage',{})['enabled']=False; body.setdefault('terrain_lod',{})['max_triangle_budget']=12000
config=out/'scene.json'; config.write_text(json.dumps(scene,indent=2)+'\n')
def capture(name,frames=1,replay=None,extra=()):
 image=out/(name+'.png'); command=[str(a.binary.resolve()),'--config',str(config),'--astronaut-capture',str(image),'--render-size','480','270','--benchmark-frames',str(frames),'--benchmark-step','0',*extra]
 if frames>1: command+=['--benchmark-character-step','.04']
 if replay: command+=['--replay',str(replay)]
 run=subprocess.run(command,cwd=root,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT); (out/(name+'.log')).write_text(run.stdout)
 assert run.returncode==0,run.stdout
 data=json.loads(Path(str(image)+'.json').read_text()); assert data['render']['astronaut_pixels']>40
 return hashlib.sha256(image.read_bytes()).hexdigest(),data
def replay(name,sha,data):
 result,restored=capture(name+'-replay',replay=out/(name+'.png.json'))
 assert sha==result,name+' image replay changed'; assert data['astronaut_pose']==restored['astronaut_pose'],name+' effect/pose replay changed'
sha,burn=capture('burn',frames=15,extra=('--benchmark-jump-frame','0','--benchmark-boost-frame','1'))
particles=burn['astronaut_pose']['exhaust']['particles']; assert 15<=len(particles)<=64
assert len({round(p['age_s'],3) for p in particles})>10,'All bubbles appeared at once'
assert burn['astronaut_pose']['exhaust']['draw_calls_per_view']==1
assert burn['astronaut_pose']['exhaust']['instance_bytes_per_view']<=2048
replay('burn',sha,burn)
seed=out/'burn.png.json'
sha,tail=capture('released',frames=6,replay=seed)
assert not tail['astronaut_pose']['boosting']
assert tail['astronaut_pose']['exhaust']['particles'],'Release removed exhaust immediately'
assert tail['astronaut_pose']['exhaust']['next_id']==burn['astronaut_pose']['exhaust']['next_id']
replay('released',sha,tail)
sha,retired=capture('retired',frames=38,replay=out/'released.png.json')
assert retired['astronaut_pose']['exhaust']['particles']==[],'Particles survived beyond lifetime'
replay('retired',sha,retired)
# Invalid particle state must reject the replay rather than allocate unbounded history.
bad=json.loads((out/'burn.png.json').read_text()); bad['astronaut_pose']['exhaust']['particles']*=4
path=out/'invalid.json'; path.write_text(json.dumps(bad)+'\n')
command=[str(a.binary.resolve()),'--replay',str(path),'--astronaut-capture',str(out/'invalid.png'),'--render-size','480','270']
result=subprocess.run(command,cwd=root,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
(out/'invalid.log').write_text(result.stdout); assert result.returncode!=0
print('Validated incremental emission, release tail, bounded uploads, retirement, invalid replay rejection and exact images/particle history')
