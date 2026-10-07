from pathlib import Path
import json
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
root=Path(__file__).resolve().parents[3]
reports=[json.loads((root/'validation/final'/case/'results.json').read_text()) for case in ('walking25','walking350','sprint25','sprint350')]
metrics=[('eligible_visible_m2','Eligible visible area'),('visible_roots_per_eligible_m2','Visible root density'),('fade_per_eligible_m2','Fade-weighted root density'),('composed_projected_eligible_coverage','Native displayed color coverage')]
fig,axes=plt.subplots(2,2,figsize=(10.5,6.7),sharex=True)
colors=['#245a9b','#ad5426','#35733e']
for ax,(key,title) in zip(axes.flat,metrics):
 ax.axhspan(.95,1.05,color='#e4eee8');ax.axhline(1,color='#56606a',linewidth=.8)
 for band in range(3):
  ys=[p['bands'][band]['compute_over_cpu'][key] for r in reports for p in r['pairs']]
  ax.plot(range(12),ys,'o-',color=colors[band],markersize=4.2,linewidth=1.1,label=['0–5 m','5–15 m','15–30 m'][band])
 for x in (2.5,5.5,8.5):ax.axvline(x,color='#bbc3cb',linewidth=.6)
 ax.set_title(title,fontsize=15);ax.set_ylabel('Compute / CPU',fontsize=12);ax.grid(axis='y',alpha=.2)
 ax.tick_params(labelsize=11.5);ax.spines[['top','right']].set_visible(False)
 ax.set_xticks(range(12), ['1', '2\nwalk 25 m', '3', '1', '2\nwalk 350 m', '3', '1', '2\nsprint 25 m', '3', '1', '2\nsprint 350 m', '3']); ax.tick_params(labelbottom=True)
 ax.set_xlabel('Alternating pair', fontsize=11.5)
axes[0,0].legend(fontsize=11.5,ncol=3,loc='best')
fig.suptitle('Fresh native Quadro coverage pairs; shaded region is the declared ±5% gate',fontsize=16)
fig.text(.5,.014,'Native plans and common controls retained. Color is a display-grid proxy with recorded edge uncertainty.',ha='center',fontsize=11.5)
fig.tight_layout(rect=(0,.055,1,.94), h_pad=2);fig.savefig(root/'coverage.svg')

p=root/'coverage.svg'
p.write_text('\n'.join(line.rstrip() for line in p.read_text().splitlines())+'\n')
