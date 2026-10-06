"""Check bounded native memory receipts without interpreting them as cost gates."""
import csv
import os
from collections import Counter
from pathlib import Path


def validate_memory(path, backend, renderer):
    rows = list(csv.DictReader(Path(path).open()))
    assert rows[0]['phase'] == 'renderer_ready' and rows[-1]['phase'] == 'shutdown'
    assert all(r['renderer'] == renderer for r in rows)
    assert all(int(r['dropped_events']) == 0 for r in rows)
    assert all(int(r['dropped_observations']) <= int(r['skipped_latest_observations']) for r in rows)
    expected_uuid = os.getenv('TEST_EXPECT_GPU_UUID')
    for row in rows:
        assert int(row['query_end_ns']) >= int(row['query_begin_ns'])
        assert float(row['query_ms']) >= 0 and float(row['observation_age_ms']) >= 0
        assert row['sample_after_observation'] == str(int(int(row['query_end_ns']) > int(row['observed_ns'])))
        assert row['stale'] == str(int(float(row['sample_age_ms']) > 2000))
        assert row['cpu_scope'] == 'partial_owned_vectors' and row['rss_status'] == 'ok'
        assert int(row['process_rss_bytes']) > 0
        if row['scheduler_status'] == 'ok':
            assert all(int(row[k]) <= 1 for k in ('worker_running', 'worker_queued', 'worker_ready'))
        else:
            assert all(not row[k] for k in ('ready_vector_bytes', 'worker_running', 'worker_queued', 'worker_ready'))
        assert row['logical_status'] == ('resident_reservation' if backend == 'compute' else 'unavailable')
        if backend == 'compute':
            assert int(row['overlap_reserved_bytes']) == max(int(row['live_reserved_bytes']), int(row['replacement_reserved_bytes']))
        else:
            assert not row['overlap_reserved_bytes']
        if row['nvml_status'] == 'ok':
            assert row['context_status'] == 'uuid_verified'
            assert int(row['total_bytes']) > 0
            assert int(row['used_bytes']) + int(row['free_bytes']) <= int(row['total_bytes'])
        else:
            assert all(not row[k] for k in ('total_bytes', 'used_bytes', 'free_bytes'))
        if expected_uuid:
            assert row['context_uuid'] == expected_uuid and row['nvml_status'] == 'ok'
            assert row['nvx_status'] == 'ok'
        assert row['physical_scope'] == 'device_wide_other_processes_included'
        assert row['transient_peak_scope'] == 'periodic_samples_may_miss_peaks'
    phases = Counter(r['phase'] for r in rows)
    assert phases['steady'] and phases['minimized']
    if backend == 'compute':
        assert phases['loading']
    samples = [r for r in rows if r['kind'] == 'sample']
    assert len(samples) >= 3
    physical = [int(r['used_bytes']) for r in samples if r['nvml_status'] == 'ok']
    return {'rows': len(rows), 'samples': len(samples), 'phases': dict(phases),
            'nvml_statuses': dict(Counter(r['nvml_status'] for r in rows)),
            'context_uuid': rows[0]['context_uuid'],
            'dropped_observations': int(rows[-1]['dropped_observations']),
            'dropped_events': int(rows[-1]['dropped_events']),
            'skipped_latest_observations': int(rows[-1]['skipped_latest_observations']),
            'sampled_device_used_peak_bytes': max(physical) if physical else None,
            'logical_overlap_peak_bytes': max(int(r['overlap_reserved_bytes']) for r in rows) if backend == 'compute' else None}
