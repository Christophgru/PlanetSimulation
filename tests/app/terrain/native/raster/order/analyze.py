"""Independent raw Blade permutation and pipeline receipt checks."""
from collections import Counter
import hashlib
import json
from pathlib import Path
import statistics
import struct


def records(path):
    raw=path.read_bytes();assert len(raw)%64==0
    return [raw[i:i+64] for i in range(0,len(raw),64)]


def check(folder):
    item=json.loads((folder/'snapshot.json').read_text())
    assert item['inspection']['blocking'] and not item['inspection']['timing_acceptance']
    assert item['inspection']['query_reads']==12 and item['inspection']['pixel_reads']==5
    assert item['workload']==item['workload_after']
    events=item['events'];assert len(events)==4
    assert {(e['view'],e['queue']) for e in events}=={('main',0),('main',1),('reflection',0),('reflection',1)}
    contents={};counts={};timings={};samples={}
    for event in events:
        view,queue=event['view'],event['queue'];name='detailed' if queue==0 else 'quads';base=folder/view
        source=records(base/(name+'.input'));target=records(base/(name+'.permuted'));n=len(source)
        assert len(target)==n and event['command']==[14 if queue==0 else 4,n,0,0]
        assert event['generated_primitives']==n*(12 if queue==0 else 2)
        assert event['wind_s']==12 and event['source_buffer_bytes']%128==0
        assert n<=event['source_buffer_bytes']//128
        keys=struct.unpack('<'+'f'*n,(base/(name+'.depth-keys')).read_bytes())
        indices=struct.unpack('<'+'I'*n,(base/(name+'.indices')).read_bytes())
        expected=list(range(n))
        if event['mode']!='native':expected.sort(key=lambda i:keys[i],reverse=event['mode']=='far')
        assert tuple(expected)==indices
        assert target==[source[i] for i in indices]
        assert Counter(source)==Counter(target)
        # Digest independent of compaction arrival order, retaining every byte.
        contents[view,name]=hashlib.sha256(b''.join(sorted(source))).hexdigest()
        counts[view,name]=n;timings[view,name]=event['draw_gpu_ns']/1e6;samples[view,name]=event['passed_samples']
    assert sum(counts.values())>0
    for view in ('main','reflection'):
        w,h=item['depth_viewports'][view]
        assert all(len((folder/view/name).read_bytes())==w*h*4 for name in ('before.depth','after.depth'))
    assert item['inspection']['buffer_read_bytes']==64+sum(counts.values())*64
    assert item['inspection']['buffer_upload_bytes']==sum(counts.values())*128
    assert len((folder/'display.rgba').read_bytes())==item['viewport'][0]*item['viewport'][1]*4
    return item,contents,counts,timings,samples


def compare(folders):
    rows=[(p,*check(p)) for p in folders];first=rows[0]
    for folder,item,contents,counts,timings,samples in rows:
        assert item['workload']==first[1]['workload']
        assert contents==first[2] and counts==first[3]
    # Keep raw depth checks distinct from color differences. Quantized-depth
    # ties may select a different shaded blade; report rather than discard it.
    depth_equal=True;color=[]
    reference=(folders[0]/'display.rgba').read_bytes()
    for folder in folders:
        depth_equal &= all((folders[0]/view/name).read_bytes()==(folder/view/name).read_bytes()
            for view in ('main','reflection') for name in ('before.depth','after.depth'))
        actual=(folder/'display.rgba').read_bytes();assert len(actual)==len(reference)
        changed=sum(actual[i:i+3]!=reference[i:i+3] for i in range(0,len(actual),4))
        color.append({'path':str(folder),'changed_rgb_pixels':changed,'fraction':changed/(len(actual)//4)})
    assert depth_equal,'Permutation changed depth: retain raw discrepancy for diagnosis'
    summary={}
    for mode in ('native','near','far'):
        selected=[row for row in rows if row[1]['controls']['order_mode']==mode]
        assert len(selected)>=3
        summary[mode]={}
        for view in ('main','reflection'):
            for name in ('detailed','quads'):
                key=view+'/'+name
                summary[mode][key]={'instances':first[3][view,name],
                    'draw_gpu_ms':[row[4][view,name] for row in selected],
                    'draw_gpu_median_ms':statistics.median(row[4][view,name] for row in selected),
                    'passed_samples':[row[5][view,name] for row in selected]}
    return {'frames':len(rows),'exact_multisets':True,'depth_equal':depth_equal,'color_differences':color,'controls':summary,
            'timing_acceptance':False,'migration_acceptance':False}
