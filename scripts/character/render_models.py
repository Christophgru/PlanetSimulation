"""Render the delivered GLBs and audit their imported walking deformation."""
import argparse
import json
import math
import sys
from pathlib import Path

import bpy
import numpy as np
from mathutils import Vector
from bpy_extras.object_utils import world_to_camera_view

# Debian Blender 3.4's bundled importer still uses the removed NumPy alias.
if bpy.app.version < (3, 5, 0):
    np.bool = np.bool_

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'USER_IO/astronaut_vis'


def aim(ob, target):
    ob.rotation_euler = (Vector(target) - ob.location).to_track_quat('-Z', 'Y').to_euler()


def import_and_check(key):
    folder = OUT / 'models' / key
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=str(folder / 'astronaut.glb'))
    meshes = [o for o in bpy.data.objects if o.type == 'MESH']
    arms = [o for o in bpy.data.objects if o.type == 'ARMATURE']
    assert len(meshes) == len(arms) == 1
    mesh, arm = meshes[0], arms[0]
    action = next(a for a in bpy.data.actions if 'InspectionWalk' in a.name)
    arm.animation_data_create()
    for track in arm.animation_data.nla_tracks:
        track.mute = True
    arm.animation_data.action = action
    report = json.loads((folder / 'inspection.json').read_text())
    semantic = report['semantic_bones']
    for side in ('left', 'right'):
        thigh, shin, foot = [arm.data.bones[semantic[side + '_' + part]]
                             for part in ('thigh', 'shin', 'foot')]
        assert shin.parent == thigh and foot.parent == shin
    for v in mesh.data.vertices:
        weights = [g.weight for g in v.groups if g.weight > 0]
        assert 1 <= len(weights) <= 4
        assert abs(sum(weights) - 1) < 2e-5
    first, last = action.frame_range
    all_points, ankles = [], {'left': [], 'right': []}
    for sample in range(17):
        frame = first + (last - first) * sample / 16
        bpy.context.scene.frame_set(int(frame), subframe=frame % 1)
        evaluated = mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
        points = [evaluated.matrix_world @ v.co for v in evaluated.data.vertices]
        assert all(math.isfinite(x) for p in points for x in p)
        assert all(p.length < 4 for p in points)
        all_points.append(points)
        for side in ankles:
            ankles[side].append(arm.matrix_world @ arm.pose.bones[semantic[side + '_foot']].head)
    travel = {side: max((p - samples[0]).length for p in samples)
              for side, samples in ankles.items()}
    assert all(d > .05 for d in travel.values()), travel
    start, finish = all_points[0], all_points[-1]
    closure = max((a - b).length for a, b in zip(start, finish))
    assert closure < 1e-4, closure
    deformation = max((p - rest).length for points in all_points for p, rest in zip(points, start))
    assert .05 < deformation < 1.2
    bpy.context.scene.frame_set(int(first), subframe=first % 1)
    return {'model': key, 'vertices': len(mesh.data.vertices),
            'bones': len(arm.data.bones), 'weighted_vertices': len(mesh.data.vertices),
            'finite_pose_samples': 17, 'loop_closure_m': closure,
            'ankle_travel_m': travel, 'max_vertex_travel_m': deformation}, first, last


def studio():
    scene = bpy.context.scene
    scene.render.engine = 'BLENDER_EEVEE'
    scene.eevee.taa_render_samples = 48
    scene.eevee.use_gtao = True
    scene.eevee.gtao_distance = .18
    scene.eevee.gtao_factor = 1.1
    scene.eevee.use_soft_shadows = True
    scene.render.resolution_x = 900
    scene.render.resolution_y = 1080
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.color_mode = 'RGB'
    scene.view_settings.view_transform = 'Filmic'
    scene.view_settings.look = 'Medium High Contrast'
    scene.view_settings.exposure = 0
    scene.world = bpy.data.worlds.new('Studio')
    scene.world.use_nodes = True
    bg = scene.world.node_tree.nodes['Background']
    bg.inputs[0].default_value = (.14, .17, .23, 1)
    bg.inputs[1].default_value = .6
    bpy.ops.mesh.primitive_plane_add(size=200, location=(0, 0, -.025))
    floor = bpy.context.object
    mat = bpy.data.materials.new('Slate floor')
    mat.diffuse_color = (.13, .16, .22, 1)
    floor.data.materials.append(mat)
    for label, loc, energy, size in [('Key', (3, -4, 5), 500, 4),
                                     ('Fill', (-3, -1, 3), 250, 3),
                                     ('Rim', (2, 4, 4), 650, 3)]:
        light = bpy.data.lights.new(label, 'AREA')
        light.energy, light.size = energy, size
        light.use_shadow = True
        ob = bpy.data.objects.new(label, light)
        scene.collection.objects.link(ob)
        ob.location = loc
        aim(ob, (0, 0, 1))
    cam = bpy.data.objects.new('Studio camera', bpy.data.cameras.new('Studio camera'))
    scene.collection.objects.link(cam)
    scene.camera = cam
    cam.data.type = 'ORTHO'
    cam.data.ortho_scale = 3.2
    return scene, cam


def main():
    keys = ('astrodev', 'polygonal-astronaut', 'polygonal-cosmonaut')
    parser = argparse.ArgumentParser()
    parser.add_argument('--model', choices=keys, help='Render one candidate per process')
    parser.add_argument('--views', nargs='+', choices=('front', 'back', 'walk'),
                        default=['front', 'back', 'walk'])
    parser.add_argument('--audit-only', action='store_true')
    argv = sys.argv
    args = parser.parse_args(argv[argv.index('--') + 1:] if '--' in argv else [])
    report_path = OUT / 'validation/roundtrip.json'
    reports = json.loads(report_path.read_text()) if args.model and report_path.exists() else []
    for key in ([args.model] if args.model else keys):
        report, first, last = import_and_check(key)
        reports = [r for r in reports if r['model'] != key]
        reports.append(report)
        report_path.write_text(json.dumps(reports, indent=2) + '\n')
        print('ROUNDTRIP PASS', json.dumps(report), flush=True)
        if args.audit_only:
            continue
        actors = [o for o in bpy.data.objects if o.type == 'MESH']
        scene, cam = studio()
        for name, loc, frame, path in [
            ('front', (3, -6, 3), first, OUT / (key + '.png')),
            ('back', (-3, 6, 3), first, OUT / 'views' / (key + '-back.png')),
            ('walk', (3, -6, 3), first + .25 * (last - first), OUT / 'views' / (key + '-walk.png')),
        ]:
            if name not in args.views:
                continue
            scene.frame_set(int(frame), subframe=frame % 1)
            cam.location = loc
            aim(cam, (0, 0, 1))
            bpy.context.view_layer.update()
            dg = bpy.context.evaluated_depsgraph_get()
            projected = [world_to_camera_view(scene, cam, ob.matrix_world @ v.co)
                         for ob in actors for v in ob.evaluated_get(dg).data.vertices]
            assert all(.01 < p.x < .99 and .01 < p.y < .99 and p.z > 0 for p in projected), 'Actor cropped by preview camera'
            scene.render.filepath = str(path)
            bpy.ops.render.render(write_still=True)
            print('RENDERED', key, name, path, flush=True)


if __name__ == '__main__':
    main()
