#!/usr/bin/env python3
"""Check attribution independently of clocks and real workload durations."""
import importlib.util
from pathlib import Path
import tempfile
import sys
sys.dont_write_bytecode = True
import unittest

root = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location('cpu_report', root/'scripts/benchmarks/cpu_report.py')
report = importlib.util.module_from_spec(spec)
spec.loader.exec_module(report)


def event(name, start, duration, cpu=None, tid=0):
    return dict(ph='X', name=name, ts=start, dur=duration, tid=tid,
                args={} if cpu is None else dict(thread_cpu_us=cpu))


class Attribution(unittest.TestCase):
    def test_nested_siblings_recursion_and_workers(self):
        events = [event('root', 0, 10000, 8000), event('recursive', 1000, 5000, 4000),
                  event('recursive', 2000, 2000, 1000), event('sibling', 6000, 2000, 2000),
                  event('worker', 0, 10000, 9000, tid=1)]
        _, rows, _, reverse = report.analyze(dict(traceEvents=list(reversed(events))))
        self.assertEqual(rows[('thread [0]', 'root')]['self_ms'], 3)
        self.assertEqual(rows[('thread [0]', 'recursive')]['self_ms'], 5)
        self.assertEqual(rows[('thread [0]', 'recursive')]['self_thread_cpu_ms'], 4)
        self.assertEqual(sum(r['self_ms'] for (t, _), r in rows.items() if t == 'thread [0]'), 10)
        self.assertEqual(sum(reverse.values()), 20)  # Two threads, each 10 ms.
        self.assertIn(('thread [0]', ('recursive', 'recursive', 'root')), reverse)

    def test_unavailable_thread_clock_propagates(self):
        _, rows, _, _ = report.analyze(dict(traceEvents=[event('parent', 0, 5, 3), event('child', 1, 1)]))
        self.assertIsNone(rows[('thread [0]', 'parent')]['self_thread_cpu_ms'])

    def test_crossing_intervals_rejected(self):
        with self.assertRaises(ValueError):
            report.analyze(dict(traceEvents=[event('a', 0, 3), event('b', 2, 3)]))

    def test_frame_slice_excludes_children_of_incomplete_worker_jobs(self):
        data = dict(traceEvents=[event('capture.frame', 10, 10),
                                event('render.child', 12, 2),
                                event('early.job', 5, 10, tid=1),
                                event('early.child', 11, 2, tid=1),
                                event('late.job', 18, 10, tid=1),
                                event('late.child', 18, 1, tid=1),
                                event('complete.job', 15, 2, tid=1)])
        events, _, _, _ = report.analyze(data, frame_count=1)
        self.assertEqual({e['name'] for e in events},
                         {'capture.frame', 'render.child', 'complete.job'})
        events, _, _, _ = report.analyze(data)
        self.assertEqual(len(events), 7)

    def test_frame_selection_and_escaped_standalone_report(self):
        data = dict(traceEvents=[event('capture.frame', 0, 10), event('warmup', 1, 5),
                                 event('capture.frame', 11, 10), event('<unsafe>', 12, 5)])
        with tempfile.TemporaryDirectory() as tmp:
            report.write_report(data, Path(tmp), 1, 1)
            html = (Path(tmp)/'report.html').read_text()
            self.assertNotIn('warmup', html)
            self.assertIn('&lt;unsafe&gt;', html)
            self.assertNotIn('<unsafe>', html)
            self.assertTrue((Path(tmp)/'scopes.csv').exists())


if __name__ == '__main__':
    unittest.main()
