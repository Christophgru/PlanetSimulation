"""Raw wind/draw receipt validation and declared stationary cohorts."""
import copy
import math
from receipts import rows, percentiles, summarize


def geometry(frame):
    return [{k:g[k] for k in ('body','land_field','land_topology','water_field','water_topology')}
            for g in frame['publication_geometry']]


def measured(frame, count):
    r=frame.get('benchmark',{}).get('grass_raster',{})
    c=r.get('control',{})
    return frame['controls'].get('phase')=='measured' and c.get('phase')=='measured' and c.get('sequence',count)<count and bool(r.get('events'))


def validate_wind(frame, wind, raster, qualification=False):
    r=frame['benchmark']['grass_raster'];c=r['control']
    assert c['wind_mode']==wind and c['raster_mode']==raster and c['qualification']==qualification
    assert not r['timing_acceptance']
    assert r['full_draw_timing_eligible']==(not qualification and raster=='full')
    assert sum(r['other_draw_calls'].values())>0
    events=[e for e in r['events'] if e['body']==0]
    assert {e['view'] for e in events}=={'main','reflection'} and len(events)==2
    for e in events:
        assert e['original_time_s']==e['original_blade_time_s'] and math.isfinite(e['original_time_s'])
        expected=e['original_time_s'] if wind=='native' else float(c['wind_start_s']+(c['sequence']*c['wind_step_s'] if wind=='indexed' else 0))
        assert e['effective_time_s']==expected
        assert e['raster_mode']==raster and e['draws']
        placement=frame['benchmark']['bodies'][e['body']]['foliage']['compute_placement']
        assert e['placement_time_writes']==int(placement)
        if placement:assert e['placement_time_s']==expected
        for d in e['draws']:
            assert d['submitted']==(raster!='suppress')
            assert d['mode']==5 # GL_TRIANGLE_STRIP
            if placement:assert d['kind']=='indirect' and d['offset_bytes'] in (0,16)
            else:assert d['kind']=='instanced' and d['instances']>0 and d['vertices']>=4
        assert ('blocking_qualification_query_reads' in e)==qualification
    assert events[0]['effective_time_s']==events[1]['effective_time_s']


def analysis(trace, frames, uuid, wind, raster, count):
    selected=[f for f in frames if measured(f,count)]
    assert len(selected)==count and [f['benchmark']['grass_raster']['control']['sequence'] for f in selected]==list(range(count))
    assert [f['profile_frame'] for f in selected]==list(range(selected[0]['profile_frame'],selected[0]['profile_frame']+count))
    first=selected[0]
    normalized=copy.deepcopy(frames)
    for f in normalized:
        eligible=measured(f,count)
        f['controls']['phase']='measured' if eligible else 'excluded'
        f['benchmark'].pop('grass_raster',None)
    result=summarize(trace,normalized,uuid)
    assert result['observed_duration_s']>=5
    assert result['gpu_status_counts']=={'ready':count}
    for f in selected:
        validate_wind(f,wind,raster)
        assert f['benchmark']['bodies']==first['benchmark']['bodies'] and geometry(f)==geometry(first)
        assert f['camera_local']==first['camera_local'] and f['pose']['root']==first['pose']['root']
        assert not any(f['keys'].values()) and not f['pose']['airborne']
    perf={int(r['frame']):r for r in rows(trace)}
    assert all(int(perf[f['profile_frame']]['gpu_events_dropped'])==0 for f in selected)
    for name in ('opaque','reflections','meshes','atmosphere'):
        for prefix in ('cpu','gpu'):
            key=prefix+'_'+name+'_ms'
            if key in perf[first['profile_frame']]:result['stats'][key]=percentiles([float(perf[f['profile_frame']][key]) for f in selected])
    work=rows(str(trace)+'.gpu-work.csv')
    assert all(r['status']=='ready' for r in work)
    for view in ('main','reflection'):
        spans=[r for r in work if r['stage']=='grass_placement' and r['view']==view and
               selected[0]['profile_frame']<=int(r['frame'])<=selected[-1]['profile_frame']]
        assert len(spans)==count
        for key in ('gpu_ms','cpu_submit_ms'):result['stats']['placement_'+view+'_'+key]=percentiles([float(r[key]) for r in spans])
    result['geometry']=geometry(first)
    result['wind_mode']=wind;result['raster_mode']=raster
    result['wind_original_range_s']=[min(e['original_time_s'] for f in selected for e in f['benchmark']['grass_raster']['events']),
        max(e['original_time_s'] for f in selected for e in f['benchmark']['grass_raster']['events'])]
    result['wind_effective_range_s']=[min(e['effective_time_s'] for f in selected for e in f['benchmark']['grass_raster']['events']),
        max(e['effective_time_s'] for f in selected for e in f['benchmark']['grass_raster']['events'])]
    result['wind_phase_matched_candidate']=wind!='native'
    result['timing_acceptance']=False;result['migration_acceptance']=False
    return result
