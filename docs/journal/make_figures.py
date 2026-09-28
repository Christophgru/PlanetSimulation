#!/usr/bin/env python3
"""Generate deterministic, dependency-free SVG diagrams for paper.typ.

These are explanatory schematics, not screenshots or performance measurements.
The only empirical bars use the three historical numbers quoted in README.md.
"""
from html import escape
from math import cos, sin, pi, exp, sqrt, log
from pathlib import Path

ROOT = Path(__file__).resolve().parent
NAVY = '#17334b'
TEAL = '#177f89'
BLUE = '#4978b7'
GOLD = '#df982c'
CORAL = '#cb6269'
GREEN = '#559b76'
GRAY = '#647889'
PALE = '#f2f7f8'
LITE = '#e2ebee'
WHITE = '#ffffff'

class SVG:
    def __init__(self, width=1000, height=430, title=''):
        self.w, self.h = width, height
        self.parts = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" width="{width}" height="{height}">',
            '<defs><marker id="arrow" markerWidth="9" markerHeight="9" refX="7" refY="4.5" orient="auto"><path d="M0,0 L9,4.5 L0,9 Z" fill="#647889"/></marker></defs>',
            f'<rect width="{width}" height="{height}" fill="{WHITE}"/>']
        if title:
            self.text(26, 35, title, 22, NAVY, bold=True)
            self.line(26, 51, width-26, 51, LITE, 2)
    def raw(self, value): self.parts.append(value)
    def line(self,x1,y1,x2,y2,color=GRAY,width=2,dash=None,arrow=False):
        attrs = f' stroke="{color}" stroke-width="{width}" stroke-linecap="round"'
        if dash: attrs += f' stroke-dasharray="{dash}"'
        if arrow: attrs += ' marker-end="url(#arrow)"'
        self.raw(f'<line x1="{x1:.2f}" y1="{y1:.2f}" x2="{x2:.2f}" y2="{y2:.2f}"{attrs}/>')
    def rect(self,x,y,w,h,fill=PALE,stroke=None,r=10,sw=2):
        st = f' stroke="{stroke}" stroke-width="{sw}"' if stroke else ''
        self.raw(f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h:.2f}" rx="{r}" fill="{fill}"{st}/>')
    def circle(self,x,y,r,fill=WHITE,stroke=None,sw=2,dash=None):
        st = f' stroke="{stroke}" stroke-width="{sw}"' if stroke else ''
        ds = f' stroke-dasharray="{dash}"' if dash else ''
        self.raw(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="{r:.2f}" fill="{fill}"{st}{ds}/>')
    def ellipse(self,x,y,rx,ry,fill='none',stroke=GRAY,sw=2,dash=None):
        ds = f' stroke-dasharray="{dash}"' if dash else ''
        self.raw(f'<ellipse cx="{x:.2f}" cy="{y:.2f}" rx="{rx:.2f}" ry="{ry:.2f}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}"{ds}/>')
    def path(self,d,stroke=GRAY,sw=2,fill='none',dash=None,arrow=False):
        ds = f' stroke-dasharray="{dash}"' if dash else ''
        ar = ' marker-end="url(#arrow)"' if arrow else ''
        self.raw(f'<path d="{d}" stroke="{stroke}" stroke-width="{sw}" fill="{fill}" stroke-linecap="round" stroke-linejoin="round"{ds}{ar}/>')
    def text(self,x,y,s,size=16,color=NAVY,bold=False,anchor='start',italic=False):
        weight = ' font-weight="700"' if bold else ''
        sty = ' font-style="italic"' if italic else ''
        self.raw(f'<text x="{x:.2f}" y="{y:.2f}" text-anchor="{anchor}" font-family="DejaVu Sans, sans-serif" font-size="{size}" fill="{color}"{weight}{sty}>{escape(str(s))}</text>')
    def save(self,path):
        full = ROOT / path
        full.parent.mkdir(parents=True,exist_ok=True)
        full.write_text('\n'.join(self.parts+['</svg>'])+'\n')


def timeline():
    s=SVG(1000,420,'A chronological reconstruction from selected commits')
    milestones=[
        (90,125,'09 Sep','Config + GL','bd73ba9',BLUE),
        (235,270,'11 Sep','Shader + PNG','94b42f6',TEAL),
        (385,125,'15 Sep','Terrain + camera','b3975d6',GREEN),
        (540,270,'24 Sep','Orbits + lighting','18ffbad',GOLD),
        (690,125,'26 Sep','Atmosphere','0e48155',CORAL),
        (855,270,'28 Sep','Diagnostics + fixes','ab59069…',NAVY),
    ]
    s.line(70,205,925,205,NAVY,4,arrow=True)
    for x,y,date,cap,commit,col in milestones:
        s.circle(x,205,9,col,WHITE,2)
        s.line(x,205,x, y+15 if y>205 else y+25,col,2)
        yy=y if y<205 else y+18
        s.text(x,yy,date,15,col,True,'middle')
        s.text(x,yy+22,cap,14,NAVY,True,'middle')
        s.text(x,yy+42,commit,12,GRAY,False,'middle')
    s.text(500,389,'Chronology follows code milestones; each point anchors a group of commits, not an isolated invention.',12,GRAY,anchor='middle')
    s.save('figures/early/history.svg')


def transforms():
    s=SVG(1000,390,'From a configured body to a framebuffer pixel')
    boxes=[(42,'JSON scene','body / orbit / camera'),(257,'Simulation','P(t), R(t), light'),(472,'Geometry','terrain / water / Sun'),(687,'GPU passes','P · V · M · x')]
    for x,head,sub in boxes:
        s.rect(x,108,182,112,PALE,stroke=LITE)
        s.text(x+91,145,head,18,NAVY,True,'middle')
        s.text(x+91,176,sub,13,GRAY,False,'middle')
    for x in [232,447,662]:s.line(x,164,x+18,164,GRAY,3,arrow=True)
    s.rect(295,265,405,69,WHITE,TEAL,8)
    s.text(497,291,'clip = P · V · M · local',15,TEAL,True,'middle')
    s.text(497,318,'column vectors; divide by w before viewport',11,GRAY,False,'middle')
    s.line(779,220,779,272,GRAY,2,arrow=True)
    s.text(845,304,'HDR → exposure → sRGB',13,NAVY,True,'middle')
    s.save('figures/early/transforms.svg')


def terrain():
    s=SVG(1000,440,'Stable height field and camera-dependent tessellation')
    s.circle(265,252,124,'#ecf2f0',TEAL,2)
    s.path('M 145 250 Q 184 163 214 177 Q 238 127 266 177 Q 291 144 325 190 Q 373 203 383 262',TEAL,4)
    s.circle(265,252,5,NAVY)
    s.line(265,252,265,109,GOLD,2,arrow=True)
    s.text(279,116,'r + h(u)',15,GOLD,True)
    s.circle(285,82,16,CORAL)
    s.text(313,86,'eye',16,CORAL,True)
    s.path('M 391 204 Q 508 130 598 214',GRAY,2,dash='7 6')
    s.rect(570,89,373,255,PALE,LITE)
    s.text(593,122,'angular distance from eye',17,NAVY,True)
    rows=[('near',153,16,TEAL),('middle',214,8,BLUE),('far',275,3,GRAY)]
    for label,y,n,col in rows:
        s.text(593,y+11,label,14,col,True)
        for i in range(n+1):
            x=687+i*231/n
            s.line(x,y-7,x,y+16,col,1)
        s.line(687,y+16,918,y+16,col,2)
    s.text(593,329,'shared edge samples prevent cracks',12,GRAY)
    s.text(265,407,'h(u) is fixed in planet-local coordinates; only mesh density follows the camera.',14,NAVY,anchor='middle')
    s.save('figures/early/terrain.svg')


def pole():
    s=SVG(1000,395,'Great-circle camera transport across a pole')
    s.circle(292,226,125,'#f2f6f8',TEAL,2)
    s.circle(292,101,7,CORAL)
    s.text(309,105,'north pole',15,CORAL,True)
    s.path('M 195 151 Q 247 87 292 101 Q 337 116 382 158',GOLD,4,arrow=True)
    for x,y,dx,dy in [(211,138,30,-21),(350,138,31,22)]:
        s.circle(x,y,6,NAVY)
        s.line(x,y,x+dx,y+dy,TEAL,3,arrow=True)
    s.text(161,141,'old tangent',13,TEAL)
    s.text(386,141,'transported tangent',13,TEAL)
    s.rect(520,96,430,229,PALE,LITE)
    s.text(546,133,'transport the camera basis',17,NAVY,True)
    s.text(546,173,'axis = normalize(radial × tangent)',15,TEAL)
    s.text(546,207,'angle = walk distance / radius',15,TEAL)
    s.text(546,241,'view′ = rotation(axis, angle) · view',15,TEAL)
    s.text(546,290,'avoids the longitude-frame flip at the pole',13,GRAY)
    s.save('figures/early/pole.svg')


def kepler():
    s=SVG(1000,445,'Kepler ellipse and hierarchical barycentric recoil')
    s.ellipse(275,233,205,126,'none',BLUE,3)
    focus=275-sqrt(205**2-126**2)
    s.circle(focus,233,15,GOLD)
    s.circle(440,159,11,TEAL)
    s.line(275,233,480,233,GRAY,2)
    s.line(275,233,275,107,GRAY,2)
    s.text(372,225,'a',16,GRAY,True)
    s.text(283,171,'b',16,GRAY,True)
    s.line(focus,233,440,159,GOLD,2,arrow=True)
    s.text(251,169,'r(t)',16,GOLD,True)
    s.text(focus,263,'parent focus',13,GOLD,True,'middle')
    s.text(439,137,'child collective',13,TEAL,True,'middle')
    s.rect(535,88,426,263,PALE,LITE)
    s.text(556,121,'Earth–Moon subtree',17,NAVY,True)
    s.line(584,224,901,224,GRAY,2)
    s.circle(733,224,7,NAVY)
    s.circle(673,224,21,BLUE)
    s.circle(861,224,11,GRAY)
    s.text(733,199,'barycenter',13,NAVY,True,'middle')
    s.text(673,265,'Earth',14,BLUE,True,'middle')
    s.text(861,265,'Moon',14,GRAY,True,'middle')
    s.text(556,314,'m₁ r₁ + m₂ r₂ = 0 around the collective center',13,GRAY)
    s.text(40,402,'Solve E − e sin E = M(t); then rotate the orbital-plane ellipse into world space.',15,NAVY)
    s.save('figures/dynamics/kepler.svg')


def orbit_trails():
    s=SVG(1000,420,'Ten future moon revolutions inherit parent translation')
    s.ellipse(295,228,220,131,'none',BLUE,2,dash='8 6')
    s.circle(140,228,34,GOLD)
    s.text(140,286,'Sun',14,GOLD,True,'middle')
    points=[]
    for i in range(1100):
        t=2*pi*i/1099
        ex=295+220*cos(0.55*t+0.1)
        ey=228+131*sin(0.55*t+0.1)
        x=ex+27*cos(10*t+1.0)
        y=ey+27*sin(10*t+1.0)
        points.append(f'{x:.1f},{y:.1f}')
    s.raw(f'<polyline points="{" ".join(points)}" fill="none" stroke="{CORAL}" stroke-width="2.1" opacity="0.9"/>')
    s.circle(295+220*cos(.1),228+131*sin(.1),13,BLUE)
    s.text(549,188,'Earth',14,BLUE,True)
    s.rect(608,100,352,211,PALE,LITE)
    s.text(631,137,'sampling rule',17,NAVY,True)
    s.text(631,177,'tₖ = t₀ + k · Tmoon / 32',15,TEAL)
    s.text(631,207,'k = 0 … 320',15,TEAL)
    s.text(631,248,'position = full hierarchy at tₖ',14,NAVY)
    s.text(631,280,'red curve: illustrative inertial trail',12,GRAY)
    s.save('figures/dynamics/orbit-trails.svg')


def lighting():
    s=SVG(1000,430,'Direct illumination, reflected moonlight and terrain visibility')
    s.circle(123,214,61,GOLD)
    s.circle(715,249,105,BLUE)
    s.circle(474,100,34,GRAY)
    s.text(123,301,'Sun',17,GOLD,True,'middle')
    s.text(715,380,'planet',17,BLUE,True,'middle')
    s.text(474,55,'moon',16,GRAY,True,'middle')
    s.line(184,205,596,226,GOLD,4,arrow=True)
    s.line(174,180,440,107,GOLD,3,arrow=True)
    s.line(494,125,648,189,TEAL,3,arrow=True)
    s.text(393,181,'inverse-square direct flux',14,GOLD,True,'middle')
    s.text(547,139,'phase × albedo',13,TEAL,True)
    s.path('M 639 306 Q 665 268 689 292 L 709 275 L 725 295',NAVY,3)
    s.path('M 615 324 L 659 294 L 708 294 L 669 334 Z',fill='#dae0e8',stroke='none')
    s.text(868,233,'shadow-map',14,NAVY,True)
    s.text(868,256,'comparison',14,NAVY)
    s.line(799,263,854,247,NAVY,2,arrow=True)
    s.text(45,404,'The same terrain depth map gates opaque shading, water reflection and direct atmospheric scattering.',13,GRAY)
    s.save('figures/dynamics/lighting.svg')


def ocean():
    s=SVG(1000,410,'Why a geometric reflection must be gated on the night side')
    s.circle(487,238,142,'#dfeaf0',BLUE,2)
    s.path('M 487 96 A 142 142 0 0 1 487 380 L 487 96',fill='#b9cbd5',stroke='none')
    s.circle(91,171,45,GOLD)
    s.line(145,177,322,196,GOLD,4,arrow=True)
    s.line(185,275,332,275,GRAY,2)
    s.text(257,301,'Sun-facing',14,GOLD,True,'middle')
    s.text(648,286,'night side',14,NAVY,True,'middle')
    s.circle(579,176,11,CORAL)
    s.line(579,176,673,112,CORAL,2,dash='6 5')
    s.text(697,112,'invalid bright reflected sample',13,CORAL,True)
    s.rect(728,171,236,162,PALE,LITE)
    s.text(748,205,'water radiance',16,NAVY,True)
    s.text(748,240,'indirect + Vsun × R',15,TEAL)
    s.text(748,273,'Vsun = local incidence',13,GRAY)
    s.text(748,297,'× terrain visibility',13,GRAY)
    s.save('figures/dynamics/ocean.svg')


def atmosphere():
    s=SVG(1000,450,'Atmospheric density, curved sightline and scattering')
    s.circle(246,376,177,'#e8eff2',BLUE,2)
    s.circle(246,376,213,'none',TEAL,2,dash='7 5')
    s.circle(313,179,9,NAVY)
    s.text(328,178,'eye',15,NAVY,True)
    s.line(313,179,595,170,GRAY,2,dash='7 6')
    s.path('M 313 179 C 384 193 460 211 582 170',CORAL,4,arrow=True)
    s.text(465,141,'straight raster ray',13,GRAY)
    s.text(452,237,'bent view ray',14,CORAL,True)
    s.line(410,205,535,292,GOLD,3,arrow=True)
    s.text(560,307,'scattered solar light',13,GOLD,True)
    s.rect(639,92,325,278,PALE,LITE)
    s.text(663,128,'radial model',17,NAVY,True)
    s.text(663,165,'ρ(h) ≈ exp(−h / H)',16,TEAL)
    s.text(663,201,'n(h) − 1 ∝ (P / T) · ρ(h)',15,TEAL)
    s.text(663,237,'d direction / ds = ∇n⊥ / n',14,TEAL)
    s.text(663,283,'48 view steps when bending;',13,GRAY)
    s.text(663,307,'24 if refraction is disabled',13,GRAY)
    s.text(663,345,'distant image is reprojected',13,GRAY)
    s.save('figures/atmosphere/refraction.svg')


def atmosphere_pipeline():
    s=SVG(1000,430,'Atmospheric composition: reusable columns and depth-aware fields')
    nodes=[(29,108,'Opaque HDR','full resolution',BLUE),(222,108,'Density columns','shell-dependent',TEAL),(415,108,'Scattering field','quarter resolution',GREEN),(608,108,'Depth-aware resolve','full resolution',GOLD),(801,108,'Exposure + sRGB','full resolution',CORAL)]
    for x,y,title,sub,col in nodes:
        s.rect(x,y,168,113,PALE,col,10)
        s.text(x+84,148,title,12,col,True,'middle')
        s.text(x+84,179,sub,11,GRAY,False,'middle')
    for x in [204,397,590,783]:s.line(x,164,x+13,164,GRAY,2,arrow=True)
    s.rect(265,292,470,84,WHITE,LITE,8)
    s.text(500,322,'terrain depth + body IDs constrain reprojected sky',16,NAVY,True,'middle')
    s.text(500,350,'edges with no matching coarse sample are integrated at full resolution',12,GRAY,False,'middle')
    s.line(110,221,320,292,GRAY,2,arrow=True)
    s.line(710,292,694,221,GRAY,2,arrow=True)
    s.save('figures/atmosphere/pipeline.svg')


def performance():
    s=SVG(1000,455,'Historical Quadro M1000M benchmark reported in the project README')
    names=['initial','column LUT','reduced fields']
    values=[208,138,26]
    colors=[CORAL,GOLD,TEAL]
    s.line(104,360,878,360,NAVY,2)
    for i,(name,val,col) in enumerate(zip(names,values,colors)):
        x=165+i*265
        h=val*1.05
        s.rect(x,360-h,136,h,col,None,4)
        s.text(x+68,360-h-12,f'{val} ms',19,col,True,'middle')
        s.text(x+68,390,name,14,NAVY,True,'middle')
    s.text(878,339,'0',12,GRAY)
    s.text(590,97,'mean frame time',16,NAVY,True)
    s.text(590,122,'1280 × 720, 90 frames',12,GRAY)
    s.text(590,147,'Debug CPU build; single scene and GPU',12,GRAY)
    s.text(504,430,'Bars reproduce historical reported values, not a new measurement.',12,GRAY,anchor='middle')
    s.save('figures/atmosphere/performance.svg')


def adaptive():
    s=SVG(1000,425,'Best-effort quality control: timing feedback and memory cap')
    s.rect(47,101,400,254,PALE,LITE)
    s.text(69,134,'feedback law',18,NAVY,True)
    s.text(69,171,'mₜ = 0.85 mₜ₋₁ + 0.15 frame-ms',15,TEAL)
    s.text(69,206,'mₜ > 55 ms → lower scale',15,CORAL)
    s.text(69,241,'mₜ < 32 ms → raise scale',15,GREEN)
    s.text(69,276,'1.5 s cooldown; moving scene only',13,GRAY)
    s.text(69,317,'20 FPS remains a target, not a guarantee',13,NAVY)
    s.rect(492,101,462,254,WHITE,LITE)
    s.text(516,134,'discrete scene scales',18,NAVY,True)
    scales=[1,.8,.65,.5,.35,.25]
    for i,v in enumerate(scales):
        x=525+i*68
        s.rect(x,320-75*v,48,75*v,TEAL if i==0 else BLUE,None,3)
        s.text(x+24,340,f'{int(v*100)}%',12,NAVY,True,'middle')
    s.text(516,177,'memory estimate ∝ width × height × scale²',13,GRAY)
    s.text(516,199,'reserve 75% of reported graphics memory',13,GRAY)
    s.save('figures/interaction/adaptive.svg')


def transition():
    s=SVG(1000,430,'One wall-clock second from orbital pose to surface pose')
    s.circle(259,360,154,'#e8eff2',BLUE,2)
    s.circle(259,360,178,'none',TEAL,2,dash='6 5')
    s.circle(200,80,9,CORAL)
    s.circle(376,244,9,TEAL)
    s.path('M 200 80 C 233 117 296 158 350 209 Q 370 230 376 244',CORAL,4,arrow=True)
    s.text(128,74,'orbit eye',14,CORAL,True)
    s.text(388,240,'surface eye',14,TEAL,True)
    s.rect(531,90,414,275,PALE,LITE)
    s.text(552,124,'parameter u = clamp(Δwall-time / 1 s)',15,NAVY,True)
    s.text(552,156,'ease(u) = 3u² − 2u³',15,TEAL)
    s.line(574,326,907,326,GRAY,2)
    s.line(574,326,574,187,GRAY,2)
    pts=[]
    for i in range(101):
        u=i/100
        x=574+333*u
        y=326-139*(3*u*u-2*u*u*u)
        pts.append(f'{x:.1f},{y:.1f}')
    s.raw(f'<polyline points="{" ".join(pts)}" fill="none" stroke="{TEAL}" stroke-width="3"/>')
    s.text(574,348,'0',12,GRAY,'middle'); s.text(907,348,'1 s',12,GRAY,'middle')
    s.text(740,204,'position: spherical radial + log radius',12,GRAY,'middle')
    s.save('figures/interaction/transition.svg')


for fn in [timeline,transforms,terrain,pole,kepler,orbit_trails,lighting,ocean,atmosphere,atmosphere_pipeline,performance,adaptive,transition]:
    fn()
print('Generated 13 SVG figures')
