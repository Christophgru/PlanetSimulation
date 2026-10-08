"""Distance-aligned native route receipts; scalar parity is not image coverage."""
import math
import statistics
from receipts import percentiles, rows


def landmark(frames, distance):
    """Interpolate positions only; take workload/generation from a real frame."""
    for a, b in zip(frames, frames[1:]):
        lo, hi = a['pose']['walked_m'], b['pose']['walked_m']
        if lo <= distance <= hi and hi > lo:
            t = (distance - lo) / (hi - lo)
            nearest = a if t < .5 else b
            mix = lambda key: [x + t * (y - x) for x, y in zip(a[key], b[key])]
            root = [x + t * (y - x) for x, y in zip(a['pose']['root'], b['pose']['root'])]
            return {'distance_m': distance, 'bracket_frames': [a['profile_frame'], b['profile_frame']],
                    'fraction': t, 'nearest_frame': nearest['profile_frame'], 'root_m': root,
                    'camera_local': mix('camera_local'), 'workload': nearest['benchmark'],
                    'geometry': nearest['publication_geometry']}
    raise AssertionError(f'No route bracket at {distance} m')


def summarize_route(trace, frames, uuid, sprint, distance, units=1000):
    observed = {f['profile_frame']: f for f in frames}
    perf = {int(r['frame']): r for r in rows(trace)}
    native = {int(r['frame']): r for r in rows(str(trace) + '.native-loop.csv')}
    observers = {int(r['frame']): r for r in rows(trace.parent / 'frames.jsonl.observer.csv')}
    assert len(observed) == len(frames) and observed.keys() == perf.keys() == native.keys() == observers.keys()
    moving = [f for f in frames if f['controls'].get('phase') == 'route' and f['keys']['w'] and
              f['keys']['shift'] == sprint]
    assert moving and moving[-1]['pose']['walked_m'] >= distance
    assert all(b['frame'] == a['frame'] + 1 and b['pose']['walked_m'] > a['pose']['walked_m']
               for a, b in zip(moving, moving[1:]))
    measured = [f for f in moving if 5 <= f['pose']['walked_m'] < distance]
    assert len(measured) >= 240, len(measured)
    numbers = [f['profile_frame'] for f in measured]
    for f in moving:
        b = f['benchmark']
        assert f['mode'] == 3 and f['paused'] and f['selected'] == 0 and not f['pose']['airborne']
        assert b['quality_scale'] == 1 and b['viewport'] == b['scene_size'] == [1280, 720]
        assert native[f['profile_frame']]['outcome'] == 'rendered'
        assert abs(float(perf[f['profile_frame']]['simulation_s'])) < 1e-9
        assert b['bodies'][0]['foliage']['configured_budget'] == 2000000
        # F3's adaptive allowance is a measured quality input, not the hard cap.
        # compare_routes still reports unequal effective budgets as a mismatch.
        assert 0 < b['bodies'][0]['foliage']['budget'] <= b['bodies'][0]['foliage']['configured_budget']
        assert all(f['gl'][key] == 0 for key in ('blocking_polls', 'server_waits', 'bulk_reads', 'finishes', 'memory_queries'))
    first, last = measured[0], measured[-1]
    moved = last['pose']['walked_m'] - first['pose']['walked_m']
    elapsed = (last['observed_ns'] - first['observed_ns']) / 1e9
    animation = last['pose']['effect_s'] - first['pose']['effect_s']
    expected = 12 if sprint else 6
    assert abs(moved / elapsed / expected - 1) < .05, (expected, moved / elapsed)
    memory = rows(str(trace) + '.memory.csv')
    assert memory[0]['phase'] == 'renderer_ready' and memory[-1]['phase'] == 'shutdown'
    assert all(r['context_uuid'] == uuid and r['context_status'] == 'uuid_verified' and r['nvml_status'] == 'ok'
               and int(r['dropped_events']) == 0 for r in memory)
    physical = [int(r['used_bytes']) for r in memory if r['kind'] == 'sample' and
                first['observed_ns'] <= int(r['query_end_ns']) <= last['observed_ns']]
    assert len(physical) >= 3
    wall = [float(native[n]['wall_ms']) for n in numbers]
    q1, _, q3 = statistics.quantiles(wall, n=4, method='inclusive')
    fence = q3 + 1.5 * (q3 - q1)
    stats = {name: percentiles([float(native[n][name]) for n in numbers])
             for name in ('wall_ms', 'event_poll_ms', 'presentation_ms')}
    stats['observer_ms'] = percentiles([float(observers[n]['observer_ms']) for n in numbers])
    stats['gpu_frame_span_ms'] = percentiles([float(perf[n]['gpu_frame_span_ms']) for n in numbers
                                             if perf[n]['gpu_frame_span_ms']])
    stages = {key: percentiles([float(perf[n][key]) for n in numbers])
              for key in perf[numbers[0]] if (key.startswith('cpu_') or key.startswith('gpu_')) and key.endswith('_ms')}
    publications = rows(str(trace) + '.publications.csv')
    outcomes = {}
    starts_in_route = []
    for row in publications:
        outcomes[row['outcome']] = outcomes.get(row['outcome'], 0) + 1
        if row['start_frame'] and int(row['start_frame']) in numbers:
            starts_in_route.append(row)
    work = rows(str(trace) + '.gpu-work.csv')
    work_statuses = {}
    for row in work:
        work_statuses[row['status']] = work_statuses.get(row['status'], 0) + 1
    density = [f['benchmark']['bodies'][0]['foliage']['density'] for f in measured]
    lags = [math.dist([x / units for x in f['pose']['root']], f['benchmark']['bodies'][0]['foliage']['plan_eye']) * units
            for f in measured if f['benchmark']['bodies'][0]['foliage']['plan_eye']]
    return {'measured_frames': len(measured), 'excluded_frames': len(frames) - len(measured),
            'first_measured_frame': numbers[0], 'last_measured_frame': numbers[-1],
            'measured_distance_interval_m': [first['pose']['walked_m'], last['pose']['walked_m']],
            'wall_mps': moved / elapsed, 'animation_mps': moved / animation, 'observed_duration_s': elapsed,
            'stats': stats, 'stages': stages, 'upper_outlier_fence_ms': fence,
            'upper_outliers_retained': sum(x > fence for x in wall),
            'gpu_status_counts': {status: sum(perf[n]['gpu_status'] == status for n in numbers)
                                  for status in sorted({perf[n]['gpu_status'] for n in numbers})},
            'memory': {'sampled_device_used_peak_bytes': max(physical), 'physical_samples': len(physical),
                       'logical_overlap_peak_bytes': max((int(r['overlap_reserved_bytes']) for r in memory
                                                          if r['overlap_reserved_bytes']), default=None),
                       'physical_scope': 'periodic device-wide samples; includes other processes and can miss peaks'},
            'effective_density': percentiles(density), 'root_to_grass_plan_eye_m': percentiles(lags),
            'mesh_upload_frames': sum(int(perf[n]['mesh_uploads']) > 0 for n in numbers),
            'foliage_rebuilds': sum(int(perf[n]['foliage_rebuilds']) for n in numbers),
            'worker_build_ms': sum(float(perf[n]['terrain_build_ms']) for n in numbers),
            'publication_outcomes_all_frames': outcomes, 'publications_admitted_in_interval': starts_in_route,
            'gpu_work_statuses_all_frames': work_statuses,
            'landmarks': [landmark(moving, d) for d in range(25, int(distance) + 1, 25)],
            'distance_bins': [{'start_m': d, 'end_m': d + 25,
                               'wall_ms': percentiles([float(native[f['profile_frame']]['wall_ms']) for f in measured
                                                       if d <= f['pose']['walked_m'] < d + 25])}
                              for d in range(0, int(distance), 25)],
            'renderer': first['renderer'], 'opengl_version': first['opengl_version'],
            'initial_root_m': moving[0]['pose']['root'], 'gl': frames[-1]['gl']}


def compare_routes(cpu, compute):
    root_errors, camera_errors, density_ratios, differences, empty_density = [], [], [], [], []
    for a, b in zip(cpu['landmarks'], compute['landmarks']):
        assert a['distance_m'] == b['distance_m']
        root_errors.append(math.dist(a['root_m'], b['root_m']))
        camera_errors.append(math.dist(a['camera_local'], b['camera_local']) * 1000)
        for x, y in zip(a['workload']['bodies'], b['workload']['bodies']):
            for key in ('configured_density', 'configured_budget', 'configured_distance_m', 'budget', 'distance_m', 'compute_placement', 'enabled'):
                if x['foliage'][key] != y['foliage'][key]:
                    differences.append({'distance_m': a['distance_m'], 'body': x['body'], 'key': key,
                                        'cpu': x['foliage'][key], 'compute': y['foliage'][key]})
            if x['foliage']['enabled']:
                cd, gd = x['foliage']['density'], y['foliage']['density']
                if cd > 0:
                    density_ratios.append(gd / cd)
                elif gd == 0:
                    empty_density.append({'distance_m': a['distance_m'], 'body': x['body']})
                else:
                    differences.append({'distance_m': a['distance_m'], 'body': x['body'],
                                        'key': 'empty_cpu_density', 'cpu': cd, 'compute': gd})
    return {'fixed_quality_and_budget_matched': not differences, 'differences': differences,
            'max_landmark_root_difference_m': max(root_errors), 'max_landmark_camera_difference_m': max(camera_errors),
            'effective_density_ratio_range': [min(density_ratios), max(density_ratios)] if density_ratios else None,
            'empty_density_landmarks': empty_density,
            'wall_p95_ratio': compute['stats']['wall_ms']['p95'] / cpu['stats']['wall_ms']['p95'],
            'distance_bin_ratios': [{'start_m': a['start_m'], 'end_m': a['end_m'],
                                    'cpu_frames': a['wall_ms']['samples'], 'compute_frames': b['wall_ms']['samples'],
                                    'wall_p95_ratio': b['wall_ms']['p95'] / a['wall_ms']['p95']}
                                   for a, b in zip(cpu['distance_bins'], compute['distance_bins'])],
            'coverage': 'pending independent rendered/root distribution checks; scalar budget/density cannot establish parity',
            'cost_acceptance': 'pending rendered coverage, stationary regression investigation and remaining migration gates'}
