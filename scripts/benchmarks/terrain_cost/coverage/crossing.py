"""Excluded diagnostics; never move a native generation to an exact landmark."""
import math


def bracket(frames, snapshot, target):
    index = next(i for i, f in enumerate(frames)
                 if f['profile_frame'] == snapshot['profile_frame'])
    assert index > 0
    previous, current = frames[index-1:index+1]
    assert current['frame'] == previous['frame']+1
    assert current['pose']['root'] == snapshot['root_m']
    assert current['pose']['walked_m'] == snapshot['walked_m']
    lo, hi = [f['pose']['walked_m'] for f in (previous, current)]
    assert lo < target <= hi, (lo, target, hi)
    assert all(f['pose']['walked_m'] < target for f in frames[:index] if f.get('pose'))
    body = snapshot['workload']['bodies'][snapshot['body']]
    scale = snapshot['meters_per_radius']
    return {'target_m': target, 'previous_profile_frame': previous['profile_frame'],
            'crossing_profile_frame': current['profile_frame'], 'walked_interval_m': [lo, hi],
            'interval_width_m': hi-lo, 'overshoot_m': hi-target,
            'native_root_m': snapshot['root_m'],
            'land_field': snapshot['land_field'], 'land_topology': snapshot['land_topology'],
            'land_revision': snapshot['land_revision'],
            'terrain_plan_eye': body['terrain_plan_eye'],
            'grass_plan_eye': body['foliage']['plan_eye'],
            'grass_anchor_distance_m': math.dist(snapshot['root_m'],
                [x*scale for x in body['foliage']['plan_eye']]),
            'effective_density': body['foliage']['density'],
            'patches': body['foliage']['patches'],
            'publication': snapshot['publication_before_grass']}
