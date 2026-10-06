from pathlib import Path
import csv,hashlib,json,os,subprocess
root=Path('/workspace');out=root/'build-resume/memory-hardware/captures';out.mkdir(parents=True,exist_ok=True)
scene=json.loads((root/'tests/scenarios/foliage/surface.json').read_text())
for p in scene['planets']:p.setdefault('terrain_lod',{})['max_triangle_budget']=10000
scene['planets'][0]['foliage']['max_blades']=128
scene['lighting']['shadows']['resolution']=256
config=out/'scene.json';config.write_text(json.dumps(scene,indent=2))
base={**os.environ,'__EGL_VENDOR_LIBRARY_FILENAMES':str(root/'build-resume/gpu-comparison/nvidia-egl.json'),'LD_PRELOAD':str(root/'build-resume/gpu-comparison/libegl_capture.so')}
for k in ['LIBGL_ALWAYS_SOFTWARE','__GLX_VENDOR_LIBRARY_NAME','__NV_PRIME_RENDER_OFFLOAD']:base.pop(k,None)
expected=['GPU-2cefee61-6b3b-a670-c390-c7449dad3f79','GPU-1795f01b-f1fd-61a0-3b79-199560dd0793'];results=[]
for device in [0,1]:
    for backend in ['cpu','compute']:
        folder=out/f'device-{device}-{backend}';folder.mkdir(exist_ok=True);pair=[]
        for traced in [False,True]:
            image=folder/('traced.png' if traced else 'untraced.png');trace=folder/'frames.csv'
            cmd=[str(root/'build-resume/PlanetSimulation'),'--config',str(config),'--surface-capture',str(image),'--render-size','320','180','--simulation-time','20','--terrain-backend',backend,'--terrain-grass-planner','gpu' if backend=='compute' else 'cpu','--benchmark-frames','32','--benchmark-step',str(1/60),'--benchmark-walk-step','0.1']
            if traced:cmd+=['--performance-trace',str(trace)]
            with (folder/('traced.log' if traced else 'untraced.log')).open('w') as log:
                run=subprocess.run(cmd,cwd=root,env={**base,'PLANET_EGL_DEVICE':str(device)},stdout=log,stderr=subprocess.STDOUT,timeout=120)
            assert run.returncode==0,(device,backend,traced)
            text=(folder/('traced.log' if traced else 'untraced.log')).read_text();assert f'EGL device UUID: {expected[device]}' in text
            pair.append(hashlib.sha256(image.read_bytes()).hexdigest())
        assert pair[0]==pair[1],(device,backend,pair)
        rows=list(csv.DictReader(Path(str(trace)+'.memory.csv').open()));assert rows[0]['phase']=='renderer_ready' and rows[-1]['phase']=='shutdown'
        for r in rows:
            assert r['context_uuid']==expected[device] and r['context_status']=='uuid_verified' and r['nvml_status']=='ok' and r['nvx_status']=='ok',r
            assert int(r['used_bytes'])+int(r['free_bytes'])<=int(r['total_bytes'])
            assert int(r['dropped_events'])==0
            assert int(r['dropped_observations'])<=int(r['skipped_latest_observations'])
            assert r['logical_status']==('resident_reservation' if backend=='compute' else 'unavailable')
        result={'device':device,'backend':backend,'uuid':expected[device],'renderer':rows[0]['renderer'],'rows':len(rows),'samples':sum(r['kind']=='sample' for r in rows),'png_sha256':pair[0],'traced_untraced_png_exact':True,'sampled_device_used_peak_bytes':max(int(r['used_bytes']) for r in rows if r['kind']=='sample'),'logical_overlap_peak_bytes':max(int(r['overlap_reserved_bytes']) for r in rows) if backend=='compute' else None}
        results.append(result);print(json.dumps(result),flush=True)
        (out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
