"""Native receipt joins and explicit production workload preflight checks."""
import csv
import math
from pathlib import Path
import statistics


def rows(path):
    with Path(path).open() as source:
        return list(csv.DictReader(source))


def percentiles(values):
    values = sorted(values)
    if not values:
        return {'samples': 0}
    return {'samples': len(values), 'mean': statistics.mean(values),
            **{name: values[math.ceil(fraction * len(values)) - 1]
               for name, fraction in [('p50', .5), ('p95', .95), ('p99', .99)]},
            'min': values[0], 'max': values[-1]}


def summarize(trace, frames, expected_uuid):
    performance = {int(r['frame']): r for r in rows(trace)}
    native = {int(r['frame']): r for r in rows(str(trace) + '.native-loop.csv')}
    observer = {int(r['frame']): r for r in rows(trace.parent / 'frames.jsonl.observer.csv')}
    observed = {f['profile_frame']: f for f in frames}
    assert len(observed) == len(frames) and performance.keys() == native.keys() == observed.keys() == observer.keys()
    measured = [number for number, f in observed.items() if f['controls'].get('phase') == 'measured']
    assert len(measured) >= 200, len(measured)
    assert all(native[n]['outcome'] == 'rendered' for n in measured)
    assert all(f['paused'] and f['benchmark']['quality_scale'] == 1 and
               f['benchmark']['viewport'] == f['benchmark']['scene_size'] == [1280, 720]
               for f in frames if f['profile_frame'] in measured)
    assert all(abs(float(performance[n]['simulation_s'])) < 1e-9 for n in measured)
    memory = rows(str(trace) + '.memory.csv')
    assert memory[0]['phase'] == 'renderer_ready' and memory[-1]['phase'] == 'shutdown'
    assert all(r['context_uuid'] == expected_uuid and r['nvml_status'] == 'ok' and
               r['context_status'] == 'uuid_verified' for r in memory)
    assert not any(int(r['dropped_events']) for r in memory)
    first = observed[measured[0]]
    start_ns, end_ns = first['observed_ns'], observed[measured[-1]]['observed_ns']
    physical = [int(r['used_bytes']) for r in memory if r['kind'] == 'sample' and
                start_ns <= int(r['query_end_ns']) <= end_ns]
    assert len(physical) >= 3
    summaries = {key: percentiles([float(native[n][key]) for n in measured])
                 for key in ['wall_ms', 'event_poll_ms', 'presentation_ms']}
    summaries['observer_ms'] = percentiles([float(observer[n]['observer_ms']) for n in measured])
    summaries['gpu_frame_span_ms'] = percentiles([float(performance[n]['gpu_frame_span_ms'])
                                                 for n in measured if performance[n]['gpu_frame_span_ms']])
    summaries['frame_ms'] = percentiles([float(performance[n]['frame_ms']) for n in measured])
    summary = {'measured_frames': len(measured), 'excluded_frames': len(frames) - len(measured),
               'first_measured_frame': min(measured), 'last_measured_frame': max(measured),
               'observed_duration_s': (end_ns - start_ns) / 1e9,
               'stats': summaries, 'gpu_status_counts': {},
               'workload_first': first['benchmark'], 'workload_last': observed[measured[-1]]['benchmark'],
               'camera_local': first['camera_local'], 'pose_root': first['pose']['root'],
               'sampled_device_used_peak_bytes': max(physical),
               'sampled_device_used_min_bytes': min(physical), 'physical_samples': len(physical),
               'logical_overlap_peak_bytes': max((int(r['overlap_reserved_bytes']) for r in memory
                                                 if r['overlap_reserved_bytes']), default=None),
               'publication_outcomes': {}, 'gl': frames[-1]['gl'],
               'renderer': first['renderer'], 'opengl_version': first['opengl_version']}
    for n in measured:
        status = performance[n]['gpu_status']
        summary['gpu_status_counts'][status] = summary['gpu_status_counts'].get(status, 0) + 1
    for r in rows(str(trace) + '.publications.csv'):
        status = r['outcome']
        summary['publication_outcomes'][status] = summary['publication_outcomes'].get(status, 0) + 1
    summary['observer_mean_fraction'] = summaries['observer_ms']['mean'] / summaries['wall_ms']['mean']
    return summary


def compare_pair(cpu, compute):
    """Report mismatches; never disguise an unmatched run as an accepted speedup."""
    differences = []
    a, b = cpu['workload_first'], compute['workload_first']
    for key in ['viewport', 'scene_size', 'quality_scale']:
        if a[key] != b[key]:
            differences.append({'key': key, 'cpu': a[key], 'compute': b[key]})
    if math.dist(cpu['pose_root'], compute['pose_root']) > .001:
        differences.append({'key': 'pose_root', 'distance_m': math.dist(cpu['pose_root'], compute['pose_root'])})
    for x, y in zip(a['bodies'], b['bodies']):
        body = x['body']
        for key in ['triangles', 'unique_samples', 'topology_input_bytes']:
            if x[key] != y[key]:
                differences.append({'body': body, 'key': key, 'cpu': x[key], 'compute': y[key]})
        for key in ['enabled', 'configured_density', 'configured_budget', 'configured_distance_m', 'budget', 'distance_m', 'compute_placement']:
            if x['foliage'][key] != y['foliage'][key]:
                differences.append({'body': body, 'key': key, 'cpu': x['foliage'][key], 'compute': y['foliage'][key]})
        cd, gd = x['foliage']['density'], y['foliage']['density']
        if not math.isclose(cd, gd, rel_tol=.001, abs_tol=1e-6):
            differences.append({'body': body, 'key': 'effective_density', 'cpu': cd, 'compute': gd})
        # Slot count is evidence, not an equality gate: gpu-v1 and legacy use
        # distinct versioned rounded-slot policies. Coverage still needs images.
    return {'scalar_workload_matched': not differences, 'differences': differences,
            'wall_p95_ratio': compute['stats']['wall_ms']['p95'] / cpu['stats']['wall_ms']['p95'],
            'image_and_near_root_coverage': 'pending; scalar receipts alone do not prove rendered coverage',
            'cost_acceptance': 'pending movement/Moon/reload, coverage and bulk-field gates'}
