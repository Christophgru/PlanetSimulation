"""Render actual NASA meshes: front, rear and close-up, with inspection receipts."""
import argparse
import hashlib
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector
from bpy_extras.object_utils import world_to_camera_view


assert bpy.app.version[:2] == (4, 5), 'Use Blender 4.5 LTS for these material/image imports'

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'USER_IO/astronaut_vis'
CACHE = ROOT / 'build-f5/astronaut-next'


def aim(ob, target):
    ob.rotation_euler = (Vector(target) - ob.location).to_track_quat('-Z', 'Y').to_euler()


def studio():
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 64
    scene.cycles.use_denoising = True
    prefs = bpy.context.preferences.addons['cycles'].preferences
    devices = []
    try:
        prefs.compute_device_type = 'CUDA'
        prefs.get_devices()
        devices = [d for d in prefs.devices if d.type == 'CUDA' and '3070 Ti' in d.name]
    except (TypeError, RuntimeError):
        pass  # CPU fallback on platforms without CUDA support.
    for device in prefs.devices:
        device.use = device in devices
    scene.cycles.device = 'GPU' if devices else 'CPU'
    print('Render device:', [d.name for d in devices] or ['CPU'], flush=True)
    scene.render.resolution_x = 900
    scene.render.resolution_y = 1080
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.color_mode = 'RGB'
    scene.view_settings.view_transform = 'AgX'
    scene.view_settings.look = 'AgX - Medium High Contrast'
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
    parser = argparse.ArgumentParser()
    parser.add_argument('--model', required=True)
    parser.add_argument('--views', nargs='+', choices=('front', 'back', 'detail'),
                        default=['front', 'back', 'detail'])
    argv = sys.argv[sys.argv.index('--') + 1:]
    args = parser.parse_args(argv)
    row = next(r for r in json.loads((OUT / 'sources.json').read_text())['models']
               if r['key'] == args.model)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    source = CACHE / (row['name'] + '.glb')
    assert hashlib.sha256(source.read_bytes()).hexdigest() == row['sha256']
    bpy.ops.import_scene.gltf(filepath=str(source))
    actors = [o for o in bpy.data.objects if o.type == 'MESH']
    assert actors and not any(o.type == 'ARMATURE' for o in bpy.data.objects)
    points = [o.matrix_world @ v.co for o in actors for v in o.data.vertices]
    assert all(math.isfinite(x) for p in points for x in p)
    low = Vector(tuple(min(p[i] for p in points) for i in range(3)))
    high = Vector(tuple(max(p[i] for p in points) for i in range(3)))
    scale = 2 / (high.z - low.z)
    center = Vector(((high.x + low.x) / 2, (high.y + low.y) / 2, low.z))
    transform = (Matrix.Rotation(math.radians(row['yaw_degrees']), 4, 'Z')
                 @ Matrix.Scale(scale, 4) @ Matrix.Translation(-center))
    # Apply to mesh world matrices only: all comparison assets are static.
    matrices = [(o, transform @ o.matrix_world) for o in actors]
    for o, matrix in matrices:
        o.parent = None
        o.matrix_world = matrix
    triangles = sum(sum(len(p.vertices) - 2 for p in o.data.polygons) for o in actors)
    assert triangles == row['triangles'] - row.get('degenerate_triangles', 0), (triangles, row['triangles'])
    materials = {m.name for o in actors for m in o.data.materials if m}
    # Several NASA exports mark opaque cloth/hardware as full transmission
    # with IOR=1. That makes them disappear in a physically based renderer.
    # Repair this exact export combination for the inspection scene only;
    # retain base colour, roughness, normals and partial visor transmission.
    repairs = []
    for name in sorted(materials):
        material = bpy.data.materials[name]
        for node in material.node_tree.nodes if material.use_nodes else []:
            if node.type != 'BSDF_PRINCIPLED':
                continue
            transmission = node.inputs['Transmission Weight']
            ior = node.inputs['IOR']
            if (not transmission.is_linked and not ior.is_linked
                    and transmission.default_value == 1 and ior.default_value == 1):
                transmission.default_value = 0
                repairs.append(name)
    images = [im for im in bpy.data.images if im.type == 'IMAGE']
    # Access pixel lengths to force Blender's lazy embedded-image loading.
    assert all(len(im.pixels) > 0 and im.has_data for im in images), 'Missing texture'
    report = {'key': row['key'], 'triangles': triangles,
              'source_triangles': row['triangles'],
              'display_yaw_degrees': row['yaw_degrees'],
              'vertices': sum(len(o.data.vertices) for o in actors),
              'meshes': len(actors), 'skins': 0, 'animations': len(bpy.data.actions),
              'normalized_height_m': 2, 'source_sha256': row['sha256'],
              'materials': sorted(materials),
              'opaque_transmission_repairs': repairs,
              'textures': [{'name': im.name, 'size': list(im.size)} for im in images],
              'blender_version': bpy.app.version_string, 'views': {}}
    scene, camera = studio()
    report['render_engine'] = scene.render.engine
    report['render_device'] = scene.cycles.device
    for name, location, target, ortho, path in (
        ('front', (2.5, -6, 2.7), (0, 0, 1), 2.8, OUT / (row['key'] + '.png')),
        ('back', (-2.5, 6, 2.7), (0, 0, 1), 2.8, OUT / 'views' / (row['key'] + '-back.png')),
        ('detail', (1, -4, 2), (0, 0, 1.65), 1.25, OUT / 'views' / (row['key'] + '-detail.png')),
    ):
        if name not in args.views:
            continue
        camera.location = location
        camera.data.ortho_scale = ortho
        aim(camera, target)
        bpy.context.view_layer.update()
        projected = [world_to_camera_view(scene, camera, o.matrix_world @ v.co)
                     for o in actors for v in o.data.vertices]
        assert all(p.z > 0 for p in projected)
        if name != 'detail':
            assert all(.01 < p.x < .99 and .01 < p.y < .99 for p in projected), 'Cropped suit'
        scene.render.filepath = str(path)
        path.parent.mkdir(parents=True, exist_ok=True)
        bpy.ops.render.render(write_still=True)
        report['views'][name] = {'file': str(path.relative_to(OUT)),
                                 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                                 'resolution': [900, 1080]}
        print('RENDERED', row['key'], name, flush=True)
    folder = OUT / 'validation'
    folder.mkdir(exist_ok=True)
    (folder / (row['key'] + '.json')).write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
