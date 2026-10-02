#!/usr/bin/env python3
"""Check actual space-mode, Moon handoff and their exact rendered replays."""
import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
import subprocess

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--binary',type=Path,required=True)
parser.add_argument('--output-dir',type=Path,required=True)
args=parser.parse_args()
root=Path(__file__).resolve().parents[2]
out=args.output_dir.resolve(); out.mkdir(parents=True,exist_ok=True)
scene=json.loads((root/'tests/scenarios/foliage/surface.json').read_text())
for planet in scene['planets']:
    planet.setdefault('foliage',{})['enabled']=False
    planet.setdefault('terrain_lod',{})['max_triangle_budget']=12000
config=out/'scene.json'; config.write_text(json.dumps(scene,indent=2)+'\n')

def capture(name,replay=None,frames=2,extra=()):
    path=out/(name+'.png')
    command=[str(args.binary.resolve()),'--config',str(config),'--astronaut-capture',str(path),
        '--render-size','320','240','--benchmark-frames',str(frames),'--benchmark-step','0',*extra]
    if frames>1: command+=['--benchmark-character-step','.04']
    if replay: command+=['--replay',str(replay)]
    result=subprocess.run(command,cwd=root,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    (out/(name+'.log')).write_text(result.stdout)
    assert result.returncode==0,result.stdout
    data=json.loads(Path(str(path)+'.json').read_text())
    assert data['render']['astronaut_pixels']>40,data['render']
    return hashlib.sha256(path.read_bytes()).hexdigest(),data

_,launch=capture('launch',extra=('--benchmark-jump-frame','0'))
assert launch['astronaut_pose']['navigation']
bodies={b['index']:b for b in launch['astronaut_pose']['navigation']['gravity_bodies']}
assert set(bodies)=={0,1,2}

def local(world):
    rotation=scene['planets'][0].get('rotation',{})
    period=rotation.get('period_seconds',0)
    time=launch['surface_camera']['simulation_time_seconds']
    spin=math.radians(rotation.get('phase_deg',0))+(2*math.pi*time/period if period else 0)
    tilt=math.radians(rotation.get('axial_tilt_deg',0))
    x,y,z=[v-c for v,c in zip(world,bodies[1]['position_m'])]
    # Transpose of bodyOrientation = Rx(tilt) Rz(phase+spin).
    y,z=math.cos(tilt)*y+math.sin(tilt)*z,-math.sin(tilt)*y+math.cos(tilt)*z
    return [math.cos(spin)*x+math.sin(spin)*y,-math.sin(spin)*x+math.cos(spin)*y,z]

def seed(name,world):
    data=copy.deepcopy(launch); pose=data['astronaut_pose']; nav=pose['navigation']
    nav['position_m']=world; nav['velocity_mps']=[0,0,0]; nav['outer_space']=True
    nav['reference_body']=1
    newroot=local(world); delta=[b-a for a,b in zip(pose['root'],newroot)]
    pose['root']=newroot; pose['velocity_mps']=[0,0,0]
    pose['boosting']=False; pose['jetpack_armed']=False; pose['thrust_n']=0
    for foot in pose['feet']:
        for key in ('position','start','target','hip','knee','ankle'):
            foot[key]=[v+d for v,d in zip(foot[key],delta)]
    path=out/(name+'-input.json'); path.write_text(json.dumps(data,indent=2)+'\n')
    return path

space=[c+d for c,d in zip(bodies[1]['position_m'],[0,0,4000])]
space_seed=seed('space',space)
space_hash,space_data=capture('space',space_seed)
nav=space_data['astronaut_pose']['navigation']
assert nav['outer_space'] and nav['reference_body']==1,nav
assert len(nav['gravity_body_indices'])==3
assert nav['up']==launch['astronaut_pose']['navigation']['up'],'Space up drifted toward departure planet'
replay_hash,replayed=capture('space-replay',out/'space.png.json',frames=1)
assert replay_hash==space_hash,'Space camera/pose failed exact replay'
assert replayed['astronaut_pose']==space_data['astronaut_pose']

moon=[c+d for c,d in zip(bodies[2]['position_m'],[0,0,1.5*bodies[2]['radius_m']])]
moon_seed=seed('moon',moon)
moon_hash,moon_data=capture('moon',moon_seed)
nav=moon_data['astronaut_pose']['navigation']
assert not nav['outer_space'] and nav['reference_body']==2,nav
assert moon_data['surface_camera']['planet_index']==1,'Surface frame did not switch to Moon'
assert math.dist(nav['position_m'],moon)<1,'Moon handoff teleported the astronaut'
assert moon_data['astronaut_pose']['grass_trail']==[]
replay_hash,replayed=capture('moon-replay',out/'moon.png.json',frames=1)
assert replay_hash==moon_hash,'Moon handoff camera/pose failed exact replay'
assert replayed['astronaut_pose']==moon_data['astronaut_pose']
settled_hash,settled=capture('moon-settled',out/'moon.png.json',frames=61)
nav=settled['astronaut_pose']['navigation']
radial=[v-c for v,c in zip(nav['position_m'],bodies[2]['position_m'])]
norm=math.sqrt(sum(v*v for v in radial)); radial=[v/norm for v in radial]
assert nav['reference_body']==2 and not nav['outer_space']
assert sum(u*r for u,r in zip(nav['up'],radial))>.995,'Moon up did not settle'
assert sum(u*r for u,r in zip(nav['suit_up'],radial))>.99,'Coasting suit did not align to Moon'
replay_hash,replayed=capture('moon-settled-replay',out/'moon-settled.png.json',frames=1)
assert replay_hash==settled_hash,'Settled Moon pose failed exact replay'
assert replayed['astronaut_pose']==settled['astronaut_pose']
print('Validated world-space free orientation, three-source gravity, Moon handoff/settling and exact image/state replay')
