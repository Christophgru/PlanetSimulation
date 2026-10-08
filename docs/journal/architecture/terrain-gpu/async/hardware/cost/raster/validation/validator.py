#!/usr/bin/env python3
"""Reconstruct every stationary cohort, wind control and pipeline qualification."""
import csv
import gzip
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import statistics
import sys
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
DATA=HERE/'validation'


def read(path):
    path=Path(path)
    if not path.exists():path=path.with_suffix(path.suffix+'.gz')
    return gzip.decompress(path.read_bytes()).decode() if path.suffix=='.gz' else path.read_text()


def obj(path):return json.loads(read(path))
def rows(path):return list(csv.DictReader(io.StringIO(read(path))))
def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path);value=importlib.util.module_from_spec(spec)
    sys.modules[name]=value;spec.loader.exec_module(value);return value


def qualification(base,quadro,uuid,session):
    report=obj(base/'results.json');assert report['frames']==108 and len(report['cases'])==27
    checked=0
    for case in ('legacy','resident','vertex'):
        frames=[json.loads(l) for l in read(base/case/'frames.jsonl').splitlines()]
        session.validate(frames,managed=case=='resident')
        assert obj(base/case/'result.json')=={'native_audit':True,'qualification_only':True}
        for item in report['cases']:
            if item['case']!=case:continue
            wind,mode=item['wind_mode'],item['raster_mode'];phase=wind+'-'+mode
            selected=[f for f in frames if f['benchmark']['grass_raster']['control']['phase']==phase and
                f['benchmark']['grass_raster']['control']['sequence']<4 and f['benchmark']['grass_raster']['events']]
            assert len(selected)==4
            values=[]
            for f in selected:
                r=f['benchmark']['grass_raster'];c=r['control'];assert c['qualification'] and not r['timing_acceptance']
                assert sum(r['other_draw_calls'].values())>0
                events=[e for e in r['events'] if e['body']==0]
                assert len(events)==2 and {e['view'] for e in events}=={'main','reflection'}
                for e in events:
                    expected=e['original_time_s'] if wind=='native' else 12+(c['sequence']*.125 if wind=='indexed' else 0)
                    assert e['effective_time_s']==e['verified_blade_time_s']==expected
                    assert e['original_time_s']==e['original_blade_time_s']
                    assert e['blocking_qualification_query_reads']==2
                    assert all(d['submitted']==(mode!='suppress') for d in e['draws']) and e['draws']
                    assert e['placement_time_writes']==int(case!='vertex')
                    if case!='vertex':assert e['placement_time_s']==e['verified_placement_time_s']==expected
                    if mode=='suppress':assert e['generated_primitives']==e['passed_samples']==0
                    else:assert e['generated_primitives']>0
                    if mode=='discard':assert e['passed_samples']==0
                if mode=='full':assert next(e for e in events if e['view']=='main')['passed_samples']>0
                values.append([(e['view'],e['generated_primitives'],e['passed_samples'],len(e['draws'])) for e in events])
                checked+=1
            assert values==[list(map(tuple,r)) for r in item['pipeline_results']]
        if quadro:
            memory=rows(base/case/'trace/performance.csv.memory.csv')
            assert memory and all(r['context_uuid']==uuid and r['context_status']=='uuid_verified' and r['nvml_status']=='ok' for r in memory)
        else:assert all('llvmpipe' in f['renderer'] for f in frames)
    assert checked==108
    # A separate comparison confirms fragment discard retains the very same
    # primitive workload, not merely a positive primitive count.
    for case in ('legacy','resident','vertex'):
        for wind in ('fixed','indexed'):
            modes={r['raster_mode']:r['pipeline_results'] for r in report['cases'] if r['case']==case and r['wind_mode']==wind}
            assert [[(v,p,c) for v,p,_,c in row] for row in modes['full']]==[[(v,p,c) for v,p,_,c in row] for row in modes['discard']]
    return checked


def main():
    evidence=obj(HERE/'evidence.json')
    for name,h in evidence['artifact_sha256'].items():assert hashlib.sha256((HERE/name).read_bytes()).hexdigest()==h,name
    codec=obj(DATA/'codec-receipts.json')
    assert codec['entry_count']==len(codec['entries'])
    decoded_bytes=stored_bytes=0
    for name,receipt in codec['entries'].items():
        payload=(DATA/name).read_bytes();stored_bytes+=len(payload)
        raw=gzip.decompress(payload) if name.endswith('.gz') else payload
        assert len(raw)==receipt['decoded_bytes'] and hashlib.sha256(raw).hexdigest()==receipt['decoded_sha256'],name
        decoded_bytes+=len(raw)
    assert decoded_bytes==codec['decoded_bytes'] and stored_bytes==codec['stored_bytes']
    before,after=[obj(DATA/name) for name in ('before-provenance.json','provenance.json')]
    assert before['source_sha256']==after['source_sha256'] and before['driver_fixtures']==after['driver_fixtures']
    assert len(before['binaries'])==44 and len(after['binaries'])==45
    assert all(after['binaries'][p]==h for p,h in before['binaries'].items())
    manifest=obj(DATA/'manifest.json');uuid=manifest['uuid']
    assert manifest['frames_per_run']==240 and manifest['pairs_per_case']==3 and manifest['native_p95_limit']==1.05
    assert not manifest['timing_acceptance'] and not manifest['migration_acceptance']
    session=module('NativeSession',DATA/'inputs/native/NativeSession.py')
    receipts=module('receipts',DATA/'inputs/standard/receipts.py');receipts.rows=rows
    records=module('raster_records',DATA/'inputs/driver/records.py');records.rows=rows
    total=excluded=outliers=0;findings=[]
    for wind in manifest['wind_modes']:
        for raster in manifest['raster_modes']:
            base=DATA/'final'/wind/raster;report=obj(base/'results.json')
            assert len(report['runs'])==6 and len(report['pairs'])==3
            assert report['wind_mode']==wind and report['raster_mode']==raster
            assert report['frozen_provenance']['source_sha256']==after['source_sha256']
            assert report['frozen_provenance']['build_test_sha256']==after['build_test_input_sha256']
            assert report['frozen_provenance']['probe_sha256']==after['binaries']['build-resume/tests/terrain_raster_probe']
            assert [(r['pair'],r['backend']) for r in report['runs']]==[(1,'cpu'),(1,'compute'),(2,'compute'),(2,'cpu'),(3,'cpu'),(3,'compute')]
            paired={}
            for run in report['runs']:
                folder=base/run['path'];frames=[json.loads(l) for l in read(folder/'frames.jsonl').splitlines()]
                assert session.validate(frames,managed=run['backend']=='compute')==run['audit']
                actual=records.analysis(folder/'trace/performance.csv',frames,uuid,wind,raster,240)
                expected={k:v for k,v in run.items() if k not in ('pair','backend','audit','path','command')}
                assert actual==expected
                selected=[f for f in frames if records.measured(f,240)]
                assert all(f['benchmark']['viewport']==f['benchmark']['scene_size']==[1280,720] and f['benchmark']['quality_scale']==1 for f in selected)
                assert all(f['benchmark']['bodies'][0]['triangles']==100000 and f['benchmark']['bodies'][0]['foliage']['budget']==f['benchmark']['bodies'][0]['foliage']['configured_budget']==2000000 for f in selected)
                native={int(r['frame']):r for r in rows(folder/'trace/performance.csv.native-loop.csv')}
                wall=[float(native[f['profile_frame']]['wall_ms']) for f in selected]
                q1,_,q3=statistics.quantiles(wall,n=4,method='inclusive');fence=q3+1.5*(q3-q1)
                count=sum(x>fence for x in wall);outliers+=count
                perf={int(r['frame']):r for r in rows(folder/'trace/performance.csv')}
                stages={key:receipts.percentiles([float(perf[f['profile_frame']][key]) for f in selected]) for key in perf[selected[0]['profile_frame']] if (key.startswith('cpu_') or key.startswith('gpu_')) and key.endswith('_ms')}
                total+=len(selected);excluded+=len(frames)-len(selected)
                paired[run['pair'],run['backend']]={'summary':actual,'frames':selected,'stages':stages,'outliers':count}
            for pair in report['pairs']:
                number=pair['pair'];a,b=[paired[number,backend] for backend in ('cpu','compute')]
                comparison=receipts.compare_pair(a['summary'],b['summary'])
                assert all(pair[k]==value for k,value in comparison.items())
                assert comparison['scalar_workload_matched'] and a['summary']['geometry']==b['summary']['geometry']
                assert a['summary']['camera_local']==b['summary']['camera_local'] and a['summary']['pose_root']==b['summary']['pose_root']
                if wind!='native':
                    for x,y in zip(a['frames'],b['frames']):
                        xe,ye=[f['benchmark']['grass_raster']['events'] for f in (x,y)]
                        assert [(e['body'],e['view'],e['effective_time_s']) for e in xe]==[(e['body'],e['view'],e['effective_time_s']) for e in ye]
                findings.append({'wind':wind,'raster':raster,'pair':number,'native_p95_ratio':comparison['wall_p95_ratio'],
                    'within_1_05':comparison['wall_p95_ratio']<=1.05,'wind_matched':wind!='native',
                    'cpu':a['summary']['stats'],'compute':b['summary']['stats'],
                    'cpu_stages':a['stages'],'compute_stages':b['stages'],
                    'outliers_retained':a['outliers']+b['outliers']})
            print('Reconstructed',wind,raster,'3 pairs /1440 measured frames',flush=True)
    assert total==12960 and len(findings)==27
    qual=sum(qualification(DATA/'qualification'/kind,kind=='quadro',uuid,session) for kind in ('software','quadro'))
    preflight=sum(qualification(DATA/'preflight'/kind,False,uuid,session) for kind in ('labels','counts'))
    result={'runs':54,'pairs':27,'measured_frames':total,'excluded_frames':excluded,'upper_outliers_retained':outliers,
        'qualification_frames':qual,'preflight_frames':preflight,'findings':findings,'timing_acceptance':False,'migration_acceptance':False}
    (HERE/'validation-result.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k!='findings'},indent=2))


if __name__=='__main__':main()
