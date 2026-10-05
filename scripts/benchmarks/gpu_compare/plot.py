#!/usr/bin/env python3
"""Plot paired GPU frame means and observed run ranges from runner evidence."""
import argparse
import json
import statistics
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('results', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()
report = json.loads(args.results.read_text())
assert report.get('validation') and len(report['runs']) == 12
plt.rcParams.update({'font.family': 'DejaVu Sans', 'font.size': 10, 'svg.hashsalt': 'gpu-comparison'})
fig, ax = plt.subplots(figsize=(7.6, 3.8), layout='constrained')
for offset, device, color in [(-.18, 0, '#647889'), (.18, 1, '#177f89')]:
    means, lower, upper = [], [], []
    for case in ['orbit', 'walking']:
        runs = [r for r in report['runs'] if r['device'] == device and r['case'] == case]
        assert len(runs) == 3
        values = [r['stats']['frame_ms']['mean'] for r in runs]
        mean = statistics.mean(values)
        means.append(mean)
        lower.append(mean - min(values))
        upper.append(max(values) - mean)
    bars = ax.bar([offset, 1 + offset], means, width=.34,
                  yerr=[lower, upper], capsize=4, color=color,
                  label=runs[0]['gpu']['name'].removeprefix('NVIDIA GeForce '))
    ax.bar_label(bars, labels=[f'{m:.2f} ms' for m in means], padding=5, fontsize=10)
ax.set_xticks([0, 1], ['Fixed camera, orbital motion', 'Walking, orbital motion'])
ax.set_ylabel('Mean frame wall time (ms; lower is faster)')
ax.set_ylim(0, 98)
ax.spines[['top', 'right']].set_visible(False)
ax.set_axisbelow(True)
ax.grid(axis='y', alpha=.18)
ax.legend(frameon=False, loc='upper right')
fig.suptitle('Matched 1280 × 720 offscreen renderer workload', fontsize=13)
fig.text(.5, -.025, 'Three runs per GPU and case; whiskers show min–max run means, not confidence intervals.',
         ha='center', fontsize=8)
args.output.parent.mkdir(parents=True, exist_ok=True)
fig.savefig(args.output, bbox_inches='tight', metadata={'Date': None})
if args.output.suffix.lower() == '.svg':
    args.output.write_text('\n'.join(line.rstrip() for line in args.output.read_text().splitlines()) + '\n')
plt.close(fig)
