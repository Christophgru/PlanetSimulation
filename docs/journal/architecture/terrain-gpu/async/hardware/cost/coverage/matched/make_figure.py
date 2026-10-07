#!/usr/bin/env python3
"""Plot declared matched-coverage gates and post hoc area sensitivity."""
import json
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
import numpy as np

ROOT=Path(__file__).resolve().parent
DATA=ROOT/'validation'
colors=['#d78b31','#277ea5','#8156a8','#238774']
fields=['eligible_visible_m2','visible_roots_per_eligible_m2','fade_per_eligible_m2','composed_projected_eligible_coverage']
labels=['Area: finite quad','Visible root density','Visible fade density','Composed coverage']
names=['sprint-25','walking-25','sprint-350','walking-350']
diagnostics=json.loads((DATA/'diagnostics.json').read_text())['coverage']
fig,axes=plt.subplots(2,2,figsize=(8.0,5.5),sharex=True,sharey=True)
for ax,name in zip(axes.flat,names):
    report=json.loads((DATA/'production'/name/'results.json').read_text())
    control=json.loads((DATA/'production'/name/'pair-1/cpu/coverage/1/matched/controls.json').read_text())
    common_distance=control['astronaut']['walked_m']
    for p in report['pairs']:
        for band,row in enumerate(p['bands']):
            for i,field in enumerate(fields):
                x=band+(i-1.5)*.16+(p['pair']-2)*.028
                ax.plot(x,row['compute_over_cpu'][field],'o',color=colors[i],ms=4.8,alpha=.9)
    for d in (d for d in diagnostics if d['case']==name):
        for band,row in enumerate(d['bands']):
            area=row['analytic_jacobian_eligible_m2'];ratio=area['compute']/area['cpu']
            x=band-1.5*.16+(d['pair']-2)*.028
            ax.plot(x,ratio,'o',mfc='white',mec=colors[0],ms=5.5,mew=1.2)
    ax.axhspan(.95,1.05,color='#edf2f5',zorder=-3)
    for value in (.95,1.05):ax.axhline(value,color='#8796a4',lw=.8,ls='--',zorder=-2)
    ax.axhline(1,color='#526576',lw=.8,zorder=-2)
    case,distance=name.split('-');passed=sum(p['coverage_parity'] for p in report['pairs'])
    ax.set_title(f'{case.capitalize()}, {distance} m crossing: {passed}/3 declared passes\nCommon root {common_distance:.3f} m',fontsize=10)
    ax.set_xticks(range(3),['0–5 m','5–15 m','15–30 m']);ax.set_xlim(-.5,2.5);ax.set_ylim(.93,1.07)
    ax.grid(axis='y',color='#dce3e8',lw=.5,zorder=-4)
    for spine in ('top','right'):ax.spines[spine].set_visible(False)
    ax.tick_params(labelsize=10)
for ax in axes[:,0]:ax.set_ylabel('Compute / CPU ratio',fontsize=10)
legend=[Line2D([],[],marker='o',ls='',color=c,label=l,ms=5) for c,l in zip(colors,labels)]
legend.append(Line2D([],[],marker='o',ls='',mfc='white',mec=colors[0],label='Area: analytic sensitivity',ms=5))
fig.legend(handles=legend,loc='upper center',ncol=3,frameon=False,fontsize=10,bbox_to_anchor=(.5,1.01))
fig.text(.5,.02,'Declared ±5% gate; analytic sensitivity is post hoc.',ha='center',fontsize=10,color='#566b7b')
fig.subplots_adjust(top=.85,bottom=.11,hspace=.37,wspace=.17)
for suffix in ('svg','png'):fig.savefig(ROOT/('coverage.'+suffix),dpi=160,bbox_inches='tight',metadata={'Date':None} if suffix=='svg' else {})
svg=ROOT/'coverage.svg'
svg.write_text('\n'.join(line.rstrip() for line in svg.read_text().splitlines())+'\n')
print('Generated matched coverage gate and area-sensitivity figure')
