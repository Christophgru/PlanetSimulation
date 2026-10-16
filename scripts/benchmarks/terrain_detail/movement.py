"""Native detail coverage receipt; exit 2 when the camera leaves a requested core."""
import argparse, csv, json, math, os, subprocess, sys
from pathlib import Path
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'scripts/benchmarks'))
from archives.local import local_output
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--probe',type=Path,required=True)
parser.add_argument('--output-dir',type=local_output,required=True)
parser.add_argument('--device',type=int,default=1)
args=parser.parse_args()
sys.path.insert(0,str(root/'tests/app/terrain/native'))
from NativeSession import Session,validate,write_json
out=args.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
adapter=out/'libnative_egl.so'
subprocess.run(['c++','-std=c++17','-O2','-shared','-fPIC',str(root/'scripts/benchmarks/grass_allocation/NativeEgl.cpp'),
 '-o',str(adapter),'-lEGL','-lGL','-lGLEW','-ldl'],check=True)
vendor=out/'nvidia.json'
write_json(vendor,{'file_format_version':'1.0.0','ICD':{'library_path':'libEGL_nvidia.so.0'}})
identities=list(csv.DictReader(subprocess.check_output(['nvidia-smi','--query-gpu=index,name,uuid','--format=csv'],text=True).splitlines()))
device=next({k.strip():v.strip() for k,v in d.items()} for d in identities if int(d['index'])==args.device)
environment={**os.environ,'LD_PRELOAD':str(adapter),'PLANET_EGL_DEVICE':str(args.device),
 '__EGL_VENDOR_LIBRARY_FILENAMES':str(vendor),'PLANET_NATIVE_BENCHMARK':'1'}
for name in ('LIBGL_ALWAYS_SOFTWARE','MESA_GL_VERSION_OVERRIDE','MESA_GLSL_VERSION_OVERRIDE'):
 environment.pop(name,None)
scene=json.loads((root/'tests/scenarios/foliage/surface.json').read_text())
for p in scene['planets']:
 p['rotation']={'period_seconds':0,'axial_tilt_deg':0}
 p['atmosphere']={'enabled':False};p.setdefault('water',{})['enabled']=False
 p['terrain_landscape']={'enabled':False};p['surface_noise']=[]
 p['terrain_lod']={**p.get('terrain_lod',{}),'max_triangle_budget':10000}
p=scene['planets'][0];p['color']=[.2,.8,.1]
p['surface_noise']=[{'amplitude_m':.01,'wavelength_m':.25,'octaves':1,'seed':-71}]
p['foliage'].update(density_per_m2=1,max_blades=100000,draw_distance_m=30,quad_distance_m=8,height_m=.6)
p['terrain_lod'].update(base_edge_segments=1,medium_edge_segments=4,max_edge_segments=8,steep_edge_segments=8,
 max_triangle_budget=30000,shoreline_edge_m=0,relief_sinking=True,geometric_error_m=.05,
 local_detail_streaming=True,local_detail_radius_m=.5,local_edge_m=.05,local_error_m=.01,local_transition_m=.5)
scene['lighting']['shadows']['resolution']=256
scene['surface_camera'].update(simulation_time_seconds=0,direction_ned=[1,0,0])
scene['surface_camera'].pop('up_ned',None)
scene['skybox']['enabled']=False
results=[]
for radius,sprint in [(.5,False),(.5,True),(1.5,True)]:
 p['terrain_lod']['local_detail_radius_m']=radius
 folder=out/f'{radius}-{sprint}';folder.mkdir(exist_ok=True)
 replay=folder/'input.json';write_json(replay,{'scenario':scene,'surface_camera':scene['surface_camera']})
 session=Session(args.probe.resolve(),root,folder,
  ['--replay',str(replay),'--terrain-backend','compute','--terrain-grass-planner','gpu','--uncapped'],
  environment=environment)
 try:
  session.focus();subprocess.run(['xdotool','windowsize','--sync',session.window,'320','180'],check=True)
  session.key('4')
  def ready(f):return f['mode']==3 and not f['publication']['loading'] and f.get('pose') and not f['pose']['airborne']
  start=session.wait(lambda f:ready(f) and not f['publication']['pending'] and not any(f['worker'].values()),timeout=120)
  if sprint:session.down('Shift_L')
  session.down('w');end=session.wait(lambda f:ready(f) and f['observed_ns']>=start['observed_ns']+4_000_000_000,timeout=60)
  session.up('w')
  if sprint:session.up('Shift_L')
  session.close();audit=validate(session.frames)
  assert f"EGL device UUID: {device['uuid']}" in session.text()
  assert all(f['renderer'].startswith(device['name']) for f in session.frames)
  frames=[f for f in session.frames if ready(f) and start['observed_ns']<=f['observed_ns']<=end['observed_ns']]
  offsets=[]
  for f in frames:
   a=f['camera_local'];b=f['benchmark']['bodies'][0]['terrain_plan_eye']
   dot=sum(x*y for x,y in zip(a,b))/math.sqrt(sum(x*x for x in a)*sum(x*x for x in b))
   offsets.append(1000*math.acos(max(-1,min(1,dot))))
  result={'radius_m':radius,'sprint':sprint,'frames':len(frames),'maximum_anchor_offset_m':max(offsets),
   'outside_core_frames':sum(x>radius for x in offsets),'speed_mps':(end['pose']['walked_m']-start['pose']['walked_m'])/((end['observed_ns']-start['observed_ns'])*1e-9),
   'publications':[start['publication']['published'],end['publication']['published']],'audit':audit}
  results.append(result);write_json(out/'results.json',results);print(result,flush=True)
 finally:session.abort()

if any(r['outside_core_frames'] for r in results):
 print('Detail coverage gate failed; see outside_core_frames and maximum_anchor_offset_m.',flush=True)
 raise SystemExit(2)
print('Detail coverage gate passed.',flush=True)
