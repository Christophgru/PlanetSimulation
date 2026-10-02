#!/usr/bin/env python3
"""Reproduce the production scene's jetpack sizing; SI, no third-party modules."""
import argparse
import hashlib
import json
import math
from pathlib import Path

root = Path(__file__).resolve().parents[4]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--config', type=Path, default=root / 'configs/scenarios/solar_system.json')
args = parser.parse_args()
source = args.config.resolve()
scene = json.loads(source.read_text())
p = scene['planets'][0]
a = dict(enabled=False, surface_pressure_pa=101325, temperature_k=293.15,
         nitrogen=78.08, oxygen=20.95, water=0, carbon_dioxide=.04, argon=.93, red_dust=0)
a.update(p.get('atmosphere', {}))
a.update(p.get('atmosphere', {}).get('gas-contents', {}))
units = {'m': 1, 'km': 1000, 'au': 149597870700}[scene['distance_unit']]
radius = p['radius'] * units + (p.get('water', {}).get('level_m', 0) if p.get('water', {}).get('enabled') else 0)
period = p.get('rotation', {}).get('period_seconds', 0)
omega = 2 * math.pi / period if period else 0
g = 6.67430e-11 * p['mass_kg'] / radius**2
balance = max(0, 100 - sum(a[k] for k in ('nitrogen', 'oxygen', 'water', 'carbon_dioxide', 'argon', 'red_dust')))
molar = (a['nitrogen']*.0280134+a['oxygen']*.031998+a['water']*.01801528+a['carbon_dioxide']*.0440095+(a['argon']+balance)*.039948) / (100-a['red_dust'])
rho = a['surface_pressure_pa']*molar/(8.314462618*a['temperature_k']) if a['enabled'] else 0
mass, cd_area, speed, exhaust, efficiency = 100, .7, 100, 1000, .6
drag = .5*rho*cd_area*speed**2
installed = max(3000, 1.1*math.hypot(mass*(g+20), drag))
rows = []
for label, east in [('east',100),('north',0),('west',-100)]:
    support = mass*(g-omega**2*radius-2*omega*east-speed**2/radius)
    thrust = math.hypot(max(0,support),drag)
    rows.append(dict(heading=label,support_n=support,level_thrust_n=thrust,
                     tilt_deg=math.degrees(math.atan2(drag,max(0,support))),
                     estimated_exhaust_input_w=thrust*exhaust/(2*efficiency)))
report = dict(source=str(source.relative_to(root)) if source.is_relative_to(root) else str(source),source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
              planet=p['name'],radius_sea_m=radius,spin_rad_s=omega,gravity_mps2=g,
              equatorial_rest_gravity_mps2=g-omega**2*radius,
              molar_mass_kg_mol=molar,sea_density_kg_m3=rho,sea_pressure_pa=a['surface_pressure_pa'],
              mass_kg=mass,drag_area_cd_m2=cd_area,speed_mps=speed,drag_n=drag,
              useful_horizontal_power_w=drag*speed,exhaust_speed_mps=exhaust,efficiency=efficiency,
              maximum_thrust_n=installed,maximum_estimated_exhaust_input_w=installed*exhaust/(2*efficiency),
              fixed_local_sea_density_terminal_speed_mps=math.sqrt(2*mass*max(0,g-omega**2*radius)/(rho*cd_area)) if rho else None,
              equatorial_level_flight=rows)
Path(__file__).with_name('power.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
