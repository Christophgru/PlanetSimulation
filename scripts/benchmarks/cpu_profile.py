#!/usr/bin/env python3
"""Collect native wall/thread-CPU traces and an independent Callgrind study.

Run under Xvfb or a real OpenGL display. All workloads run sequentially.
Callgrind instruction counts are never reported as execution milliseconds.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import resource
import shutil
import subprocess
import sys
import time
sys.dont_write_bytecode = True
from cpu_report import write_report

ROOT = Path(__file__).resolve().parents[2]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--foliage-benchmark', type=Path, required=True)
    parser.add_argument('--replay', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--frames', type=int, default=14)
    args = parser.parse_args()
    if args.frames < 8:
        parser.error('At least eight frames required')
    if not shutil.which('valgrind'):
        parser.error('Install the optional valgrind package for the function study')
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    replay = args.replay.resolve()
    binary, bench = args.binary.resolve(), args.foliage_benchmark.resolve()
    metadata = dict(generated_utc=datetime.now(timezone.utc).isoformat(), platform=platform.platform(), source_revision=subprocess.check_output(
        ['git', '-c', f'safe.directory={ROOT}', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        source_note='Working tree with CPU trace additions; see committed source.',
        binary_sha256=digest(binary), benchmark_sha256=digest(bench), input_sha256=digest(replay),
        input=str(replay.relative_to(ROOT)), frames=args.frames, warmup=3,
        environment={key: os.environ.get(key) for key in ('LIBGL_ALWAYS_SOFTWARE', 'LP_NUM_THREADS')},
        source_sha256={str(p.relative_to(ROOT)): digest(p) for p in sorted((ROOT/'src').rglob('*')) if p.is_file()},
        shader_sha256={str(p.relative_to(ROOT)): digest(p) for p in sorted((ROOT/'shaders').rglob('*')) if p.is_file()},
        runs=[])

    def run(command, log, label):
        before = resource.getrusage(resource.RUSAGE_CHILDREN)
        start = time.monotonic()
        with log.open('w') as stream:
            subprocess.run(list(map(str, command)), cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT, check=True)
        after = resource.getrusage(resource.RUSAGE_CHILDREN)
        metadata['runs'].append(dict(label=label, command=list(map(str, command)),
            elapsed_s=time.monotonic()-start,
            process_cpu_s=after.ru_utime+after.ru_stime-before.ru_utime-before.ru_stime))
        (out/'manifest.json').write_text(json.dumps(metadata, indent=2)+'\n')
        print(f'Completed {label}', flush=True)

    for case in ('stationary', 'walking', 'walking-bare', 'walking-untraced'):
        folder = out/case
        folder.mkdir(exist_ok=True)
        source = replay
        if case == 'walking-bare':
            document = json.loads(replay.read_text())
            for planet in document['scenario']['planets']:
                planet.setdefault('foliage', {})['enabled'] = False
            source = folder/'input.json'
            source.write_text(json.dumps(document, indent=2)+'\n')
        command = [binary, '--replay', source, '--surface-capture', folder/'frame.png',
                   '--render-size', '640', '360', '--benchmark-frames', str(args.frames),
                   '--benchmark-step', '0', '--benchmark-walk-step', '0' if case == 'stationary' else '2',
                   '--performance-trace', folder/'frames.csv']
        if case != 'walking-untraced':
            command += ['--cpu-trace', folder/'trace.json']
        run(command, folder/'run.log', case)
        if case != 'walking-untraced':
            document = json.loads((folder/'trace.json').read_text())
            write_report(document, folder/'report', 3, args.frames-4)
            if case == 'walking':
                write_report(document, folder/'detail', 5, 3)
    metadata['traced_untraced_image_equal'] = digest(out/'walking/frame.png') == digest(out/'walking-untraced/frame.png')
    if not metadata['traced_untraced_image_equal']:
        raise RuntimeError('CPU instrumentation changed the frozen walking image')

    folder = out/'callgrind'
    folder.mkdir(exist_ok=True)
    metadata['callgrind_version'] = subprocess.check_output(['valgrind', '--version'], text=True).strip()
    run([bench, replay], folder/'native.csv', 'native-foliage')
    run(['valgrind', '--tool=callgrind', '--cache-sim=no', '--branch-sim=no',
         f'--callgrind-out-file={folder}/callgrind.out', bench, replay], folder/'run.log', 'callgrind-foliage')
    for label, options in [('self', ['--inclusive=no']), ('inclusive', ['--inclusive=yes']),
                           ('callers', ['--inclusive=no', '--tree=both'])]:
        run(['callgrind_annotate', '--auto=no', '--threshold=99', *options, folder/'callgrind.out'],
            folder/f'{label}.txt', f'callgrind-{label}')
    metadata['artifact_sha256'] = {str(p.relative_to(out)): digest(p) for p in sorted(out.rglob('*'))
                                   if p.is_file() and p.name != 'manifest.json'}
    (out/'manifest.json').write_text(json.dumps(metadata, indent=2)+'\n')


if __name__ == '__main__':
    main()
