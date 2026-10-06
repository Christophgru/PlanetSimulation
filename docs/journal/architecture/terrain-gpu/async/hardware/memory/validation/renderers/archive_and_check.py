from pathlib import Path
import csv,gzip,json,shutil,sys
root=Path('/workspace');base=root/'docs/journal/architecture/terrain-gpu/async/hardware/memory/validation/renderers'/sys.argv[1];base.mkdir(parents=True,exist_ok=True)
results={}
for name in ['memory-cpu','memory-resident']:
 src=root/'build-resume/terrain-frame'/name;target=base/name;target.mkdir(exist_ok=True)
 for p in src.iterdir():
  if p.name=='scene.json' or p.suffix=='.png' or p.name.endswith('memory-audit.json') or '.csv' in p.name:
   if p.suffix in ['.png','.json']:shutil.copyfile(p,target/p.name)
   else:
    with (target/(p.name+'.gz')).open('wb') as f:
     with gzip.GzipFile(fileobj=f,mode='wb',mtime=0) as z:z.write(p.read_bytes())
 trace=src/'scene.json.frames.csv'
 rows=list(csv.DictReader(Path(str(trace)+'.memory.csv').open()))
 pubs={r['attempt']:r for r in csv.DictReader(Path(str(trace)+'.publications.csv').open()) if r['kind']=='reload'}
 joined=0
 for r in rows:
  if r['kind']!='event' or not r['phase'].startswith('reload_'):continue
  p=pubs[r['reload_attempt']];assert p['epoch']==r['replacement_epoch']
  if r['phase']=='reload_failed':assert p['outcome']=='invalid_config'
  if r['phase']=='reload_superseded':assert p['outcome']=='superseded'
  if r['phase']=='reload_exchange':assert p['scene_exchange_ms']
  joined+=1
 results[name]={'rows':len(rows),'samples':sum(r['kind']=='sample' for r in rows),'phases':sorted({r['phase'] for r in rows}),'root_events_joined':joined,'roots':len(pubs),'renderer':rows[0]['renderer'],'uuid':rows[0]['context_uuid'],'dropped_events':int(rows[-1]['dropped_events']),'skipped_latest_observations':int(rows[-1]['skipped_latest_observations']),'logical_overlap_peak_bytes':max(int(r['overlap_reserved_bytes']) for r in rows) if name=='memory-resident' else None}
 print(sys.argv[1],name,'joined',joined,'events to',len(pubs),'reload roots')
(base/'results.json').write_text(json.dumps(results,indent=2)+'\n')
