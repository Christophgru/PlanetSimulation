#!/usr/bin/env python3
"""Plot all declared stationary pair ratios from reconstructed raw receipts."""
import json
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

HERE = Path(__file__).resolve().parent
result = json.loads((HERE / 'validation-result.json').read_text())
plt.rcParams.update({'font.size': 10, 'svg.fonttype': 'none'})
figure, axes = plt.subplots(1, 3, figsize=(10.4, 3.8), sharey=True)
controls = [('full', 'Full draws', '#1467a1', 'o', -.2),
            ('discard', 'Fragment discard', '#368a63', 's', 0),
            ('suppress', 'Suppressed draws', '#765998', '^', .2)]
for axis, wind, title in zip(axes, ['native', 'fixed', 'indexed'],
                              ['Native wind', 'Fixed wind (12 s)', 'Matched evolving wind']):
    axis.axhspan(.8, 1.05, color='#eaf3ee', zorder=0)
    axis.axhline(1.05, color='#b74739', linestyle='--', linewidth=1.3)
    axis.axhline(1, color='#73818b', linewidth=.7)
    for mode, label, color, marker, offset in controls:
        points = [f for f in result['findings'] if f['wind'] == wind and f['raster'] == mode]
        assert [f['pair'] for f in points] == [1, 2, 3]
        axis.scatter([f['pair'] + offset for f in points],
                     [f['native_p95_ratio'] for f in points],
                     label=label, color=color, marker=marker, s=45, zorder=3)
    axis.set(title=title, xlabel='Alternating pair', xticks=[1, 2, 3],
             xlim=(.5, 3.5), ylim=(.8, 1.18))
    axis.grid(axis='y', color='#d5dce1', linewidth=.5)
    axis.spines[['top', 'right']].set_visible(False)
axes[0].set_ylabel('Compute / CPU native frame p95')
axes[2].text(3.42, 1.055, '1.05 limit', ha='right', va='bottom', color='#b74739', fontsize=9)
figure.legend(*axes[0].get_legend_handles_labels(), loc='upper center', ncol=3, frameon=False)
figure.text(.5, .025, 'Quadro M1000M · 240 measured frames per run · all outliers retained · ablations diagnose cost only',
            ha='center', fontsize=9)
figure.subplots_adjust(left=.085, right=.985, bottom=.19, top=.83, wspace=.12)
figure.savefig(HERE / 'raster.svg', metadata={'Date': None})
figure.savefig(Path('/workspace/build-resume/raster-review.png'), dpi=160)
# Matplotlib SVG indentation otherwise leaves whitespace-only lines.
path = HERE / 'raster.svg'
path.write_text('\n'.join(line.rstrip() for line in path.read_text().splitlines()) + '\n')
