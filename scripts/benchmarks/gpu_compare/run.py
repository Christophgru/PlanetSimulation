#!/usr/bin/env python3
"""Compare the unchanged capture renderer on two physical EGL GPUs."""
import argparse,csv,hashlib,json,math,os,platform,re,statistics,subprocess,time
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
P=argparse.ArgumentParser(description=__doc__)
P.add_argument('--binary',type=Path,required=True)
P.add_argument('--output-dir',type=Path,required=True)
P.add_argument('--config',type=Path,default=ROOT/'configs/scenarios/solar_system.json')
P.add_argument('--devices',type=int,nargs=2,default=[0,1])
P.add_argument('--pairs',type=int,default=3)
P.add_argument('--frames',type=int,default=90)
P.add_argument('--warmup',type=int,default=10)
P.add_argument('--size',type=int,nargs=2,default=[1280,720])
args=P.parse_args()
if args.pairs<3 or args.warmup<0 or args.frames<args.warmup+20 or len(set(args.devices))!=2:
    P.error('Need distinct GPUs, at least three pairs and twenty measured frames')
out=args.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
binary=args.binary.resolve();config=args.config.resolve()
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def inputs():
    h=hashlib.sha256()
    files=[ROOT/'CMakeLists.txt']+[p for folder in ['src','shaders'] for p in (ROOT/folder).rglob('*') if p.is_file()]
    files += [config,Path(__file__),Path(__file__).with_name('EglCapture.cpp')]
    for p in sorted(set(files),key=str):h.update(str(p.relative_to(ROOT)).encode()+b'\0'+p.read_bytes()+b'\0')
    return {'input_sha256':h.hexdigest(),'binary_sha256':sha(binary),'config_sha256':sha(config)}
frozen=inputs()
(out/'scenario.json').write_bytes(config.read_bytes())
vendor=out/'nvidia-egl.json';vendor.write_text(json.dumps({'file_format_version':'1.0.0','ICD':{'library_path':'libEGL_nvidia.so.0'}})+'\n')
shim=out/'libegl_capture.so'
command=['c++','-std=c++17','-O2','-fPIC','-shared',str(Path(__file__).with_name('EglCapture.cpp')),'-o',str(shim),'-lEGL','-lGL','-lGLEW','-ldl']
result=subprocess.run(command,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
(out/'build.log').write_text(result.stdout);assert result.returncode==0,result.stdout
identity=subprocess.check_output(['nvidia-smi','--query-gpu=index,name,uuid,pci.bus_id,driver_version,memory.total','--format=csv'],text=True)
(out/'devices.csv').write_text(identity)
devices={r[' uuid'].strip():{k.strip():v.strip() for k,v in r.items()} for r in csv.DictReader(identity.splitlines())}
base={**os.environ,'__EGL_VENDOR_LIBRARY_FILENAMES':str(vendor),'LD_PRELOAD':str(shim)}
for k in ['LIBGL_ALWAYS_SOFTWARE','MESA_GL_VERSION_OVERRIDE','MESA_GLSL_VERSION_OVERRIDE','__GLX_VENDOR_LIBRARY_NAME','__NV_PRIME_RENDER_OFFLOAD']:base.pop(k,None)
report={'date':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),'base_revision':subprocess.check_output(['git','-c',f'safe.directory={ROOT}','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
        **frozen,'adapter_sha256':sha(shim),'platform':platform.platform(),'compiler':subprocess.check_output(['c++','--version'],text=True).splitlines()[0],
        'method':{'size':args.size,'frames':args.frames,'warmup':args.warmup,'excluded_final_frame':args.frames-1,'pairs':args.pairs,'step_s':1/60,'walking_m_per_frame':0.1,
                  'start_time_s':20,'terrain_backend':'cpu','grass_planner':'cpu','presentation':'uncapped EGL pbuffer; no desktop presentation','frame_time_scope':'capture CPU wall time; GPU scopes overlap CPU; final readback is outside the timer'},'runs':[]}
monitor_file=(out/'telemetry.csv').open('w')
monitor=subprocess.Popen(['nvidia-smi','--query-gpu=timestamp,index,name,uuid,temperature.gpu,clocks.sm,clocks.mem,power.draw,utilization.gpu,memory.used','--format=csv','-lms','1000'],stdout=monitor_file,stderr=subprocess.STDOUT)
def summarize(rows):
    measured=[r for r in rows if args.warmup<=int(r['frame'])<args.frames-1]
    stats={}
    for key in measured[0]:
        if key.endswith('_ms'):
            values=sorted(float(r[key]) for r in measured if r[key]!='')
            if values:stats[key]={'mean':statistics.mean(values),'median':statistics.median(values),'p95':values[math.ceil(.95*len(values))-1],'p99':values[math.ceil(.99*len(values))-1]}
    return {'measured_frames':len(measured),'gpu_valid_frames':sum(int(r['gpu_valid']) for r in measured),
            'stats':stats,'counts':{key:sum(int(r[key]) for r in measured) for key in ['scene_reuses','mesh_uploads','foliage_rebuilds','shadow_updates','shadow_reuses']}}
try:
    for case,walk in [('orbit',0),('walking',.1)]:
        for pair in range(args.pairs):
            order=args.devices if pair%2==0 else list(reversed(args.devices))
            for device in order:
                folder=out/case/f'pair-{pair+1}'/f'device-{device}';folder.mkdir(parents=True,exist_ok=True)
                image=folder/'capture.png';trace=folder/'frames.csv'
                cmd=[str(binary),'--config',str(out/'scenario.json'),'--surface-capture',str(image),'--simulation-time','20','--render-size',*map(str,args.size),
                     '--benchmark-frames',str(args.frames),'--benchmark-step',str(1/60),'--benchmark-walk-step',str(walk),'--performance-trace',str(trace)]
                env={**base,'PLANET_EGL_DEVICE':str(device)};started=time.time()
                with (folder/'run.log').open('w') as stream:result=subprocess.run(cmd,cwd=ROOT,env=env,stdout=stream,stderr=subprocess.STDOUT)
                ended=time.time();text=(folder/'run.log').read_text();assert result.returncode==0,text[-4000:]
                assert 'Shader compilation error' not in text and 'Shader linking error' not in text
                uuid=re.search(r'EGL device UUID: (GPU-[a-f0-9-]+)',text)[1]
                assert uuid in devices,(uuid,devices)
                assert 'EGL config: RGBA 8/8/8/8; depth 24; stencil 8; samples 4' in text
                assert '4.3.0 NVIDIA' in text
                meta=json.loads(Path(str(image)+'.json').read_text());r=meta['render'];assert devices[uuid]['name'] in r['renderer']
                assert r['terrain_backend']=='cpu' and r['terrain_grass_planner']=='cpu' and r['foliage_gpu_compute']
                assert [r['width'],r['height']]==args.size and r['terrain_pixels']>0 and r['sky_pixels']>0
                rows=sorted(csv.DictReader(trace.open()),key=lambda r:int(r['frame']));assert [int(r['frame']) for r in rows]==list(range(args.frames))
                summary=summarize(rows);assert summary['gpu_valid_frames']==summary['measured_frames'],summary
                assert summary['counts']['scene_reuses']==0,summary
                evidence={'device':device,'uuid':uuid,'gpu':devices[uuid],'case':case,'pair':pair+1,'started_unix_s':started,'ended_unix_s':ended,
                          'command':cmd,'artifacts':{p.name:sha(p) for p in folder.iterdir() if p.is_file()},'render':r,'surface_camera':meta['surface_camera'],**summary}
                report['runs'].append(evidence);(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
                assert inputs()==frozen,'Benchmark source/config/binary changed during measurement'
                print(case,pair+1,devices[uuid]['name'],round(summary['stats']['frame_ms']['mean'],3),'ms',flush=True)
finally:
    monitor.terminate();monitor.wait(timeout=10);monitor_file.close()
# Within a pair, both cards must execute the same deterministic geometry/foliage workload.
for case in ['orbit','walking']:
    group=[r for r in report['runs'] if r['case']==case]
    assert len(set(r['uuid'] for r in group))==2
    for pair in range(args.pairs):
        a,b=[r for r in group if r['pair']==pair+1]
        assert a['surface_camera']==b['surface_camera']
        assert a['counts']==b['counts']
        for key in ['foliage_blades','foliage_candidates','foliage_patches','atmosphere_downsample']:
            assert a['render'][key]==b['render'][key],(case,pair,key,a['render'][key],b['render'][key])
        differences={}
        for key in ['foliage_gpu_drawn_blades','foliage_gpu_triangles','foliage_gpu_vertices']:
            x,y=a['render'][key],b['render'][key]
            assert abs(x-y)<=max(1,math.ceil(max(x,y)*.0001)),(case,pair,key,x,y)
            differences[key]=x-y
        images=[out/case/f'pair-{pair+1}'/f'device-{r["device"]}'/'capture.png' for r in [a,b]]
        result=subprocess.run(['compare','-metric','MAE',*map(str,images),'null:'],text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        assert result.returncode in [0,1],result.stderr
        normalized=float(re.search(r'\(([^)]+)\)',result.stderr)[1])
        assert normalized*255<1,('Image mismatch exceeds one 8-bit level mean',normalized)
        for r in [a,b]:r['pair_image_mean_error_8bit']=normalized*255;r['pair_gpu_count_differences']=differences
report['validation']='matched UUIDs, OpenGL/config/MSAA, camera, planned geometry/grass counts, frame receipts, complete GPU samples and frozen inputs; GPU cull/primitive counts within 0.01%, images within one 8-bit mean level'
(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
print('Validated all matched GPU benchmark pairs',flush=True)
