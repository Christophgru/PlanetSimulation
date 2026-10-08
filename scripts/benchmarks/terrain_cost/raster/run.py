#!/usr/bin/env python3
"""Three alternating stationary pairs with frozen wind and private raster controls."""

import sys
from pathlib import Path
_repo = next(p for p in Path(__file__).resolve().parents if (p/'scripts/benchmarks/archives').is_dir())
sys.path.insert(0, str(_repo/'scripts/benchmarks'))
from archives.local import local_output
import argparse
import inspect # Preload stdlib before the historical inspect.py CLI directory.
import json
from pathlib import Path
import subprocess
import sys
import time
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[4]
sys.path.insert(0,str(ROOT/'tests/app/terrain/native'))
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from NativeSession import Session, validate, write_json
from receipts import compare_pair
from records import analysis, measured
import importlib.util
spec=importlib.util.spec_from_file_location('frozen_stationary_driver',Path(__file__).resolve().parents[1]/'run.py')
stationary=importlib.util.module_from_spec(spec);spec.loader.exec_module(stationary)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--probe',type=Path,required=True)
    p.add_argument('--output-dir',type=local_output,required=True)
    p.add_argument('--expected-uuid',required=True)
    p.add_argument('--wind-mode',choices=('native','fixed','indexed'),required=True)
    p.add_argument('--raster-mode',choices=('full','discard','suppress'),required=True)
    p.add_argument('--frames',type=int,default=240)
    args=p.parse_args()
    assert args.frames>=200
    out=args.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
    probe=args.probe.resolve();frozen={**stationary.inputs(),'probe_sha256':stationary.sha(probe)}
    source=ROOT/'configs/scenarios/solar_system.json';scene=json.loads(source.read_text())
    replay=out/'production-input.json';write_json(replay,{'scenario':scene,'surface_camera':scene['surface_camera']})
    (out/'devices.csv').write_text(subprocess.check_output(['nvidia-smi','--query-gpu=name,uuid,driver_version,memory.total','--format=csv'],text=True))
    report={'schema':1,'scope':'Stationary wind/raster diagnostics; no migration acceptance',
        'base_revision':subprocess.check_output(['git','-c',f'safe.directory={ROOT}','rev-parse','HEAD'],text=True).strip(),
        'utc':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),'frozen_provenance':frozen,
        'expected_uuid':args.expected_uuid,'production_config_sha256':stationary.sha(source),'replay_sha256':stationary.sha(replay),
        'wind_mode':args.wind_mode,'raster_mode':args.raster_mode,'method':{'pairs':3,'frames':args.frames,
            'warmup':'at least 30 grounded frames and 3 wall seconds',
            'cohort':f'first {args.frames} synchronized measured wind indices, all other raw frames retained; no outlier filtering',
            'wind_start_s':12,'wind_step_s':.125,'native_wall_p95_limit':1.05,
            'observer':'all hooks, receipts and primary observer overhead included',
            'qualification_queries_in_measured_frames':0,'mode_exclusions':'discard/suppress are diagnostic ablations, never full-frame acceptance'},
        'runs':[],'pairs':[],'timing_acceptance':False,'migration_acceptance':False}
    write_json(out/'results.json',report)
    control={'wind_mode':args.wind_mode,'raster_mode':args.raster_mode,'wind_start_s':12,'wind_step_s':.125,'qualify_raster':False}
    for pair in range(1,4):
        paired={}
        for backend in (('cpu','compute') if pair%2 else ('compute','cpu')):
            folder=out/f'pair-{pair}'/backend;trace=folder/'performance.csv'
            flags=['--replay',str(replay),'--terrain-backend',backend,'--terrain-grass-planner','gpu' if backend=='compute' else 'cpu','--performance-trace',str(trace)]
            session=Session(probe,ROOT,folder,flags,environment={'PLANET_NATIVE_BENCHMARK':'1'},control={**control,'phase':'startup'})
            try:
                session.focus();session.key('4')
                def ready(f):return f['mode']==3 and f.get('pose') and not f['pose']['airborne'] and not f['publication'].get('loading',False)
                session.wait(ready,timeout=180)
                warm=session.control({**control,'phase':'warmup'})
                session.wait(lambda f:ready(f) and f['frame']>=warm['frame']+30 and f['observed_ns']>=warm['observed_ns']+3_000_000_000,timeout=180)
                session.control({**control,'phase':'measured'})
                session.wait(lambda f:ready(f) and sum(measured(v,args.frames) for v in session.frames)>=args.frames,timeout=180)
                session.control({**control,'phase':'close'});session.close()
                audit=validate(session.frames,managed=backend=='compute')
                result=analysis(trace,session.frames,args.expected_uuid,args.wind_mode,args.raster_mode,args.frames)
                result.update({'pair':pair,'backend':backend,'audit':audit,'path':str(folder.relative_to(out)),'command':session.process.args})
                assert stationary.inputs()=={k:frozen[k] for k in ('source_sha256','build_test_sha256')}
                assert stationary.sha(probe)==frozen['probe_sha256'] and stationary.sha(source)==report['production_config_sha256']
                write_json(folder/'summary.json',result);report['runs'].append(result);paired[backend]=result
                write_json(out/'results.json',report)
                print(args.wind_mode,args.raster_mode,'pair',pair,backend,'native p95',round(result['stats']['wall_ms']['p95'],3),flush=True)
            finally:session.abort()
        comparison=compare_pair(paired['cpu'],paired['compute'])
        assert comparison['scalar_workload_matched'] and paired['cpu']['geometry']==paired['compute']['geometry']
        assert paired['cpu']['camera_local']==paired['compute']['camera_local']
        report['pairs'].append({'pair':pair,**comparison,'wind_phase_matched':args.wind_mode!='native',
            'full_draw':args.raster_mode=='full','within_native_p95_limit':comparison['wall_p95_ratio']<=1.05})
        write_json(out/'results.json',report)
    print('Retained three alternating pairs; earlier failures and all remaining migration gates stay explicit',flush=True)


if __name__=='__main__':main()
