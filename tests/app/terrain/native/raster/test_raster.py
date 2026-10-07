#!/usr/bin/env python3
"""Excluded pipeline-query qualification of private wind/raster controls."""
import argparse
import copy
import csv
import json
from pathlib import Path
import sys
import subprocess
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from NativeSession import Session, validate, write_json

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--probe',type=Path,required=True)
p.add_argument('--output-dir',type=Path,required=True)
p.add_argument('--expected-uuid')
args=p.parse_args()
root=Path(__file__).resolve().parents[5]
out=args.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
scene=json.loads((root/'tests/scenarios/foliage/surface.json').read_text())
for planet in scene['planets']:
    planet['terrain_lod']={**planet.get('terrain_lod',{}),'max_triangle_budget':10000}
    planet['rotation']={'period_seconds':0,'axial_tilt_deg':0}
    planet['surface_noise']=[];planet['terrain_landscape']={'enabled':False}
    planet['atmosphere']={'enabled':False}
scene['planets'][0]['color']=[.2,.8,.1]
scene['planets'][0]['water']={**scene['planets'][0]['water'],'level_m':-10}
scene['planets'][0]['foliage'].update({'max_blades':10000,'height_m':.6,'draw_distance_m':40})
scene['lighting']['shadows']['resolution']=256;scene['skybox']['enabled']=False
scene['surface_camera']['direction_ned']=[1,0,0];scene['surface_camera'].pop('up_ned',None)
scene['surface_camera']['simulation_time_seconds']=0
results=[]
for name,backend,placement in [('legacy','cpu',True),('resident','compute',True),('vertex','cpu',False)]:
    case=copy.deepcopy(scene);case['planets'][0]['foliage']['compute_placement']=placement
    replay=out/(name+'.json');write_json(replay,{'scenario':case,'surface_camera':case['surface_camera']})
    folder=out/name
    flags=['--replay',str(replay),'--terrain-backend',backend,'--terrain-grass-planner',
        'gpu' if backend=='compute' else 'cpu','--performance-trace',str(folder/'performance.csv')]
    session=Session(args.probe,root,folder,flags,environment={'PLANET_NATIVE_BENCHMARK':'1'})
    try:
        session.focus()
        subprocess.run(['xdotool','windowsize','--sync',session.window,'320','180'],check=True)
        session.key('4')
        def ready(f):return f['mode']==3 and f.get('pose') and not f['pose']['airborne'] and not f['publication'].get('loading',False)
        first=session.wait(ready,timeout=90)
        session.wait(lambda f:ready(f) and f['frame']>=first['frame']+30,timeout=90)
        samples={}
        for wind in ('fixed','indexed','native'):
            for mode in ('full','discard','suppress'):
                phase=wind+'-'+mode
                control={'phase':phase,'wind_mode':wind,'raster_mode':mode,'wind_start_s':12,
                    'wind_step_s':.125,'qualify_raster':True}
                session.control(control)
                def eligible(f):
                    r=f.get('benchmark',{}).get('grass_raster',{})
                    return ready(f) and r.get('control',{}).get('phase')==phase and r.get('events')
                session.wait(lambda f:eligible(f) and f['benchmark']['grass_raster']['control']['sequence']>=3,timeout=90)
                rows=[f for f in session.frames if eligible(f) and f['benchmark']['grass_raster']['control']['sequence']<4]
                assert len(rows)==4
                values=[]
                for f in rows:
                    r=f['benchmark']['grass_raster'];sequence=r['control']['sequence']
                    assert r['control']['qualification'] and not r['timing_acceptance']
                    assert sum(r['other_draw_calls'].values())>0
                    events=[e for e in r['events'] if e['body']==0]
                    assert {e['view'] for e in events}=={'main','reflection'}
                    for e in events:
                        expected=e['original_time_s'] if wind=='native' else 12+(sequence*.125 if wind=='indexed' else 0)
                        assert e['effective_time_s']==e['verified_blade_time_s']==expected
                        assert e['original_time_s']==e['original_blade_time_s']
                        assert e['blocking_qualification_query_reads']==2 and e['draws']
                        assert all(d['submitted']==(mode!='suppress') for d in e['draws'])
                        assert all(d['kind']==('indirect' if placement else 'instanced') for d in e['draws'])
                        assert e['placement_time_writes']==int(placement)
                        if placement:assert e['placement_time_s']==e['verified_placement_time_s']==expected
                        if mode=='suppress':assert e['generated_primitives']==e['passed_samples']==0
                        else:assert e['generated_primitives']>0
                        if mode=='discard':assert e['passed_samples']==0
                    if mode=='full':assert next(e for e in events if e['view']=='main')['passed_samples']>0
                    values.append([(e['view'],e['generated_primitives'],e['passed_samples'],len(e['draws'])) for e in events])
                samples[wind,mode]=values
                results.append({'case':name,'wind_mode':wind,'raster_mode':mode,'frames':4,'pipeline_results':values})
        for wind in ('fixed','indexed'):
            assert [[(view,primitives,count) for view,primitives,_,count in row] for row in samples[wind,'full']]==[
                [(view,primitives,count) for view,primitives,_,count in row] for row in samples[wind,'discard']]
        session.control({'phase':'close'});session.close()
        validate(session.frames,managed=backend=='compute')
        if args.expected_uuid:
            rows=list(csv.DictReader((folder/'performance.csv.memory.csv').open()))
            assert rows and all(r['context_uuid']==args.expected_uuid and r['context_status']=='uuid_verified' and r['nvml_status']=='ok' for r in rows)
        else:assert all('llvmpipe' in f['renderer'] for f in session.frames)
        write_json(folder/'result.json',{'native_audit':True,'qualification_only':True})
    finally:session.abort()
write_json(out/'results.json',{'cases':results,'frames':len(results)*4,'timing_acceptance':False})
print('Qualified',len(results),'controls /',len(results)*4,'frames; placement, vertex and fragment outcomes independently queried')
