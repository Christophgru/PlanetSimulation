#!/usr/bin/env python3
"""Publish compact journal tables/figure from a completed local feature study."""
import argparse
from collections import defaultdict
import csv
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import statistics
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(Path(__file__).parent))
from run import stats

STAGES = ['opaque', 'reflection', 'atmosphere', 'reflection_atmosphere', 'shadows', 'water', 'tables', 'lens_flare', 'overlay']
LABELS = ['Opaque scene (terrain/grass/ACES)', 'Reflected geometry', 'Main atmosphere/HDR',
          'Reflected atmosphere', 'Terrain shadows', 'Water composite', 'Atmosphere tables', 'Lens flare', 'Overlay']
CASES = ['surface-idle', 'surface-walk', 'aces-idle', 'aces-walk', 'mountain-idle']
NAMES = ['Surface, fixed', 'Surface, walking', 'ACES, fixed', 'ACES, walking', 'Mountain, fixed']


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('results', type=Path)
    p.add_argument('--journal-dir', type=Path, required=True)
    args = p.parse_args()
    report = json.loads(args.results.read_text())
    assert report.get('validation'), 'Measurement has not finished'
    out = args.journal_dir
    out.mkdir(parents=True, exist_ok=True)
    groups = defaultdict(list)
    for run in report['runs']:
        groups[(run['case'], run['device'])].append(run)
    summary = {k: report[k] for k in ('generated_utc', 'revision', 'binary_sha256', 'config_sha256',
               'asset_sha256', 'script_sha256', 'adapter_source_sha256', 'baseline_device_telemetry', 'method', 'validation')}
    if 'diagnostic_script_sha256' in report:
        summary['diagnostic_script_sha256'] = report['diagnostic_script_sha256']
    summary['config_snapshot'] = 'inputs.json'
    summary['results_sha256'] = hashlib.sha256(args.results.read_bytes()).hexdigest()
    summary['workloads'] = []
    table = []
    nested_table = []
    for case in CASES:
        for device in sorted({key[1] for key in groups}):
            runs = groups[(case, device)]
            assert len(runs) == report['method']['pairs']
            all_rows, work = [], []
            for run in runs:
                command = run['command']
                trace = Path(command[command.index('--performance-trace') + 1])
                rows = list(csv.DictReader(trace.open()))
                keep = [r for r in rows if report['method']['warmup'] <= int(r['frame']) < report['method']['excluded_final_frame']]
                assert len(keep) == run['measured_frames'] and all(r['gpu_status'] == 'ready' for r in keep)
                all_rows += keep
                work += [r for r in csv.DictReader(Path(str(trace) + '.gpu-work.csv').open())
                         if r['frame'] and report['method']['warmup'] <= int(r['frame']) < report['method']['excluded_final_frame']]
            steady = [r for r in all_rows if not int(r['mesh_uploads']) and not int(r['foliage_rebuilds']) and float(r['terrain_build_ms']) == 0]
            columns = [k for k in all_rows[0] if k.endswith('_ms')]
            compute = defaultdict(list)
            for r in work:
                compute[r['stage'] + (':' + r['view'] if r['view'] else '')].append(r)
            entry = {'case': case, 'device': device, 'gpu': runs[0]['gpu'], 'frames': len(all_rows), 'steady_frames': len(steady),
                     'stats': {k: stats([float(r[k]) for r in all_rows if r[k]]) for k in columns},
                     'steady_stats': {k: stats([float(r[k]) for r in steady if r[k]]) for k in columns},
                     'rebuild_frames': len(all_rows) - len(steady),
                     'cold_frame_ms': stats([float(r['cold_frame']['frame_ms']) for r in runs]),
                     'terrain_build_ms': stats([float(r['cold_frame']['terrain_build_ms']) for r in runs]),
                     'drawn_grass_range': [min(r['render']['foliage_gpu_drawn_blades'] for r in runs), max(r['render']['foliage_gpu_drawn_blades'] for r in runs)],
                     'grass_triangles_range': [min(r['render']['foliage_gpu_triangles'] for r in runs), max(r['render']['foliage_gpu_triangles'] for r in runs)],
                     'protected_density_range': [min(p['density'] for r in runs for p in r['render']['foliage_policy'] if p and p['body']=='earth'),
                                                max(p['density'] for r in runs for p in r['render']['foliage_policy'] if p and p['body']=='earth')],
                     'near_infeasible': any(p and p['near_infeasible'] for r in runs for p in r['render']['foliage_policy']),
                     'nested_work': {k: {'events': len(v), 'missing': sum(r['status'] != 'ready' for r in v),
                                        'cpu_submit_ms': stats([float(r['cpu_submit_ms']) for r in v]),
                                        'gpu_ms': stats([float(r['gpu_ms']) for r in v if r['status'] == 'ready'])} for k,v in compute.items()}}
            summary['workloads'].append(entry)
            s = entry['stats']
            table.append(f"| {case} | {entry['gpu']['name'].replace('NVIDIA GeForce ','')} | {s['frame_ms']['median']:.2f} | {s['frame_ms']['p95']:.2f} | {s['frame_ms']['p99']:.2f} | {s['frame_ms']['max']:.2f} | {entry['rebuild_frames']} |")
            for stage in ('grass_placement:main', 'grass_placement:reflection', 'grass_metadata', 'grass_allocation', 'grass_roots', 'terrain_field', 'terrain_expansion'):
                n = entry['nested_work'].get(stage)
                if n and case in ('surface-walk', 'aces-walk'):
                    nested_table.append(f"| {case} | {entry['gpu']['name'].replace('NVIDIA GeForce ','')} | {stage} | {n['events']} | {n['gpu_ms']['mean']:.3f} | {n['gpu_ms']['p95']:.3f} |")
    summary['cpu_diagnostics'] = [{k: r[k] for k in ('case','device','uuid','measured_frames','stats','cpu_scopes','cold_cpu_scopes','cpu_trace_sha256')}
                                for r in report['cpu_diagnostics']]
    (out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    (out / 'inputs.json').write_bytes((args.results.parent / 'startup.json').read_bytes())
    feature_table = []
    entries = {(r['case'],r['device']):r for r in summary['workloads']}
    devices = sorted({key[1] for key in entries})
    assert len(devices) == 2
    for stage, label in zip(STAGES, LABELS):
        cells = [f"{entries[(case,d)]['stats']['gpu_'+stage+'_ms']['mean']:.3f}" for case in ('surface-idle','aces-walk') for d in devices]
        feature_table.append('| ' + label + ' | ' + ' | '.join(cells) + ' |')
    cpu_table = []
    for r in summary['cpu_diagnostics']:
        thread = next(s['thread'] for s in r['cpu_scopes'] if s['scope']=='capture.frame')
        rows = {s['scope']:s for s in r['cpu_scopes'] if s['thread']==thread}
        for name in ('capture.frame','mesh','ProceduralGrass::draw','opaque','reflection','atmosphere','reflection_atmosphere'):
            s = rows.get(name)
            if s:
                cpu_table.append(f"| {r['case']} | {r['device']} | {name} | {s['inclusive_ms']/r['measured_frames']:.3f} | {s['thread_cpu_ms']/r['measured_frames']:.3f} |")
    cold_table = []
    for r in summary['cpu_diagnostics']:
        for s in r['cold_cpu_scopes']:
            if s['scope'] in ('TerrainSurface::buildTopologyForEye','TerrainSurface::refineSurfaceError','TerrainSurface::refineShoreline','TerrainSurface::evaluateTopology','planGrass','ProceduralGrass::upload'):
                cold_table.append(f"| {r['case']} | {r['device']} | {s['thread']} / {s['scope']} | {s['inclusive_ms']:.2f} | {s['thread_cpu_ms']:.2f} |")
    # Generated tables go to the local build folder, ready to incorporate with interpretation.
    (args.results.parent / 'tables.md').write_text('\n'.join(table) + '\n\n' + '\n'.join(feature_table) + '\n\n' + '\n'.join(nested_table) + '\n\n' + '\n'.join(cpu_table) + '\n\n' + '\n'.join(cold_table) + '\n')
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    plt.rcParams.update({'font.family':'DejaVu Sans', 'font.size':9, 'svg.hashsalt':'current-feature-costs'})
    fig, axes = plt.subplots(1,2,figsize=(11,5.4),sharey=True)
    colors = ['#287f8e','#5fa5b4','#e6a144','#edc17a','#8068a4','#4f8e63','#9f8b6b','#a65668','#999999']
    for ax, device in zip(axes, devices):
        left = [0.0] * len(CASES)
        for stage, label, color in zip(STAGES, LABELS, colors):
            values = [entries[(case,device)]['stats']['gpu_'+stage+'_ms']['mean'] for case in CASES]
            ax.barh(range(len(CASES)), values, left=left, color=color, label=label)
            left = [a+b for a,b in zip(left,values)]
        for i, total in enumerate(left):
            ax.text(total+.01*max(left),i,f'{total:.2f} ms',va='center')
        ax.set_xlim(0,max(left)*1.21); ax.set_yticks(range(len(CASES)),NAMES); ax.set_xlabel('Mean GPU render scopes (ms; separate x scales)')
        ax.set_title(entries[(CASES[0],device)]['gpu']['name'].replace('NVIDIA GeForce ',''))
        ax.axvline(1000/60,color='#444444',linestyle='--',linewidth=1)
        ax.grid(axis='x',alpha=.15); ax.set_axisbelow(True)
    axes[0].invert_yaxis()
    handles, labels = axes[0].get_legend_handles_labels()
    fig.subplots_adjust(left=.13, right=.98, top=.84, bottom=.29, wspace=.08)
    fig.legend(handles,labels,loc='lower center',ncol=3,frameon=False,fontsize=8)
    fig.suptitle('Current production feature costs — three repeats, 297 measured frames per workload/GPU', fontsize=11)
    svg = out/'gpu-features.svg'
    fig.savefig(svg,metadata={'Date':None})
    svg.write_text('\n'.join(line.rstrip() for line in svg.read_text().splitlines()) + '\n')
    fig.savefig(args.results.parent/'gpu-features.png', dpi=160)
    print('Published compact summary, input snapshot, feature plot and local tables')


if __name__ == '__main__':
    main()
