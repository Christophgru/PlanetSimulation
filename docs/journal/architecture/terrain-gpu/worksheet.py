#!/usr/bin/env python3
"""Reproduce topology and capacity arithmetic for the architecture plan.

This evaluates a design worksheet, not a GPU implementation or benchmark.
"""
import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]


def topology(level):
    golden = (1 + math.sqrt(5)) / 2
    vertices = [(-1,golden,0),(1,golden,0),(-1,-golden,0),(1,-golden,0),
                (0,-1,golden),(0,1,golden),(0,-1,-golden),(0,1,-golden),
                (golden,0,-1),(golden,0,1),(-golden,0,-1),(-golden,0,1)]
    def normalized(p):
        length = math.sqrt(sum(x*x for x in p))
        return tuple(x/length for x in p)
    vertices = [normalized(p) for p in vertices]
    faces = [(0,11,5),(0,5,1),(0,1,7),(0,7,10),(0,10,11),
             (1,5,9),(5,11,4),(11,10,2),(10,7,6),(7,1,8),
             (3,9,4),(3,4,2),(3,2,6),(3,6,8),(3,8,9),
             (4,9,5),(2,4,11),(6,2,10),(8,6,7),(9,8,1)]
    for _ in range(level):
        midpoints = {}
        def midpoint(a,b):
            key = tuple(sorted((a,b)))
            if key not in midpoints:
                midpoints[key] = len(vertices)
                vertices.append(normalized(tuple(x+y for x,y in zip(vertices[a],vertices[b]))))
            return midpoints[key]
        refined = []
        for a,b,c in faces:
            ab,bc,ca = midpoint(a,b),midpoint(b,c),midpoint(c,a)
            refined.extend(((a,ab,ca),(b,bc,ab),(c,ca,bc),(ab,bc,ca)))
        faces = refined
    edges = Counter()
    neighbors = [set() for _ in vertices]
    for a,b,c in faces:
        for x,y in ((a,b),(b,c),(c,a)):
            edges[tuple(sorted((x,y)))] += 1
            neighbors[x].add(y); neighbors[y].add(x)
    degrees = Counter(map(len,neighbors))
    assert set(edges.values()) == {2}, 'Non-manifold base topology'
    assert len(vertices)-len(edges)+len(faces) == 2
    assert degrees[5] == 12 and sum(degrees.values()) == len(vertices)
    assert set(degrees) <= {5,6}
    return {'level':level,'vertices':len(vertices),'edges':len(edges),'triangles':len(faces),
            'dual_pentagons':degrees[5],'dual_hexagons':degrees[6],'euler_characteristic':2}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    args = parser.parse_args()
    production = ROOT/'configs/scenarios/solar_system.json'
    config = json.loads(production.read_text())
    showcase_path = ROOT/'docs/journal/benchmarks/offline/showcase/validation/results.json'
    showcase = json.loads(showcase_path.read_text())
    dense = showcase['captures']['dense']['render']
    assert dense['foliage_gpu_working_bytes'] == 128*dense['foliage_candidates']+32
    triangles = dense['body_mesh_triangles'][0]
    # Euler's relation for an indexed, closed, genus-zero triangle mesh.
    # This is a topology assumption, not a measured weld of the terrain mesh.
    unique = triangles//2+2
    old_upload = triangles*120
    proposed_upload = unique*32 + triangles*12
    foliage = config['planets'][config['surface_camera']['planet_index']]['foliage']
    density = foliage['density_per_m2']
    near_radius = 15.0
    ideal_near = density*math.pi*near_radius**2
    report = {
        'scope':'Analytical design worksheet; no new runtime/performance measurements',
        'production_config_sha256':hashlib.sha256(production.read_bytes()).hexdigest(),
        'showcase_results_sha256':hashlib.sha256(showcase_path.read_bytes()).hexdigest(),
        'topology':[topology(level) for level in range(4)],
        'transfer_model':{'triangles':triangles,'assumed_unique_vertices':unique,
            'current_duplicate_mesh_upload_bytes':old_upload,
            'proposed_double_direction_sink_and_indices_bytes':proposed_upload,
            'upload_reduction_fraction':1-proposed_upload/old_upload,
            'proposed_gpu_render_buffer_and_indices_bytes':unique*36+triangles*12,
            'excludes':'parameters, alignment, IDs, shoreline decisions, staging and cached topology'},
        'foliage_capacity':{'bytes_per_current_candidate':128,
            'dense_measured_candidates':dense['foliage_candidates'],
            'dense_measured_bytes':dense['foliage_gpu_working_bytes'],
            'hypothetical_queue_budget_bytes':256*1024**2,
            'hypothetical_capacity':(256*1024**2-32)//128,
            'density_per_m2':density,'proposed_near_plateau_radius_m':near_radius,
            'ideal_near_candidates_before_slots_and_margin':ideal_near,
            'ideal_near_two_queue_bytes':math.ceil(ideal_near)*128+32,
            'notes':'Near count assumes an entirely eligible reference sphere at surface height and chord distance; slots, rebuild margin and terrain area increase reservation.'}}
    assert report['topology'][2]['triangles']==320
    assert report['transfer_model']['upload_reduction_fraction'] > .75
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print('PASS manifold levels 0-3, Euler closure, 12 pentagonal exceptions, capacity/transfer arithmetic')
    print(f'Analytical upload reduction: {100*(1-proposed_upload/old_upload):.4f}%')
    print(f'Ideal 15 m near plateau: {ideal_near:.2f} candidates at {density} blades/m²')


if __name__ == '__main__':
    main()
