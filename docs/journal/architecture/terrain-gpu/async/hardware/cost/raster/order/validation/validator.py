#!/usr/bin/env python3
"""Decode and independently recheck all qualified/excluded order controls."""
import csv
import gzip
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import struct
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent;DATA=HERE/'validation'


class ArtifactPath(type(Path())):
    def read_bytes(self):
        if self.exists():return Path.read_bytes(self)
        encoded=self.with_suffix(self.suffix+'.gz')
        if not encoded.exists() and self.name.startswith(('detailed.','quads.')):
            encoded=(self.parent/self.stem/self.suffix[1:]).with_suffix('.gz')
        return gzip.decompress(Path.read_bytes(encoded))
    def read_text(self,*args,**kwargs):return self.read_bytes().decode('utf-8')


def obj(path):return json.loads(ArtifactPath(path).read_text())
def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec)
    sys.modules[name]=m;spec.loader.exec_module(m);return m


def normalized(value):
    # Original absolute capture paths are retained in raw reports. Compare
    # the measurements using the stable request/frame suffix after relocation.
    for row in value['color_differences']:
        row['path']='/'.join(row['path'].split('/')[-2:])
    return value


def main():
    evidence=obj(HERE/'evidence.json')
    for name,sha in evidence['artifact_sha256'].items():assert hashlib.sha256((HERE/name).read_bytes()).hexdigest()==sha,name
    codec=obj(DATA/'codec-receipts.json');raw_bytes=stored_bytes=0
    for name,entry in codec['entries'].items():
        encoded=(DATA/name).read_bytes();raw=gzip.decompress(encoded) if name.endswith('.gz') else encoded
        assert len(raw)==entry['decoded_bytes'] and hashlib.sha256(raw).hexdigest()==entry['decoded_sha256'],name
        raw_bytes+=len(raw);stored_bytes+=len(encoded)
    assert len(codec['entries'])==codec['entry_count'] and raw_bytes==codec['decoded_bytes'] and stored_bytes==codec['stored_bytes']
    before,after=[obj(DATA/name) for name in ('before-provenance.json','provenance.json')]
    assert before['source_sha256']==after['source_sha256'] and before['driver_fixtures']==after['driver_fixtures']
    assert len(before['binaries'])==45 and len(after['binaries'])==46
    assert all(after['binaries'][p]==h for p,h in before['binaries'].items())
    manifest=obj(DATA/'manifest.json');assert not manifest['hardware_complete'] and not manifest['timing_acceptance'] and not manifest['migration_acceptance']
    assert manifest['probe_sha256']==after['binaries']['build-resume/tests/terrain_order_probe']
    analyze=module('order_analysis',DATA/'inputs/native/raster/order/analyze.py')
    native=module('NativeSession',DATA/'inputs/native/NativeSession.py')
    checked=0;findings={}
    for kind,base in [('final',DATA/'final/software'),('coarse',DATA/'preflight/coarse'),('refined',DATA/'preflight/refined'),('metadata',DATA/'preflight/metadata')]:
        report=obj(base/'results.json');assert not report['production'] and report['expected_uuid'] is None
        findings[kind]={}
        for backend in ('cpu','compute'):
            folder=ArtifactPath(base/backend);folders=[folder/'captures'/str(token)/str(i) for token in (1,2,3) for i in range(3)]
            actual=analyze.compare(folders)
            frames=[json.loads(l) for l in (folder/'frames.jsonl').read_text().splitlines()]
            actual['audit']=native.validate(frames,managed=backend=='compute')
            assert all('llvmpipe' in f['renderer'] for f in frames)
            if kind=='final':
                assert all(not f['benchmark']['grass_raster']['full_draw_timing_eligible'] and
                    not f['benchmark']['blade_order']['timing_acceptance'] and
                    not f['benchmark']['blade_order']['migration_acceptance'] for f in frames)
            selected=[]
            for capture in folders:
                item=obj(capture/'snapshot.json');joined=[f for f in frames if f['profile_frame']==item['profile_frame']]
                assert len(joined)==1;frame=joined[0];selected.append(frame)
                assert item['workload']['bodies']==frame['benchmark']['bodies']
                if kind=='final':assert frame['benchmark']['blade_order']['inspection']==item['inspection']
                assert not frame['pose']['airborne'] and not any(frame['keys'].values())
                c=frame['benchmark']['grass_raster']['control'];assert c['wind_mode']=='fixed' and c['raster_mode']=='full' and not c['qualification']
                for event in frame['benchmark']['grass_raster']['events']:assert event['effective_time_s']==12
                assert item['workload']['viewport']==item['workload']['scene_size']==[320,180]
            # Grounded double contacts can oscillate by one ULP. Compare exact
            # draw inputs and retain the double drift rather than inventing an
            # exact-double stationary movement acceptance claim.
            root=selected[0]['pose']['root']
            assert all(struct.pack('<3f',*f['pose']['root'])==struct.pack('<3f',*root) and f['camera_local']==selected[0]['camera_local'] and
                f['publication_geometry']==selected[0]['publication_geometry'] for f in selected)
            first=obj(folders[0]/'snapshot.json')['events']
            for capture in folders:
                for event,original in zip(obj(capture/'snapshot.json')['events'],first):
                    assert all(event[k]==original[k] for k in ('view','queue','model','view_model','projection','wind_s','command'))
            if kind!='coarse':assert all(v['instances']>0 for v in actual['controls']['native'].values())
            expected=report['backends'][backend]
            assert normalized(actual)==normalized(expected)
            actual['native_root_max_abs_drift_m']=max(abs(a-b) for f in selected for a,b in zip(f['pose']['root'],root))
            findings[kind][backend]=actual;checked+=actual['frames']
            print('Rechecked',kind,backend,actual['frames'],'exact permutation/depth snapshots',flush=True)
    assert checked==72 and manifest['final_snapshot_frames']==18 and manifest['preflight_snapshot_frames']==54
    access=obj(DATA/'device-access.json');assert not access['hardware_timing_acceptance'] and 'EPERM' in access['device_open']
    assert 'Failed to create GLFW window' in (DATA/'logs/preflight/order-quadro-fixture.log').read_text()
    result={'final_frames':18,'preflight_frames':54,'all_frames':checked,'payloads':len(codec['entries']),
        'raw_bytes':raw_bytes,'stored_bytes':stored_bytes,'findings':findings,'hardware_complete':False,'timing_acceptance':False,'migration_acceptance':False}
    (HERE/'validation-result.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k!='findings'},indent=2))


if __name__=='__main__':main()
