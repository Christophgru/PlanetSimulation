#!/usr/bin/env python3
"""Keep historical benchmark payloads local; never treat missing data as valid."""
import argparse
import atexit
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
CATALOG = Path(__file__).with_name('catalog.json')
RAW_SUFFIXES = {'.gz', '.xz', '.jsonl', '.depth', '.blades', '.rgba',
                '.rgba32f', '.stencil', '.indices', '.depth-keys', '.input', '.permuted'}


def raw_payload(path):
    return path.suffix in RAW_SUFFIXES or bool(re.search(
        r'(^frames\.csv$|^performance\.csv|\.frames\.csv|\.native-loop\.csv|'
        r'\.gpu-work\.csv|\.memory\.csv|\.publications\.csv)', path.name))


def local_output(value):
    """Argparse path type: new runs go outside the repo or into ignored build trees."""
    path = Path(value).resolve()
    if path.is_relative_to(ROOT):
        first = path.relative_to(ROOT).parts
        if not first or not (first[0] == 'build' or first[0].startswith(('build-', 'build_', 'cmake-build-'))):
            raise argparse.ArgumentTypeError('Benchmark output must be local: use build-benchmarks/runs/... or a directory outside the repository')
    return path


def archive_root():
    return local_output(os.environ.get('PLANET_BENCHMARK_ARCHIVE_ROOT', ROOT/'build-benchmarks/archives'))


def catalog():
    return json.loads(CATALOG.read_text())['studies']


def artifact_paths(study, entry):
    evidence = json.loads((study/entry['evidence']).read_text())
    paths = evidence.get('artifact_sha256', evidence.get('artifacts'))
    if not isinstance(paths, dict):
        raise ValueError(f'Invalid evidence manifest: {study/entry["evidence"]}')
    for name in paths:
        path = study/name
        if not path.resolve().is_relative_to(ROOT):
            raise ValueError(f'Archive path escapes repository: {name}')
        yield path


def missing_payloads(study, entry, local):
    return [p for p in artifact_paths(study, entry)
            if not p.is_file() and not (local/p.relative_to(ROOT)).is_file()]


def unavailable(study, entry, missing, local):
    print(f'UNAVAILABLE: optional historical archive {study.relative_to(ROOT)}', file=sys.stderr)
    print(f'{len(missing)} required artifacts are missing. No archive validation was performed.', file=sys.stderr)
    print(f'Local archive root: {local}', file=sys.stderr)
    print(f'Example: {missing[0].relative_to(ROOT)}', file=sys.stderr)
    print('Regenerate a new run with the current source (not the original historical receipts):', file=sys.stderr)
    for command in entry['regenerate']:
        print('  '+command, file=sys.stderr)
    print('See the study.md method for the remaining cases and hardware requirements.', file=sys.stderr)


def prepare_archive(script):
    """Construct a disposable view without changing the archived validation math."""
    study = Path(script).resolve().parent
    key = study.relative_to(ROOT).as_posix()
    entry = catalog()[key]
    local = archive_root()
    missing = missing_payloads(study, entry, local)
    if missing:
        unavailable(study, entry, missing, local)
        raise SystemExit(2)
    # Preserve relative sibling imports and hashes. Small receipts/source stay in
    # Git; raw files are linked from the local archive, never copied back to docs.
    local.parent.mkdir(parents=True, exist_ok=True)
    temporary = tempfile.TemporaryDirectory(prefix='validate-', dir=local.parent)
    atexit.register(temporary.cleanup)
    view = Path(temporary.name)
    for base in (ROOT/'docs/journal', local/'docs/journal'):
        if not base.exists():
            continue
        owner = ROOT if base == ROOT/'docs/journal' else local
        for source in base.rglob('*'):
            if not source.is_file() or '__pycache__' in source.parts:
                continue
            target = view/source.relative_to(owner)
            if target.exists():
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            # Validators with --write or result exports must only write locally.
            if source.name in ('checks.json', 'validation-result.json'):
                shutil.copyfile(source, target)
            else:
                target.symlink_to(source.resolve())
    # Some old manifests hash their own validate.py. Preserve those exact bytes
    # and relative sibling imports rather than changing the historic receipts.
    for name, record in catalog().items():
        if 'validator' not in record:
            continue
        original = ROOT/name/record['validator']
        encoded = original.read_bytes()
        if hashlib.sha256(encoded).hexdigest() != record['validator_sha256']:
            raise ValueError(f'Frozen validator changed: {original}')
        target = view/name/'validate.py'
        target.unlink(missing_ok=True)
        target.write_bytes(encoded)
    print(f'Validating historical receipts using local archive {local}', flush=True)
    return view/study.relative_to(ROOT)


def relocate():
    """Move remaining raw payloads, retaining bytes and refusing overwrites."""
    local = archive_root()
    paths = [p for p in (ROOT/'docs/journal').rglob('*') if p.is_file() and raw_payload(p)]
    destinations = [(p, local/p.relative_to(ROOT)) for p in paths]
    if any(target.exists() for _, target in destinations):
        raise ValueError('Archive destination already exists; refusing to overwrite local evidence')
    total = 0
    for source, target in destinations:
        target.parent.mkdir(parents=True, exist_ok=True)
        total += source.stat().st_size
        shutil.move(str(source), target)
    print(f'Moved {len(paths)} raw files ({total} bytes) to {local}; no payloads deleted')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('action', choices=('list', 'relocate', 'validate'))
    p.add_argument('study', nargs='?')
    args = p.parse_args()
    if args.action == 'relocate':
        relocate()
    elif args.action == 'list':
        for name in catalog():
            print(name)
    else:
        if args.study not in catalog():
            p.error('Choose a study from the list command')
        raise SystemExit(subprocess.call([sys.executable, '-B', str(ROOT/args.study/'validate.py')]))


if __name__ == '__main__':
    main()
