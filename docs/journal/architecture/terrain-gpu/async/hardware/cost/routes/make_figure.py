#!/usr/bin/env python3
"""Export native route p95 costs; coverage acceptance remains separate."""
import json
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

root = Path(__file__).resolve().parent
report = json.loads((root / 'validation/results.json').read_text())
plt.rcParams.update({'font.family': 'DejaVu Sans', 'font.size': 10, 'svg.hashsalt': 'native-distance-route-cost'})
fig, axes = plt.subplots(1, 2, figsize=(9, 3.6), sharey=True)
for ax, case in zip(axes, ('walking', 'sprint')):
    for offset, backend, colour in ((-.18, 'cpu', '#28629a'), (.18, 'compute', '#177f89')):
        values = [next(r['stats']['wall_ms']['p95'] for r in report['runs']
                       if r['case'] == case and r['pair'] == pair and r['backend'] == backend)
                  for pair in range(1, 4)]
        bars = ax.bar([pair + offset for pair in range(1, 4)], values, width=.34, color=colour, label=backend)
        ax.bar_label(bars, fmt='%.1f', padding=3, fontsize=9)
    ax.set_xticks(range(1, 4), ['Pair 1', 'Pair 2', 'Pair 3'])
    ax.set_title(f'{case.capitalize()} — {6 if case == "walking" else 12} m/s command')
    ax.spines[['top', 'right']].set_visible(False)
    ax.set_axisbelow(True)
    ax.yaxis.grid(True, color='#e0e5e8', linewidth=.7)
    ax.set_ylim(0, ax.get_ylim()[1] * 1.15)
axes[0].set_ylabel('Complete native loop p95 (ms)')
axes[1].legend(frameon=False, loc='upper right')
fig.suptitle('Production Quadro routes: common [5,400) m interval', fontsize=12)
fig.text(.5, .015, 'Observer and every measured spike included; rendered near-root coverage remains pending', ha='center', fontsize=9)
fig.tight_layout(rect=(0, .055, 1, 1))
fig.savefig(root / 'routes.svg', metadata={'Date': None})
svg = root / 'routes.svg'
svg.write_text('\n'.join(line.rstrip() for line in svg.read_text().splitlines()) + '\n')
fig.savefig(root / 'routes.png', dpi=170, metadata={'Software': 'Matplotlib'})
