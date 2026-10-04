#!/usr/bin/env python3
"""Exercise opt-in terrain generation, replay, CPU override and GL 3.3 fallback."""
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
cpu,c=run('cpu');gpu,g=run('gpu',('--terrain-backend','compute'))
assert c['render']['terrain_backend']=='cpu' and g['render']['terrain_backend']=='compute'
assert g['render']['terrain_contract']['backend']=='compute'
assert not g['render']['terrain_compute']['cpu_compatibility_mirror']
assert g['render']['terrain_compute']['cpu_render_bytes']==0
assert g['render']['terrain_compute']['cpu_water_render_bytes']==0
assert g['render']['terrain_contract']['evaluation_requests']==0
assert g['render']['terrain_contract']['evaluation_evaluations']==0
assert g['render']['terrain_grass_planner']=='gpu-v1'
assert g['render']['foliage_patch_bytes']==0
metadata=g['render']['foliage_gpu_metadata']
assert metadata['version']==1 and metadata['allocator']=='gpu-v1'
assert metadata['resident_bytes']==160+g['render']['body_mesh_triangles'][0]*64
assert metadata['input_bytes']==160+8*metadata['dispatches']
assert metadata['diagnostic_read_bytes']==0
assert metadata['summary_read_bytes']==224
assert metadata['effective_candidate_budget']==4096
assert metadata['placement_density_per_m2']>0
assert metadata['allocation_input_bytes']==228+12*metadata['allocation_dispatches']
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
override,o=run('cpu-override',('--terrain-backend','cpu'),gpu.with_suffix('.png.json'))
assert o['render']['terrain_backend']=='cpu' and override.read_bytes()==cpu.read_bytes()
walker,w=run('walking',('--terrain-backend','compute','--benchmark-frames','6','--benchmark-walk-step','.06','--benchmark-step','0'),astronaut=True)
assert w['render']['astronaut_pixels']>150 and w['astronaut_pose']['grass_trail']
contacts=w['render']['terrain_contacts']
assert contacts['backend']=='sparse-oracle' and contacts['height_evaluations']>0
assert contacts['resident_positions']<=contacts['position_capacity']==1024
assert contacts['height_evaluations']<w['render']['terrain_contract']['unique_samples']//4
walkingReplay,wr=run('walking-replay',replay=walker.with_suffix('.png.json'),astronaut=True)
assert wr['render']['terrain_backend']=='compute' and walkingReplay.read_bytes()==walker.read_bytes()
legacy,l=run('legacy-compute',('--terrain-backend','compute','--terrain-grass-planner','cpu'))
assert l['render']['terrain_compute']['cpu_compatibility_mirror']
assert l['render']['terrain_contract']['evaluation_requests']>0
assert l['render']['terrain_grass_planner']=='cpu'
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
oldgl={'MESA_GL_VERSION_OVERRIDE':'3.3','MESA_GLSL_VERSION_OVERRIDE':'330'}
fallback,f=run('gl33-fallback',('--terrain-backend','compute'),env=oldgl)
assert f['render']['terrain_backend']=='cpu' and '4.3' in f['render']['terrain_fallback']
assert not f['render']['terrain_compute']['cpu_compatibility_mirror']
assert f['render']['foliage_gpu_metadata']['resident_bytes']==0
error=run('gl33-locked-rejected',replay=gpu.with_suffix('.png.json'),env=oldgl,fail=True)
assert 'Locked compute replay' in error
invalid=out/'invalid.json';bad=json.loads(gpu.with_suffix('.png.json').read_text());bad['render']['terrain_backend']='invalid'
invalid.write_text(json.dumps(bad));error=run('invalid-rejected',replay=invalid,fail=True)
assert 'Invalid terrain replay backend' in error and 'initialized' not in error
bad=json.loads(gpu.with_suffix('.png.json').read_text());bad['render']['terrain_contract']['field_version']=2
invalid.write_text(json.dumps(bad));error=run('version-rejected',replay=invalid,fail=True)
assert 'Unsupported terrain compute replay versions' in error and 'initialized' not in error
bad=json.loads(gpu.with_suffix('.png.json').read_text());bad['render']['terrain_grass_planner']='gpu-v2'
invalid.write_text(json.dumps(bad));error=run('planner-version-rejected',replay=invalid,fail=True)
assert 'Unsupported terrain grass replay planner' in error and 'initialized' not in error
report={'cpu_gpu_surface':compare(cpu,gpu),'compute_replay_exact':True,'compute_walking_replay_exact':True,
        'cpu_override_exact':True,'legacy_compute_replay_exact':True,'vertex_config_fallback':True,'locked_vertex_config_rejected':True,'unknown_grass_planner_rejected_before_window':True,'gl33_fallback':True,'locked_unavailable_rejected':True,'invalid_backend_rejected_before_window':True,
        'unknown_field_version_rejected_before_window':True,'terrain_compute':g['render']['terrain_compute'],'commands':commands,
        'sparse_contacts':contacts,'grass_metadata':metadata,
        'png_sha256':{q.name:hashlib.sha256(q.read_bytes()).hexdigest() for q in out.glob('*.png')}}
(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS GPU terrain/main/reflection/foliage/standing/walking, exact locked replay, CPU override and GL 3.3 fallback')
