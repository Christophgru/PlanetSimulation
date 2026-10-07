#!/usr/bin/env python3
"""Post-measurement coverage decomposition and stationary-stage diagnostics."""
import csv
import gzip
import io
import json
import math
import lzma
from pathlib import Path
import numpy as np

ROOT=Path(__file__).resolve().parent
DATA=ROOT/'validation'


def read(path):
    if not path.exists():path=next(p for p in (Path(str(path)+'.gz'),Path(str(path)+'.xz')) if p.exists())
    raw=path.read_bytes()
    return gzip.decompress(raw) if path.suffix=='.gz' else lzma.decompress(raw) if path.suffix=='.xz' else raw


def j(path):return json.loads(read(path))


def coverage(folder):
    control=j(folder/'matched/controls.json');w,h=control['viewport']
    area=np.frombuffer(read(folder/'matched/ground/eligible.rgba32f'),'<f4').reshape(h,w,4).astype(float)
    plane=np.frombuffer(read(folder/'matched/ground/plane.rgba32f'),'<f4').reshape(h,w,4).astype(float)
    stencil=np.frombuffer(read(folder/'matched/images/bare.stencil'),np.uint8).reshape(h,w)
    radius=np.linalg.norm(area[...,:3],axis=2)
    native=j(folder/'snapshot.json');foliage=native['workload']['bodies'][0]['foliage']
    roots=[]
    for name in ('detailed','quads'):
        roots.append(np.frombuffer(read(folder/'matched/grass'/(name+'.blades')),'<f4').reshape(-1,16))
    return control,area,plane,stencil,radius,native,foliage,roots


def percentile(values):
    v=sorted(values)
    return {'mean':sum(v)/len(v), 'p50':v[math.ceil(.5*len(v))-1], 'p95':v[math.ceil(.95*len(v))-1], 'max':v[-1]}


def analytic_area(control,area,plane):
    # Sensitivity only: exact differential Jacobian at the pixel center on the
    # saved live triangle plane, removing finite-quad area approximation.
    w,h=control['viewport'];scale=control['meters_per_radius']
    matrices={n:np.array(control[n]).reshape(4,4,order='F') for n in ('model','view','projection')}
    vm=matrices['view']@matrices['model'];model_radius=np.linalg.norm(matrices['model'][0,:3])
    body=area[...,:3]/scale+(np.array(control['astronaut']['root'])/scale).astype(np.float32)
    camera=body@vm[:3,:3].T+vm[:3,3]
    normals=plane[...,:3]@vm[:3,:3].T/model_radius
    normal_length=np.linalg.norm(normals,axis=2)
    normals=np.divide(normals,normal_length[...,None],out=np.zeros_like(normals),where=normal_length[...,None]>0)
    depth=-camera[...,2]
    ray=np.divide(camera,depth[...,None],out=np.zeros_like(camera),where=depth[...,None]>0)
    product=np.abs(np.sum(normals*ray,axis=2))
    jacobian=np.divide(4*(depth*scale/model_radius)**2,w*h*abs(matrices['projection'][0,0]*matrices['projection'][1,1])*product,
        out=np.zeros_like(depth),where=(depth>0)&(product>1e-12)&(plane[...,3]>0))
    retention=np.divide(area[...,3],plane[...,3],out=np.zeros_like(depth),where=plane[...,3]>0)
    return jacobian*retention


def diagnose():
    report={'scope':'Post hoc diagnostics; declared acceptance tolerances remain unchanged','coverage':[],'stationary':[]}
    for name in ('sprint-25','walking-25','sprint-350','walking-350'):
        for pair in range(1,4):
            cases=[coverage(DATA/'production'/name/f'pair-{pair}'/b/'coverage/1') for b in ('cpu','compute')]
            assert cases[0][0]==cases[1][0]
            summary={'case':name,'pair':pair,'backends':{},'bands':[]}
            masks=[]
            analytic=[]
            for backend,(control,area,plane,stencil,radius,native,foliage,roots) in zip(('cpu','compute'),cases):
                mask=(stencil==2)&(area[...,3]>0)
                masks.append(mask)
                analytic.append(analytic_area(control,area,plane))
                root=np.array(control['astronaut']['root']);scale=control['meters_per_radius']
                root_sets=[]
                for lo,hi in ((0,5),(5,15),(15,30)):
                    selected=[]
                    for values in roots:
                        d=np.linalg.norm(values[:,:3].astype(float)*scale-root,axis=1)
                        selected.extend(bytes(v) for v in np.ascontiguousarray(values[(d>=lo)&(d<hi),:3]).view('V12').ravel())
                    root_sets.append(set(selected))
                summary['backends'][backend]={'native_distance_m':native['walked_m'],
                    'native_root_from_common_root_m':math.dist(native['root_m'],root),
                    'root_to_plan_eye_m':math.dist(native['root_m'],[v*scale for v in foliage['plan_eye']]),
                    'effective_density':foliage['density'],'configured_density':foliage['configured_density'],
                    'field':native['land_field'],'topology':native['land_topology'],'revision':native['land_revision'],
                    'final_exposures':j(DATA/'production'/name/f'pair-{pair}'/backend/'coverage/1/matched/receipt.json')['final_exposures']}
                summary['backends'][backend]['root_sets']=root_sets
            for index,(lo,hi) in enumerate(((0,5),(5,15),(15,30))):
                selections=[mask&(case[4]>=lo)&(case[4]<hi) for mask,case in zip(masks,cases)]
                a,b=selections;overlap=a&b;lost=a&~b;gained=b&~a
                weights=[case[1][...,3] for case in cases]
                offset_difference=np.linalg.norm(cases[0][1][...,:3]-cases[1][1][...,:3],axis=2)
                row={'band_m':[lo,hi],'cpu_only_eligible_pixels':int(lost.sum()),'compute_only_eligible_pixels':int(gained.sum()),
                    'common_eligible_pixels':int(overlap.sum()),'cpu_only_weighted_m2':float(weights[0][lost].sum()),
                    'compute_only_weighted_m2':float(weights[1][gained].sum()),
                    'common_pixel_weight_delta_m2':float((weights[1]-weights[0])[overlap].sum()),
                    'surface_offset_difference_m':percentile(offset_difference[overlap].tolist()),
                    'analytic_jacobian_eligible_m2':{key:float(weight[selection].sum()) for key,weight,selection in
                                                   zip(('cpu','compute'),analytic,selections)}}
                supports=[float(case[2][selection,3].sum()) for case,selection in zip(cases,selections)]
                row['unweighted_eligible_support_m2']={'cpu':supports[0],'compute':supports[1]}
                row['mean_retention']={key:float(weight[selection].sum())/support for key,weight,selection,support in
                                      zip(('cpu','compute'),weights,selections,supports)}
                sets=[summary['backends'][key]['root_sets'][index] for key in ('cpu','compute')]
                row['exact_generated_root_position_overlap']={'cpu':len(sets[0]),'compute':len(sets[1]),'shared':len(sets[0]&sets[1])}
                summary['bands'].append(row)
            for key in ('cpu','compute'):del summary['backends'][key]['root_sets']
            report['coverage'].append(summary)
    # Stage percentiles are separate distributions, never additive p95 values.
    for prefix in ('stationary-repeat',):
        benchmark=j(DATA/prefix/'results.json')
        for run in benchmark['runs']:
            folder=DATA/prefix/f'pair-{run["pair"]}'/run['backend']
            frames=[json.loads(line) for line in read(folder/'frames.jsonl').splitlines()]
            numbers={f['profile_frame'] for f in frames if f['controls'].get('phase')=='measured'}
            rows=[r for r in csv.DictReader(io.StringIO(read(folder/'performance.csv').decode())) if int(r['frame']) in numbers]
            fields=[k for k in rows[0] if k.endswith('_ms') and k!='simulation_s']
            stats={key:percentile([float(r[key]) for r in rows if r[key]]) for key in fields if any(r[key] for r in rows)}
            work=[r for r in csv.DictReader(io.StringIO(read(folder/'performance.csv.gpu-work.csv').decode()))]
            measured_work=[r for r in work if int(r['frame']) in numbers]
            work_stats={}
            for stage,view in sorted({(r['stage'],r['view']) for r in measured_work}):
                selected=[r for r in measured_work if (r['stage'],r['view'])==(stage,view)]
                work_stats[stage+':'+view]={'rows':len(selected),
                    'gpu_ms':percentile([float(r['gpu_ms']) for r in selected]),
                    'cpu_submit_ms':percentile([float(r['cpu_submit_ms']) for r in selected])}
            report['stationary'].append({'pair':run['pair'],'backend':run['backend'],
                'native_stats':run['stats'],'stages':stats,'gpu_work_columns':list(work[0]),
                'measured_gpu_work':work_stats,
                'work_rows':len(work),'publication_outcomes':run['publication_outcomes']})
    return report


if __name__=='__main__':
    result=diagnose();(DATA/'diagnostics.json').write_text(json.dumps(result,indent=2)+'\n')
    print('Recorded live-area/seed overlap and stationary stage diagnostics without changing acceptance tolerances')
