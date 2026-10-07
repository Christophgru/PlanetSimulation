#!/usr/bin/env python3
"""Show illustrative opaque grass masks; common-view coverage is still pending."""
import json
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

root = Path(__file__).resolve().parent
plt.rcParams.update({'font.family': 'DejaVu Sans', 'font.size': 10, 'svg.hashsalt': 'native-grass-inspection'})
fig, axes = plt.subplots(1, 2, figsize=(9, 3.7))
for ax, backend in zip(axes, ('cpu', 'compute')):
    folder = root / 'validation/production' / backend / 'coverage/1'
    receipt = json.loads((folder / 'snapshot.json').read_text())
    analysis = json.loads((folder / 'analysis.json').read_text())
    ax.imshow(plt.imread(folder / 'grass-mask.pgm'), cmap='gray', vmin=0, vmax=255)
    ax.set_title(f'{backend.upper()}: {receipt["walked_m"]:.3f} m, wind {receipt["wind_s"]:.3f} s')
    ax.set_xlabel(f'{analysis["opaque_grass_pixels"]:,} opaque grass pixels\n'
                  f'0–5 / 5–15 / 15–30 m roots: {" / ".join(str(x) for x in analysis["root_band_counts"])}', fontsize=9)
    ax.set_xticks([]);ax.set_yticks([])
fig.suptitle('Live production sprint inspection: grass depth-change masks', fontsize=12)
fig.text(.5, .025, 'Crossing poses, wind and trails differ; eligible ground area and final compositing are not measured', ha='center', fontsize=9)
fig.tight_layout(rect=(0, .055, 1, 1))
fig.savefig(root / 'masks.svg', metadata={'Date': None})
p = root / 'masks.svg';p.write_text('\n'.join(line.rstrip() for line in p.read_text().splitlines()) + '\n')
fig.savefig(root / 'masks.png', dpi=170, metadata={'Software': 'Matplotlib'})
