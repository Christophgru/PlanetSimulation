#!/usr/bin/env python3
"""Standalone figure from archived preflight receipts; no new benchmark runs."""
import json
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

plt.rcParams.update({'font.family': 'DejaVu Sans', 'svg.hashsalt': 'terrain-cost-preflight'})

root = Path(__file__).resolve().parent
report = json.loads((root / 'validation/results.json').read_text())
fig, axes = plt.subplots(1, 2, figsize=(10, 3.9), layout='constrained')
colors = {'cpu': '#23677a', 'compute': '#ba643c'}
for backend, offset in [('cpu', -.11), ('compute', .11)]:
    runs = sorted((r for r in report['runs'] if r['backend'] == backend), key=lambda r: r['pair'])
    x = [r['pair'] + offset for r in runs]
    for ax, y in [(axes[0], [r['stats']['wall_ms']['p95'] for r in runs]),
                  (axes[1], [r['sampled_device_used_peak_bytes'] / 2**20 for r in runs])]:
        ax.bar(x, y, width=.2, color=colors[backend], label=backend)
        for a, b in zip(x, y):
            ax.text(a, b + 1, f'{b:.1f}', ha='center', fontsize=8)
for ax in axes:
    ax.set_xticks([1, 2, 3], ['Pair 1', 'Pair 2', 'Pair 3'])
    ax.spines[['top', 'right']].set_visible(False)
    ax.grid(axis='y', alpha=.2)
    ax.set_axisbelow(True)
axes[0].set(ylabel='Complete native-loop p95 (ms)', ylim=(0, 95), title='Stationary wall time, observer included')
axes[0].legend(frameon=False, ncols=2, loc='upper left')
axes[1].set(ylabel='Sampled device-wide used peak (MiB)', ylim=(0, 620), title='Periodic samples can miss memory peaks')
fig.suptitle('Quadro M1000M · production 1280×720 · 240 measured frames/run', fontsize=12)
fig.supxlabel('100k Earth triangles · 1.999M candidates · orbit paused, wind active · preflight only', fontsize=9)
for extension in ['svg', 'png']:
    output = root / f'stationary.{extension}'
    fig.savefig(output, dpi=180,
                metadata={'Date': None} if extension == 'svg' else None)
    if extension == 'svg':
        output.write_text('\n'.join(line.rstrip() for line in output.read_text().splitlines()) + '\n')
