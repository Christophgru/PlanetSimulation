# Local benchmark archives

Normal builds and tests use `tests/fixtures`, `tests/scenarios` and freshly
generated output in the build directory. They do not require historical traces
or compressed GPU maps. Source, small fixtures, screenshots used by the journal,
compact results and the original evidence manifests remain in Git.

New benchmark runs must use an ignored build directory, for example
`--output-dir build-benchmarks/runs/my-run`, or a directory outside the repository.
The active Python benchmark writers reject destinations in tracked source/docs
folders. Compression does not make raw data suitable for Git.

Historical raw data uses `build-benchmarks/archives/`, with paths relative to the
repository preserved underneath it, for example:

```text
build-benchmarks/archives/docs/journal/architecture/terrain-gpu/async/hardware/cost/preflight/validation/...
```

Existing local traces can be moved there without deleting them:

```sh
python3 -B scripts/benchmarks/archives/local.py relocate
```

This moves journal `.gz`/`.xz`, raw maps, JSONL and frame-trace CSV files and
refuses to overwrite an existing destination. Git will show deletions for raw
files that were previously tracked; commit those removals, not the local archive.
The original path/hash receipts remain as historical metadata. Their references
describe optional local artifacts, not files guaranteed to exist in a clone.
Removing files from the current tree does not remove them from older Git commits.
A shallow clone (`git clone --depth 1 ...`) avoids downloading that old history;
this cleanup does not rewrite or erase project history.

List the supported historical checks, then select one:

```sh
python3 -B scripts/benchmarks/archives/local.py list
python3 -B scripts/benchmarks/archives/local.py validate \
  docs/journal/architecture/terrain-gpu/async/hardware/cost/preflight
```

Calling that study's `validate.py` directly uses the same local archive. Set
`PLANET_BENCHMARK_ARCHIVE_ROOT` to use another ignored build directory or an
external directory containing the same relative tree.

Missing artifacts produce **UNAVAILABLE**, an explanation and a concrete
regeneration command, with exit code **2**. This is not a successful archive
validation. A complete archive runs the existing hash/numerical checks in a
temporary local view; failed checks remain failures. Exported check results stay
in that local view, and the committed historical summaries are not overwritten.
The temporary view is removed after the check; console output can be redirected
to a local log if needed.

`catalog.json` is the storage manifest for these optional checks. It identifies
the original evidence manifest and an entry command for a fresh run. Use each
study's method for additional cases, physical-GPU selection and prerequisites.
`YOUR_GPU_UUID` in commands must be replaced with the actual device UUID.
Fresh runs are new measurements; they cannot reconstruct old hashes simply by
reusing historical filenames. Do not rewrite historical receipts to make them
match new output. Frozen scripts under journal `validation/inputs` are historical
method snapshots, not the supported entry points for publishing new results.

For a checkout without any old benchmark data:

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --parallel 2
ctest --test-dir build -L core --output-on-failure
```

The `core` label covers headless unit, repository and archive-policy checks.
Rendering and native input checks generate their own local captures and remain
in the full CTest suite; run them under a working OpenGL display or software
OpenGL/Xvfb. Historical archive validators are not part of CTest.
