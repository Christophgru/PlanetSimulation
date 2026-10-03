#!/usr/bin/env python3
"""Verify offline radius/detail, visible/occluded lens flare and exact replay."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
from read_png import read_png

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--binary',type=Path,required=True)
p.add_argument('--output-dir',type=Path,required=True)
args=p.parse_args()
root=Path(__file__).resolve().parents[2]
out=args.output_dir.resolve(); out.mkdir(parents=True,exist_ok=True)
scene=json.loads((root/'tests/scenarios/foliage/surface.json').read_text())
earth=scene['planets'][0]
earth.update(color=[.2,.6,.1],surface_noise=[],terrain_landscape={'enabled':False},
             water={'enabled':False},atmosphere={'enabled':False})
earth['orbit'].update(semi_major_axis=10,semi_minor_axis=10)
earth['rotation'].update(axial_tilt_deg=0,phase_deg=0)
earth['terrain_lod'].update(base_edge_segments=1,max_triangle_budget=10000,
                           shoreline_edge_m=0,sink_depth_m=0)
earth['foliage'].update(max_blades=1024,draw_distance_m=30,wind_strength=0)
scene['planets'][1]['surface_noise']=[]
scene['lighting'].update(ambient_light=.2,auto_exposure={'enabled':False},
                         shadows={'enabled':False},reflections_enabled=False)
scene['skybox']['enabled']=False
camera=scene['surface_camera']
camera.update(latitude_deg=0,longitude_deg=135,altitude=.002,
              simulation_time_seconds=0,direction_ned=[0,1,-.5],fov=80)
camera.pop('up_ned',None)
config=out/'scene.json'
config.write_text(json.dumps(scene,indent=2)+'\n')

def capture(name,flags=(),replay=None):
    path=out/(name+'.png')
    cmd=[str(args.binary.resolve()),'--config',str(config),'--surface-capture',str(path),
         '--render-size','480','270',*flags]
    if replay: cmd+=['--replay',str(replay)]
    r=subprocess.run(cmd,cwd=root,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    (out/(name+'.log')).write_text(r.stdout)
    assert r.returncode==0,r.stdout
    assert 'OpenGL error' not in r.stdout,r.stdout
    saved=json.loads(Path(str(path)+'.json').read_text())
    return hashlib.sha256(path.read_bytes()).hexdigest(),saved

_,normal=capture('normal')
on,visible=capture('flare',('--offline-quality',))
off,disabled=capture('no-flare',('--offline-quality','--no-lens-flare'))
v=visible['render']; n=normal['render']
assert v['offline']['enabled'] and v['atmosphere_downsample']==1,v
assert v['effective_foliage_distance_m']==20*n['effective_foliage_distance_m'],v
assert v['effective_foliage_budget']>=n['effective_foliage_budget'],v
assert v['foliage_blades']<=v['effective_foliage_budget'],v
assert v['sun_mesh_triangles']==64*n['sun_mesh_triangles'],v
assert v['body_mesh_triangles'][1]>n['body_mesh_triangles'][1],v
assert v['lens_flare_visible_sun_pixels']>100 and v['lens_flare_strength']>0,v
assert on!=off,'Visible Sun did not produce a flare'
# Require an apparent ghost signal away from the fixture's upper-half Sun.
# A file-hash change alone accepted ghosts too faint to see in the README.
w,h,c,on_pixels=read_png(out/'flare.png')
fw,fh,fc,off_pixels=read_png(out/'no-flare.png')
assert (w,h,c)==(fw,fh,fc)
ghost_pixels=0
ghost_peak=0
for pixel in range(w*(h//2),w*h):
    delta=max(on_pixels[pixel*c+i]-off_pixels[pixel*c+i] for i in range(3))
    ghost_peak=max(ghost_peak,delta)
    ghost_pixels+=delta>=24
assert ghost_pixels>=80,(ghost_pixels,ghost_peak)
# The sidecar stores normal source settings plus the derived profile, so replay
# applies the multiplier exactly once and does not need the original config.
config.unlink()
replayed,restored=capture('replay',replay=out/'flare.png.json')
assert replayed==on,'Offline image did not replay exactly'
assert restored['render']['effective_foliage_distance_m']==v['effective_foliage_distance_m']
smaller,override=capture('override',('--foliage-distance-multiplier','2','--no-lens-flare'),out/'flare.png.json')
assert override['render']['effective_foliage_distance_m']==60
assert override['render']['lens_flare_strength']==0

# Sun projects behind the ground on the far side of the same spherical planet.
camera.update(longitude_deg=0,direction_ned=[.2,0,1])
config.write_text(json.dumps(scene,indent=2)+'\n')
hidden,occluded=capture('occluded',('--offline-quality',))
hidden_off,_=capture('occluded-off',('--offline-quality','--no-lens-flare'))
assert occluded['render']['lens_flare_visible_sun_pixels']==0
assert occluded['render']['lens_flare_strength']==0
assert hidden==hidden_off,'Terrain-occluded Sun leaked a lens flare'
for key,value in (('foliage_distance_multiplier',21),('foliage_distance_multiplier',True),('enabled','yes')):
    invalid=json.loads(json.dumps(visible)); invalid['render']['offline'][key]=value
    bad=out/'invalid-replay.json'; bad.write_text(json.dumps(invalid))
    environment=dict(os.environ); environment.pop('DISPLAY',None)
    result=subprocess.run([str(args.binary.resolve()),'--replay',str(bad),
        '--surface-capture',str(out/'invalid.png')],cwd=root,env=environment,
        text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    assert result.returncode!=0 and 'Invalid scenario or replay:' in result.stdout,result.stdout
    assert 'Failed to initialize GLFW' not in result.stdout,'Invalid replay reached window startup'
evidence={'visible_strength':v['lens_flare_strength'],
          'lower_half_ghost_pixels_above_24':ghost_pixels,
          'lower_half_ghost_peak_channel_delta':ghost_peak,
          'visible_sun_pixels':v['lens_flare_visible_sun_pixels'],
          'normal_body_triangles':n['body_mesh_triangles'],
          'offline_body_triangles':v['body_mesh_triangles'],
          'normal_sun_triangles':n['sun_mesh_triangles'],
          'offline_sun_triangles':v['sun_mesh_triangles'],
          'normal_radius_m':n['effective_foliage_distance_m'],
          'offline_radius_m':v['effective_foliage_distance_m'],
          'offline_candidate_budget':v['effective_foliage_budget'],
          'offline_candidates':v['foliage_candidates'],
          'png_sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in out.glob('*.png')}}
(out/'evidence.json').write_text(json.dumps(evidence,indent=2)+'\n')
print('Validated offline distance/detail, gated lens flare, option overrides and exact replay')
