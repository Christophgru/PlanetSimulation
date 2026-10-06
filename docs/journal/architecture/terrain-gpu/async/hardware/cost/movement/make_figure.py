#!/usr/bin/env python3
"""Plot measured monotonic wall speeds; no animation-clock substitution."""
import json
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

root = Path(__file__).resolve().parent
plt.rcParams.update({'font.family': 'DejaVu Sans', 'font.size': 10, 'svg.hashsalt': 'grounded-native-speed'})
reports = {stage: json.loads((root / 'validation' / stage / 'results.json').read_text())
           for stage in ('before', 'after', 'slow')}
fig, axes = plt.subplots(1, 2, figsize=(9, 3.4), sharey=True)
for ax, case, expected in zip(axes, ('walking', 'sprint'), (6, 12)):
    for offset, backend, colour in ((-.18, 'cpu', '#28629a'), (.18, 'compute', '#177f89')):
        speeds = [next(r['wall_mps'] for r in reports[stage]['runs'] if r['case'] == case and r['backend'] == backend)
                  for stage in ('before', 'after', 'slow')]
        bars = ax.bar([i + offset for i in range(3)], speeds, width=.34, color=colour, label=backend)
        ax.bar_label(bars, fmt='%.2f', padding=3, fontsize=9)
    ax.axhline(expected, color='#647889', linestyle='--', linewidth=1, label='commanded speed')
    ax.set_xticks(range(3), ['Before fix', 'Fixed', 'Fixed + 250 ms\npresentation delay'])
    ax.set_title(f'{case.capitalize()} — target {expected} m/s')
    ax.set_ylim(0, 14)
    ax.spines[['top', 'right']].set_visible(False)
    ax.set_axisbelow(True)
    ax.yaxis.grid(True, color='#e0e5e8', linewidth=.7)
axes[0].set_ylabel('Actual distance / monotonic wall time (m/s)')
axes[1].legend(loc='upper left', fontsize=8, frameon=False)
fig.suptitle('Production Quadro native input: grounded clock prerequisite', fontsize=12)
fig.tight_layout()
fig.savefig(root / 'speed.svg', metadata={'Date': None})
svg = root / 'speed.svg'
svg.write_text('\n'.join(line.rstrip() for line in svg.read_text().splitlines()) + '\n')
fig.savefig(root / 'speed.png', dpi=170, metadata={'Software': 'Matplotlib'})
