#!/usr/bin/env python3
"""Excluded private queue-order qualification and production diagnosis."""
import argparse
import csv
import json
from pathlib import Path
import subprocess
import sys
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[6]
sys.path.insert(0,str(ROOT/'tests/app/terrain/native'))
from NativeSession import Session,validate,write_json
from analyze import compare

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--probe',type=Path,required=True)
p.add_argument('--output-dir',type=Path,required=True)
p.add_argument('--production',action='store_true')
p.add_argument('--expected-uuid')
args=p.parse_args();out=args.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
assert not (out/'results.json').exists(),'Use fresh diagnosis outputs'
scene=json.loads((ROOT/('configs/scenarios/solar_system.json' if args.production else 'tests/scenarios/foliage/surface.json')).read_text())
if not args.production:
    for planet in scene['planets']:
        planet['terrain_lod']={**planet.get('terrain_lod',{}),'max_triangle_budget':10000}
        planet['rotation']={'period_seconds':0,'axial_tilt_deg':0};planet['surface_noise']=[]
        planet['terrain_landscape']={'enabled':False};planet['atmosphere']={'enabled':False}
    scene['planets'][0]['color']=[.2,.8,.1]
    scene['planets'][0]['water']={**scene['planets'][0]['water'],'level_m':-10}
    scene['planets'][0]['foliage'].update({'max_blades':10000,'height_m':.6,'draw_distance_m':40})
    scene['lighting']['shadows']['resolution']=256;scene['skybox']['enabled']=False
    scene['surface_camera']['direction_ned']=[1,0,0];scene['surface_camera'].pop('up_ned',None)
scene['surface_camera']['simulation_time_seconds']=0
replay=out/'input.json';write_json(replay,{'scenario':scene,'surface_camera':scene['surface_camera']})
results={}
for backend in ('cpu','compute'):
    folder=out/backend;captures=folder/'captures';trace=folder/'performance.csv'
    flags=['--replay',str(replay),'--terrain-backend',backend,'--terrain-grass-planner','gpu' if backend=='compute' else 'cpu','--performance-trace',str(trace)]
    controls={'wind_mode':'fixed','wind_start_s':12,'raster_mode':'full','qualify_raster':False,'phase':'warmup'}
    session=Session(args.probe,ROOT,folder,flags,environment={'PLANET_NATIVE_BENCHMARK':'1','PLANET_NATIVE_ORDER':str(captures)},control=controls)
    try:
        session.focus()
        if not args.production:subprocess.run(['xdotool','windowsize','--sync',session.window,'320','180'],check=True)
        session.key('4')
        def ready(f):return f['mode']==3 and f.get('pose') and not f['pose']['airborne'] and not f['publication'].get('loading',False)
        first=session.wait(ready,timeout=180)
        session.wait(lambda f:ready(f) and f['frame']>=first['frame']+40,timeout=180)
        folders=[]
        for request,mode in enumerate(('native','near','far'),1):
            session.control({**controls,'phase':mode,'order_request':request,'order_mode':mode,'order_frames':3})
            session.wait(lambda f:(captures/str(request)/'2/snapshot.json').exists(),timeout=180)
            folders.extend(captures/str(request)/str(i) for i in range(3))
        session.control({**controls,'phase':'close'});session.close()
        audit=validate(session.frames,managed=backend=='compute')
        result=compare(folders);result['audit']=audit
        for folder in folders:
            snapshot=json.loads((folder/'snapshot.json').read_text());linked=[f for f in session.frames if f['profile_frame']==snapshot['profile_frame']]
            assert len(linked)==1 and linked[0]['benchmark']['bodies']==snapshot['workload']['bodies']
            assert linked[0]['benchmark']['grass_raster']['control']['wind_mode']=='fixed'
        if args.expected_uuid:
            memory=list(csv.DictReader(Path(str(trace)+'.memory.csv').open()))
            assert memory and all(r['context_uuid']==args.expected_uuid and r['context_status']=='uuid_verified' and r['nvml_status']=='ok' for r in memory)
        else:assert all('llvmpipe' in f['renderer'] for f in session.frames)
        write_json(out/backend/'analysis.json',result)
        results[backend]=result
        print(backend,'qualified',result['frames'],'frames / exact Blade permutations and depth; color discrepancies retained',flush=True)
    finally:session.abort()
write_json(out/'results.json',{'backends':results,'production':args.production,'expected_uuid':args.expected_uuid,'timing_acceptance':False,'migration_acceptance':False})
