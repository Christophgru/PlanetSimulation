#!/usr/bin/env python3
"""Exercise default compute terrain, replay, CPU override and GL 3.3 fallback."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'quality'))
from read_png import read_png
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--binary',type=Path,required=True)
p.add_argument('--output-dir',type=Path,required=True)
args=p.parse_args()
root=Path(__file__).resolve().parents[3];out=args.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
scene=json.loads((root/'tests/scenarios/foliage/surface.json').read_text())
for planet in scene['planets']:
    planet.setdefault('terrain_lod', {})['relief_sinking'] = True
    planet['terrain_lod']['geometric_error_m'] = .05
    planet.setdefault('terrain_lod',{})['max_triangle_budget']=10000
scene['planets'][0]['foliage']['max_blades']=4096
config=out/'scene.json';config.write_text(json.dumps(scene,indent=2)+'\n')
commands=[]
def run(name,flags=(),replay=None,env=None,fail=False,astronaut=False,scene_path=None):
    target=out/(name+'.png');target.unlink(missing_ok=True)
    cmd=[str(args.binary.resolve()),'--config',str(scene_path or config),
         '--astronaut-capture' if astronaut else '--surface-capture',str(target),'--render-size','480','270',*flags]
    if replay:cmd+=['--replay',str(replay)]
    result=subprocess.run(cmd,cwd=root,env={**os.environ,**(env or {})},text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    (out/(name+'.log')).write_text(result.stdout);commands.append({'command':cmd,'env':env or {},'returncode':result.returncode})
    if fail:
        assert result.returncode!=0 and not target.exists(),result.stdout
        return result.stdout
    assert result.returncode==0,result.stdout
    assert 'OpenGL error' not in result.stdout,result.stdout
    return target,json.loads(target.with_suffix('.png.json').read_text())
def compare(a,b):
    aw,ah,ac,ap=read_png(a);bw,bh,bc,bp=read_png(b);assert (aw,ah,ac)==(bw,bh,bc)
    delta=[abs(x-y) for x,y in zip(ap,bp)]
    # Numerical parity is required across drivers; exact PNG parity is retained when observed.
    assert sum(delta)/len(delta)<.1 and max(delta)<32,(sum(delta)/len(delta),max(delta))
    return {'exact':a.read_bytes()==b.read_bytes(),'mean_channel_error':sum(delta)/len(delta),'max_channel_error':max(delta)}
cpu,c=run('cpu',('--terrain-backend','cpu'));gpu,g=run('gpu')
def publication(data):
    state=data['render']['terrain_publication']
    assert state['managed'] and not state['pending'] and state['failed']==state['obsolete']==0
    for consumer in state['consumers']:
        assert consumer['land']==consumer['grass']==consumer['contacts']
        assert consumer['land_revision']==consumer['grass_revision']==consumer['main_revision']>0
        if consumer['shadows_enabled']: assert consumer['shadow_revision']==consumer['land_revision']
        if any(p['water']['enabled'] for p in data['scenario']['planets']):
            assert consumer['reflection_revision']==consumer['land_revision']
        if consumer['water_enabled']: assert consumer['water_draw_revision']==consumer['water_revision']
    return state
published=publication(g)
assert not c['render']['terrain_publication']['managed']
assert c['render']['terrain_backend']=='cpu' and g['render']['terrain_backend']=='compute'
assert g['render']['terrain_contract']['backend']=='compute'
assert g['render']['terrain_contract']['topology_version']==2
assert not g['render']['terrain_compute']['cpu_compatibility_mirror']
assert g['render']['terrain_compute']['cpu_render_bytes']==0
assert g['render']['terrain_compute']['cpu_water_render_bytes']==0
assert g['render']['terrain_contract']['evaluation_requests']==0
assert g['render']['terrain_contract']['evaluation_evaluations']==0
assert g['render']['terrain_grass_planner']=='gpu-v1'
worker=g['render']['terrain_cpu_worker']
assert worker['submitted']==worker['completed']>0
assert worker['peak_running']==worker['peak_queued']==1
assert not worker['pending'] and worker['failed']==worker['obsolete']==worker['rejected_completions']==0
assert c['render']['terrain_cpu_worker']['submitted']==0
assert g['render']['foliage_patch_bytes']==0
metadata=g['render']['foliage_gpu_metadata']
assert metadata['version']==1 and metadata['allocator']=='gpu-v1'
assert metadata['resident_bytes']==160+g['render']['body_mesh_triangles'][0]*64
assert metadata['input_bytes']==160+8*metadata['dispatches']
assert metadata['diagnostic_read_bytes']==0
assert metadata['summary_read_bytes']==224
assert 0<metadata['effective_candidate_budget']<=4096
policy=g['render']['foliage_policy'][0]
assert c['render']['foliage_policy']==g['render']['foliage_policy']
assert policy['version']==1 and policy['body']==scene['planets'][0]['name']
assert policy['capacity']==4096 and policy['budget']==metadata['effective_candidate_budget']
assert policy['protected_m']==10 and policy['density']==metadata['placement_density_per_m2']
assert policy['near_infeasible'] or policy['density']==scene['planets'][0]['foliage']['density_per_m2']
assert 0<=policy['sigma_m']<=scene['planets'][0]['foliage']['draw_distance_m']/3*(1+1e-6)
assert metadata['placement_density_per_m2']>0
assert metadata['draw_resources_prepared']
assert metadata['draw_resource_bytes']==128*g['render']['foliage_candidates']+32
assert 0<metadata['stage_admitted_bytes']<=512*1024*1024
assert 0<g['render']['terrain_compute']['stage_admitted_bytes']<=512*1024*1024
assert metadata['allocation_input_bytes']==228+12*metadata['allocation_dispatches']+4*(g['render']['body_mesh_triangles'][0]-1).bit_length()
assert metadata['allocation_bytes']>0
assert g['render']['foliage_candidates']<=4096
assert c['render']['foliage_gpu_metadata']['resident_bytes']==0
assert g['render']['terrain_contacts']['backend']=='sparse-oracle'
assert c['render']['terrain_contacts']['backend']=='cpu-mesh'
assert g['render']['terrain_contacts']['height_evaluations']==0
for key in ('field_fingerprint','topology_fingerprint'):
    assert g['render']['terrain_contacts'][key]==g['render']['terrain_contract'][key]
assert g['render']['terrain_compute']['input_bytes']<g['render']['body_mesh_triangles'][0]*120*.25
for key in ('field_fingerprint','topology_fingerprint','unique_samples'):
    assert c['render']['terrain_contract'][key]==g['render']['terrain_contract'][key]
replay,r=run('gpu-replay',replay=gpu.with_suffix('.png.json'))
assert r['render']['terrain_backend']=='compute' and replay.read_bytes()==gpu.read_bytes(),'Compute replay changed PNG'
assert r['render']['foliage_policy']==g['render']['foliage_policy']
override,o=run('cpu-override',('--terrain-backend','cpu'),gpu.with_suffix('.png.json'))
assert o['render']['terrain_backend']=='cpu' and override.read_bytes()==cpu.read_bytes()
assert o['render']['foliage_policy']==g['render']['foliage_policy']
cpuReplay,cr=run('cpu-replay',replay=cpu.with_suffix('.png.json'))
assert cpuReplay.read_bytes()==cpu.read_bytes() and cr['render']['foliage_policy']==c['render']['foliage_policy']
historical=out/'historical-cpu.json';saved=json.loads(cpu.with_suffix('.png.json').read_text())
saved['render'].pop('terrain_backend');historical.write_text(json.dumps(saved))
historicalReplay,hr=run('historical-cpu-replay',replay=historical)
assert hr['render']['terrain_backend']=='cpu' and historicalReplay.read_bytes()==cpu.read_bytes()
walker,w=run('walking',('--terrain-backend','compute','--benchmark-frames','6','--benchmark-walk-step','.06','--benchmark-step','0'),astronaut=True)
assert w['render']['astronaut_pixels']>150 and w['astronaut_pose']['grass_trail']
contacts=w['render']['terrain_contacts']
assert contacts['backend']=='sparse-oracle' and contacts['height_evaluations']>0
assert contacts['resident_positions']<=contacts['position_capacity']==1024
assert contacts['height_evaluations']<w['render']['terrain_contract']['unique_samples']//4
walkingReplay,wr=run('walking-replay',replay=walker.with_suffix('.png.json'),astronaut=True)
walkingPublication=publication(w);publication(wr);publication(r)
assert walkingPublication['character_previews']==6
assert walkingPublication['astronaut_contact_revision']==walkingPublication['consumers'][0]['land_revision']
assert w['astronaut_pose']['grass_plan_eye']==walkingPublication['consumers'][0]['grass_plan_eye']
assert wr['render']['terrain_backend']=='compute' and walkingReplay.read_bytes()==walker.read_bytes()
legacy,l=run('legacy-compute',('--terrain-backend','compute','--terrain-grass-planner','cpu'))
assert l['render']['terrain_compute']['cpu_compatibility_mirror']
assert l['render']['terrain_contract']['evaluation_requests']>0
assert l['render']['terrain_cpu_worker']['submitted']==l['render']['terrain_cpu_worker']['completed']>0
assert l['render']['terrain_grass_planner']=='cpu'
assert not l['render']['terrain_publication']['managed']
old=out/'legacy.json';saved=json.loads(legacy.with_suffix('.png.json').read_text());saved['render'].pop('terrain_grass_planner');old.write_text(json.dumps(saved))
oldReplay,lr=run('legacy-replay',replay=old)
assert lr['render']['terrain_grass_planner']=='cpu' and oldReplay.read_bytes()==legacy.read_bytes()
vertexScene=out/'vertex-scene.json';vertexData=json.loads(config.read_text());vertexData['planets'][0]['foliage']['compute_placement']=False;vertexScene.write_text(json.dumps(vertexData))
vertex,v=run('vertex-planner-fallback',('--terrain-backend','compute'),scene_path=vertexScene)
assert v['render']['terrain_grass_planner']=='cpu' and v['render']['terrain_compute']['cpu_compatibility_mirror']
assert v['render']['foliage_gpu_metadata']['allocation_bytes']==0
incompatible=out/'incompatible.json';bad=json.loads(gpu.with_suffix('.png.json').read_text());bad['scenario']['planets'][0]['foliage']['compute_placement']=False;incompatible.write_text(json.dumps(bad))
error=run('locked-vertex-rejected',replay=incompatible,fail=True)
assert 'Locked GPU grass replay requires compute_placement' in error
oldgl={'MESA_GL_VERSION_OVERRIDE':'3.3','MESA_GLSL_VERSION_OVERRIDE':'330',
       '__GLX_VENDOR_LIBRARY_NAME':'mesa','__NV_PRIME_RENDER_OFFLOAD':'0'}
fallback,f=run('gl33-fallback',env=oldgl)
assert f['render']['terrain_backend']=='cpu' and '4.3' in f['render']['terrain_fallback']
assert not f['render']['terrain_compute']['cpu_compatibility_mirror']
assert f['render']['foliage_gpu_metadata']['resident_bytes']==0
assert not f['render']['terrain_publication']['managed']
assert f['render']['foliage_policy']==c['render']['foliage_policy']
fallbackReplay,fr=run('gl33-replay',replay=fallback.with_suffix('.png.json'),env=oldgl)
assert fallbackReplay.read_bytes()==fallback.read_bytes()
fallbackOverride,fo=run('gl33-cpu-override',('--terrain-backend','cpu'),gpu.with_suffix('.png.json'),env=oldgl)
assert fo['render']['foliage_policy']==g['render']['foliage_policy']
assert fallbackOverride.read_bytes()==fallback.read_bytes()
error=run('gl33-locked-rejected',replay=gpu.with_suffix('.png.json'),env=oldgl,fail=True)
assert 'Locked compute replay' in error
invalid=out/'invalid.json';bad=json.loads(gpu.with_suffix('.png.json').read_text());bad['render']['terrain_backend']='invalid'
invalid.write_text(json.dumps(bad));error=run('invalid-rejected',replay=invalid,fail=True)
assert 'Invalid terrain replay backend' in error and 'initialized' not in error
bad=json.loads(gpu.with_suffix('.png.json').read_text());bad['render']['terrain_contract']['field_version']=2
invalid.write_text(json.dumps(bad));error=run('version-rejected',replay=invalid,fail=True)
assert 'Unsupported terrain compute replay versions' in error and 'initialized' not in error
bad=json.loads(gpu.with_suffix('.png.json').read_text());bad['render']['terrain_contract']['topology_version']=4
invalid.write_text(json.dumps(bad));error=run('topology-version-rejected',replay=invalid,fail=True)
assert 'Unsupported terrain compute replay versions' in error and 'initialized' not in error
bad=json.loads(gpu.with_suffix('.png.json').read_text());bad['render']['terrain_grass_planner']='gpu-v2'
invalid.write_text(json.dumps(bad));error=run('planner-version-rejected',replay=invalid,fail=True)
assert 'Unsupported terrain grass replay planner' in error and 'initialized' not in error
# Both planners now use protected falloff; locked policy replay remains exact.
for key,value in [('version',2),('budget',policy['capacity']+1),('density',4097),('sigma_m',-1),('body','other')]:
    bad=json.loads(gpu.with_suffix('.png.json').read_text());bad['render']['foliage_policy'][0][key]=value
    invalid.write_text(json.dumps(bad));error=run('policy-'+key+'-rejected',replay=invalid,fail=True)
    assert 'foliage policy replay' in error.lower(),error
tinyData=json.loads(config.read_text());tinyData['planets'][0]['foliage']['max_blades']=2
tinyPath=out/'tiny-scene.json';tinyPath.write_text(json.dumps(tinyData))
tiny,t=run('tiny-budget',('--terrain-backend','compute'),scene_path=tinyPath)
assert t['render']['foliage_policy'][0]['near_infeasible']
assert t['render']['foliage_candidates']<=2
tinyReplay,tr=run('tiny-replay',replay=tiny.with_suffix('.png.json'),scene_path=tinyPath)
assert tinyReplay.read_bytes()==tiny.read_bytes()
assert tr['render']['foliage_policy']==t['render']['foliage_policy']
tinyCpu,tc=run('tiny-cpu-budget',scene_path=tinyPath)
assert tc['render']['foliage_policy']==t['render']['foliage_policy']
assert tc['render']['foliage_candidates']<=2
report={'cpu_gpu_surface':compare(cpu,gpu),'cpu_legacy_gpu_surface':compare(cpu,legacy),'adaptive_policy':policy,'cpu_replay_exact':True,'gl33_policy_replay_exact':True,'compute_replay_exact':True,'compute_walking_replay_exact':True,
        'cpu_override_exact':True,'legacy_compute_replay_exact':True,'vertex_config_fallback':True,'locked_vertex_config_rejected':True,'unknown_grass_planner_rejected_before_window':True,'gl33_fallback':True,'locked_unavailable_rejected':True,'invalid_backend_rejected_before_window':True,
        'unknown_field_version_rejected_before_window':True,'terrain_compute':g['render']['terrain_compute'],'commands':commands,
        'sparse_contacts':contacts,'grass_metadata':metadata,'terrain_cpu_worker':worker,
        'terrain_publication':published,'walking_publication':walkingPublication,
        'png_sha256':{q.name:hashlib.sha256(q.read_bytes()).hexdigest() for q in out.glob('*.png')}}
(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
# Version 3 exercises physical micro-relief, fine topology and shared consumers.
detail=json.loads(config.read_text())
planet=detail['planets'][0]
planet['surface_noise']=[{'amplitude_m':.01,'wavelength_m':.25,'octaves':1,'seed':-71}]
planet['terrain_landscape']['elevation_offset_m']=1.5 # Keep walking on land after replacing broad noise.
planet['terrain_lod'].update(local_detail_radius_m=.25,local_edge_m=.05,
                             local_error_m=.01,local_transition_m=.5,
                             max_triangle_budget=30000,shoreline_edge_m=0)
detailPath=out/'detail-scene.json';detailPath.write_text(json.dumps(detail))
detailGpu,dg=run('detail-gpu',scene_path=detailPath)
detailCpu,dc=run('detail-cpu',('--terrain-backend','cpu'),scene_path=detailPath)
contract=dg['render']['terrain_contract']
assert contract['topology_version']==3 and contract['local_detail_triangles']>0
assert not contract['local_detail_limited'] and contract['local_max_edge_m']<=.05
assert contract['local_remaining_error_ratio']<=1 and contract['evaluation_requests']==0
publication(dg)
detailReplay,dr=run('detail-replay',replay=detailGpu.with_suffix('.png.json'))
assert detailReplay.read_bytes()==detailGpu.read_bytes()
detailOverride,do=run('detail-override',('--terrain-backend','cpu'),detailGpu.with_suffix('.png.json'))
assert detailOverride.read_bytes()==detailCpu.read_bytes()
detailOrbit,orb=run('detail-orbit-steps',('--benchmark-frames','6','--benchmark-step','.016666666667'),scene_path=detailPath)
detailOrbitReplay,orr=run('detail-orbit-replay',replay=detailOrbit.with_suffix('.png.json'))
assert orb['render']['terrain_contract']['topology_fingerprint']==orr['render']['terrain_contract']['topology_fingerprint']
assert detailOrbitReplay.read_bytes()==detailOrbit.read_bytes()
bad=json.loads(detailOrbit.with_suffix('.png.json').read_text())
bad['render']['terrain_plan_eyes_world_units'][0]=[0,0,0]
invalid.write_text(json.dumps(bad));error=run('detail-invalid-anchor',replay=invalid,fail=True)
assert 'Terrain planning anchors' in error and 'initialized' not in error
detailFallback,df=run('detail-gl33',env=oldgl,scene_path=detailPath)
assert df['render']['terrain_backend']=='cpu' and df['render']['terrain_contract']['topology_version']==3
detailWalker,dw=run('detail-walking',('--benchmark-frames','6','--benchmark-walk-step','.06',
                                   '--benchmark-step','0'),astronaut=True,scene_path=detailPath)
publication(dw)
assert dw['render']['terrain_contract']['topology_version']==3
assert dw['render']['terrain_contacts']['backend']=='sparse-oracle'
assert dw['render']['terrain_contacts']['height_evaluations']>0 and dw['astronaut_pose']['grass_trail']
detailWalkingReplay,dwr=run('detail-walking-replay',replay=detailWalker.with_suffix('.png.json'),astronaut=True)
publication(dwr)
assert detailWalkingReplay.read_bytes()==detailWalker.read_bytes()
report.update(detail_cpu_gpu=compare(detailCpu,detailGpu),detail_contract=contract,
              detail_replay_exact=True,detail_cpu_override_exact=True,detail_gl33_fallback=True,
              detail_walking_replay_exact=True,detail_orbit_replay_exact=True)
report['png_sha256']={q.name:hashlib.sha256(q.read_bytes()).hexdigest() for q in out.glob('*.png')}
(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS GPU terrain/main/reflection/foliage/standing/walking, centimeter detail, exact locked replay, CPU override and GL 3.3 fallback')
