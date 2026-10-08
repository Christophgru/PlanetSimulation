#!/usr/bin/env python3
"""Optional archive availability, unchanged hash checks and output isolation."""
import argparse
import contextlib
import hashlib
import io
import json
from pathlib import Path
import runpy
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[3]/'scripts/benchmarks'))
from archives import local


class ArchiveTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.study = self.root/'docs/journal/example'
        self.study.mkdir(parents=True)
        self.archive = self.root/'build-benchmarks/archives'
        self.payload = b'original benchmark data'
        self.metadata = self.study/'evidence.json'
        self.metadata.write_text(json.dumps({'artifact_sha256': {
            'validation/frame.jsonl': hashlib.sha256(self.payload).hexdigest()}}))
        self.catalog = self.root/'catalog.json'
        self.catalog.write_text(json.dumps({'studies': {'docs/journal/example': {
            'evidence': 'evidence.json', 'regenerate': ['python3 run.py --output-dir build-benchmarks/runs/example']}}}))
        self.script = self.study/'validate.py'
        self.script.write_text('''from archives.local import prepare_archive
import hashlib, json
base = prepare_archive(__file__)
expected = json.loads((base/'evidence.json').read_text())['artifact_sha256']
for name, digest in expected.items():
    assert hashlib.sha256((base/name).read_bytes()).hexdigest() == digest
(base/'validation-result.json').write_text('validated')
print('Validated original bytes')
''')
        (self.study/'validation-result.json').write_text('historical summary')
        for name, value in [('ROOT', self.root), ('CATALOG', self.catalog)]:
            patcher = patch.object(local, name, value)
            patcher.start()
            self.addCleanup(patcher.stop)
        patcher = patch.dict('os.environ', {'PLANET_BENCHMARK_ARCHIVE_ROOT': str(self.archive)})
        patcher.start()
        self.addCleanup(patcher.stop)

    def install_payload(self, payload):
        target = self.archive/'docs/journal/example/validation/frame.jsonl'
        target.parent.mkdir(parents=True)
        target.write_bytes(payload)
        return target

    def test_missing_archive_is_not_a_pass(self):
        stream = io.StringIO()
        with contextlib.redirect_stderr(stream), self.assertRaises(SystemExit) as error:
            runpy.run_path(str(self.script), run_name='__main__')
        self.assertEqual(error.exception.code, 2)
        self.assertIn('UNAVAILABLE', stream.getvalue())
        self.assertIn('No archive validation was performed', stream.getvalue())
        self.assertIn('--output-dir build-benchmarks/runs/example', stream.getvalue())
        self.assertFalse(self.archive.exists())

    def test_local_archive_preserves_checks_and_keeps_result_local(self):
        target = self.install_payload(self.payload)
        with contextlib.redirect_stdout(io.StringIO()) as stream:
            runpy.run_path(str(self.script), run_name='__main__')
        self.assertIn('Validated original bytes', stream.getvalue())
        self.assertEqual(target.read_bytes(), self.payload)
        self.assertEqual((self.study/'validation-result.json').read_text(), 'historical summary')
        self.assertFalse((self.study/'validation/frame.jsonl').exists())

    def test_corrupt_local_archive_still_fails_hash_check(self):
        self.install_payload(b'changed bytes')
        with contextlib.redirect_stdout(io.StringIO()), self.assertRaises(AssertionError):
            runpy.run_path(str(self.script), run_name='__main__')

    def test_original_checker_bytes_and_sibling_path_are_preserved(self):
        self.install_payload(self.payload)
        original = self.study/'validation/validator.py'
        original.parent.mkdir(exist_ok=True)
        original.write_bytes(b'original checker bytes')
        records = json.loads(self.catalog.read_text())
        entry = records['studies']['docs/journal/example']
        entry.update(validator='validation/validator.py',
                     validator_sha256=hashlib.sha256(original.read_bytes()).hexdigest())
        self.catalog.write_text(json.dumps(records))
        with contextlib.redirect_stdout(io.StringIO()):
            view = local.prepare_archive(self.script)
        self.assertEqual((view/'validate.py').read_bytes(), original.read_bytes())
        original.write_bytes(b'modified checker')
        with contextlib.redirect_stdout(io.StringIO()), self.assertRaises(ValueError):
            local.prepare_archive(self.script)

    def test_outputs_cannot_pollute_tracked_folders(self):
        for path in (self.root, self.root/'docs/journal/new-run', self.root/'tests/results'):
            with self.assertRaises(argparse.ArgumentTypeError):
                local.local_output(path)
        self.assertEqual(local.local_output(self.root/'build-benchmarks/runs/new'), self.root/'build-benchmarks/runs/new')
        self.assertEqual(local.local_output(self.root.parent/'external-runs'), self.root.parent/'external-runs')

    def test_relocation_preserves_bytes_and_refuses_overwrites(self):
        source = self.study/'frames.jsonl'
        source.write_bytes(self.payload)
        with contextlib.redirect_stdout(io.StringIO()):
            local.relocate()
        target = self.archive/source.relative_to(self.root)
        self.assertEqual(target.read_bytes(), self.payload)
        self.assertFalse(source.exists())
        source.write_bytes(b'new data')
        with self.assertRaises(ValueError):
            local.relocate()
        self.assertEqual(source.read_bytes(), b'new data')
        self.assertEqual(target.read_bytes(), self.payload)


if __name__ == '__main__':
    unittest.main()
