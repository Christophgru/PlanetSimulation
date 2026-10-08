#!/usr/bin/env python3
"""Bounded production Moon-handoff/reload pairs using the existing native audit.

These receipts expose quality differences; timings alone never enable compute.
Startup/handoff latency and replacement frames are retained separately from
warm steady frames. Raw output must stay in an ignored build/external directory.
"""
import argparse
import copy
import json
import math
from pathlib import Path
import subprocess
import time
from run import ROOT, inputs, sha, local_output
from NativeSession import Session, validate, write_json
from FlightSeeds import seeds
from receipts import summarize, compare_pair, rows, percentiles


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--output-dir', type=local_output, required=True)
    parser.add_argument('--expected-uuid', required=True)
    parser.add_argument('--pairs', type=int, default=3)
    parser.add_argument('--frames', type=int, default=240)
    parser.add_argument('--seconds', type=float, default=10)
    args = parser.parse_args()
    if args.pairs < 3 or args.frames < 240 or args.seconds < 10:
        parser.error('Require three pairs, 240 frames and ten seconds per case')
    out = args.output_dir.resolve()
    if out.exists():
        parser.error('Choose a fresh output directory')
    out.mkdir(parents=True)
    probe, binary = args.probe.resolve(), args.binary.resolve()
    scene = json.loads((ROOT/'configs/scenarios/solar_system.json').read_text())
    paths, launch = seeds(binary, ROOT, out, scene, boosted_launch=True)
    moon = next(b for b in launch['astronaut_pose']['navigation']['gravity_bodies'] if b['index']==2)
    frozen = {**inputs(), 'probe_sha256': sha(probe), 'application_sha256': sha(binary)}
    report = {'schema': 1, 'base_revision': subprocess.check_output(
        ['git','-c',f'safe.directory={ROOT}','rev-parse','HEAD'],text=True).strip(),
        'expected_uuid': args.expected_uuid, 'frozen_provenance': frozen,
        'method': {'pairs': args.pairs, 'frames': args.frames, 'minimum_seconds': args.seconds,
                   'warmup': 'three seconds and 30 ready frames', 'viewport': [1280,720],
                   'moon': 'common public production saved outer-space pose inside Moon orientation boundary; initial handoff retained as startup latency; post-handoff window keeps natural flight/body changes',
                   'reload': 'production seed increment; measured interval includes request, preparation, publication and retirement',
                   'outliers': 'none removed; startup/warmup/close are labelled in raw trace',
                   'quality_gate': 'must be established independently; scalar/timing receipts cannot establish rendered coverage'},
        'moon_input_sha256': sha(paths['moon'][0]), 'runs': [], 'pairs': []}
    write_json(out/'results.json',report)
    for case in ('moon','reload'):
        for pair in range(1,args.pairs+1):
            matched = {}
            for backend in (('cpu','compute') if pair%2 else ('compute','cpu')):
                folder = out/case/f'pair-{pair}'/backend
                folder.mkdir(parents=True)
                replay = paths['moon'][0] if case=='moon' else folder/'input.json'
                if case=='reload':
                    write_json(replay,{'scenario':scene,'surface_camera':scene['surface_camera']})
                trace = folder/'performance.csv'
                flags = ['--replay',str(replay),'--terrain-backend',backend,
                         '--terrain-grass-planner','gpu' if backend=='compute' else 'cpu',
                         '--performance-trace',str(trace)]
                began = time.monotonic_ns()
                session = Session(probe,ROOT,folder,flags,environment={'PLANET_NATIVE_BENCHMARK':'1'},
                                  control={'phase':'startup'})
                try:
                    session.focus()
                    if case=='reload': session.key('4')
                    def ready(f):
                        if f['mode']!=3 or not f.get('pose') or f['publication'].get('loading',False): return False
                        return True
                    initial = next((f for f in session.frames if ready(f)),None)
                    if initial is None: initial = session.wait(ready,timeout=180)
                    first = initial
                    if case=='moon':
                        # Check replay before advancing physics. A production CPU
                        # first frame can take seconds: gravity legitimately moves
                        # the astronaut before the handoff frame is presented.
                        assert math.dist(initial['pose']['navigation']['position_m'],paths['moon'][1])<.001
                        assert initial['pose']['navigation']['outer_space']
                        handed_off = lambda f: ready(f) and f['pose']['navigation']['reference_body']==2
                        first = next((f for f in session.frames if handed_off(f)),None)
                        if first is None: first = session.wait(handed_off,timeout=180)
                        nav = first['pose']['navigation']
                        assert first['selected']==1 and not nav['outer_space']
                        assert math.dist(nav['position_m'],moon['position_m'])<2.4*moon['radius_m']
                        assert len(nav['gravity_indices'])==3 and 2 in nav['gravity_indices']
                    warm = session.control({'phase':'warmup'})
                    session.wait(lambda f: ready(f) and f['frame']>=warm['frame']+30 and
                                 f['observed_ns']>=warm['observed_ns']+3_000_000_000,timeout=180)
                    start = session.control({'phase':'measured'})
                    requested = None
                    if case=='reload':
                        replacement = copy.deepcopy(scene)
                        replacement['scenario_name']='F5 production replacement'
                        replacement['planets'][0]['surface_noise'][0]['seed']+=1
                        write_json(replay,{'scenario':replacement,'surface_camera':replacement['surface_camera']})
                        requested = time.monotonic_ns()
                        session.key('r')
                    def finished(f):
                        return ready(f) and f['frame']>=start['frame']+args.frames-1 and \
                            f['observed_ns']>=start['observed_ns']+args.seconds*1e9 and \
                            (case!='reload' or (f['reload']['published']>=1 and not f['reload']['pending'] and not f['reload']['retiring']))
                    end = session.wait(finished,timeout=180)
                    session.control({'phase':'close'});session.close()
                    audit = validate(session.frames,managed=backend=='compute')
                    summary = summarize(trace,session.frames,args.expected_uuid)
                    native = {int(r['frame']):r for r in rows(str(trace)+'.native-loop.csv')}
                    summary.update({'case':case,'pair':pair,'backend':backend,'audit':audit,
                        'command':[str(probe),*flags], 'frozen_provenance':frozen,
                        'startup_to_ready_ms':(first['observed_ns']-began)/1e6,
                        'all_observed_frame_wall_ms':percentiles([float(native[f['profile_frame']]['wall_ms']) for f in session.frames]),
                        'final_reload':end['reload'],
                        'replay_navigation':initial['pose'].get('navigation'),
                        'initial_navigation':first['pose'].get('navigation'),
                        'final_navigation':end['pose'].get('navigation'),
                        'input_sha256':sha(replay)})
                    if requested is not None:
                        published = next(f for f in session.frames if f['reload']['published']>=1)
                        retired = next(f for f in session.frames if f['reload']['published']>=1 and not f['reload']['retiring'])
                        summary['request_to_publication_ms']=(published['observed_ns']-requested)/1e6
                        summary['request_to_retirement_ms']=(retired['observed_ns']-requested)/1e6
                        summary['replacement_frames']=[f['profile_frame'] for f in session.frames
                            if start['frame']<=f['frame']<=retired['frame']]
                    assert {**inputs(),'probe_sha256':sha(probe),'application_sha256':sha(binary)}==frozen
                    write_json(folder/'summary.json',summary)
                    report['runs'].append(summary);matched[backend]=summary
                    write_json(out/'results.json',report)
                    print(f'{case} pair {pair} {backend}: {summary["measured_frames"]} frames, native p95 {summary["stats"]["wall_ms"]["p95"]:.3f} ms',flush=True)
                finally:
                    session.abort()
            report['pairs'].append({'case':case,'pair':pair,**compare_pair(matched['cpu'],matched['compute'])})
            write_json(out/'results.json',report)


if __name__=='__main__':
    main()
