"""Prepare the selected NASA ACES mesh for the existing procedural locomotion rig.

Blender 4.5 LTS; original geometry/linear material colours, two joint influences.
Runtime is GL 3.3 vertex skinning. This does not invent authored animation clips.
"""
import hashlib
import json
import math
import struct
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'data/character/aces'
SOURCE = OUT / 'source.glb'
SHA = '6017772d557c30ea86e2e3a7651ad2089499f3d0c577b983467650616bfaebeb'
HEIGHT = .09 + 2 * math.hypot(.47, .10)
OFFSET = HEIGHT - .75
BONES = [('body', (0, HEIGHT, 0), (0, 1.65, 0))]
for side in (-1, 1):
    x = side * .17
    BONES += [('thigh', (x, HEIGHT, 0), (x, (HEIGHT + .09) / 2, 0)),
              ('shin', (x, (HEIGHT + .09) / 2, 0), (x, .09, 0)),
              ('foot', (x, .09, 0), (x, .09, -.20))]
for side in (-1, 1):
    BONES += [('upper_arm', (side * .28, 1.55, 0), (side * .38, 1.32, -.08)),
              ('forearm', (side * .38, 1.32, -.08), (side * .40, 1.12, -.16)),
              ('hand', (side * .40, 1.12, -.16), (side * .42, 1.01, -.21))]


def smooth(low, high, value):
    t = min(1, max(0, (value - low) / (high - low)))
    return t * t * (3 - 2 * t)


def weights(p):
    x, y, _ = p
    # The helmet stays rigid. Sleeves/hands lie outside the torso and hips.
    if .90 < y < 1.66 and abs(x) > .28:
        base = 7 if x < 0 else 10
        if y > 1.43:
            return (0, base), (1 - smooth(.28, .34, abs(x)), smooth(.28, .34, abs(x)))
        if y > 1.20:
            t = smooth(1.25, 1.39, y)
            return (base, base + 1), (t, 1 - t)
        t = smooth(1.08, 1.17, y)
        return (base + 1, base + 2), (t, 1 - t)
    base = 1 if x < 0 else 4
    if y > HEIGHT + .10:
        return (0, 0), (1, 0)
    if y > HEIGHT - .10:
        t = smooth(HEIGHT - .10, HEIGHT + .10, y)
        return (0, base), (t, 1 - t)
    if y > .70:
        # Avoid tearing the joined crotch at the left/right classification seam.
        t = smooth(.02, .12, abs(x)) * (1 - smooth(.90, HEIGHT, y))
        return (0, base), (1 - t, t)
    if y > .37:
        t = smooth(.46, .67, y)
        return (base, base + 1), (t, 1 - t)
    t = smooth(.13, .27, y)
    return (base + 1, base + 2), (t, 1 - t)


def main():
    assert bpy.app.version[:2] == (4, 5)
    assert hashlib.sha256(SOURCE.read_bytes()).hexdigest() == SHA
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=str(SOURCE))
    meshes = [o for o in bpy.data.objects if o.type == 'MESH']
    points = [o.matrix_world @ v.co for o in meshes for v in o.data.vertices]
    lo = Vector(tuple(min(p[i] for p in points) for i in range(3)))
    hi = Vector(tuple(max(p[i] for p in points) for i in range(3)))
    center = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))
    # Blender Z-up/front +Y -> renderer Y-up/front -Z, both right handed.
    transform = (Matrix.Rotation(-math.pi / 2, 4, 'X')
                 @ Matrix.Scale(2.1 / (hi.z - lo.z), 4) @ Matrix.Translation(-center))
    vertices, indices, unique, materials = [], [], {}, []
    for ob in meshes:
        matrix = transform @ ob.matrix_world
        normal_matrix = matrix.to_3x3().inverted().transposed()
        ob.data.calc_loop_triangles()
        colors = []
        for material in ob.data.materials:
            node = next(n for n in material.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
            assert not node.inputs['Base Color'].is_linked, 'ACES unexpectedly gained textures'
            color = tuple(node.inputs['Base Color'].default_value[:3])
            shine = 1.0 if material.name.endswith('aceshelme.008') else 0.0
            colors.append((*color, shine))
            materials.append({'name': material.name, 'linear_rgb': color, 'shine': shine})
        for triangle in ob.data.loop_triangles:
            for loop in triangle.loops:
                v = ob.data.vertices[ob.data.loops[loop].vertex_index]
                p = matrix @ v.co
                normal = (normal_matrix @ ob.data.corner_normals[loop].vector).normalized()
                joints, influence = weights(p)
                values = (*p, *normal, *colors[triangle.material_index], *joints, *influence)
                assert all(math.isfinite(v) for v in values)
                # Weld only byte-identical position/normal/colour/skin tuples.
                record = struct.pack('<10f2H2f', *values)
                if record not in unique:
                    unique[record] = len(vertices)
                    vertices.append(record)
                indices.append(unique[record])
    assert len(indices) == 88872 * 3
    header = struct.pack('<8s4I3f', b'ACESSUIT', 1, len(vertices), len(indices), len(BONES), 0, OFFSET, 0)
    bindings = b''.join(struct.pack('<6f', *head, *tail) for _, head, tail in BONES)
    blob = header + bindings + b''.join(vertices) + struct.pack('<' + str(len(indices)) + 'I', *indices)
    (OUT / 'suit.bin').write_bytes(blob)
    report = {'schema': 1, 'source_sha256': SHA, 'suit_sha256': hashlib.sha256(blob).hexdigest(),
              'vertices': len(vertices), 'triangles': len(indices) // 3, 'vertex_stride': 52,
              'height_m': 2.1, 'bind_body_offset_m': [0, OFFSET, 0],
              'bones': [{'name': name, 'head': head, 'tail': tail} for name, head, tail in BONES],
              'materials': materials, 'max_influences': 2, 'blender': bpy.app.version_string}
    (OUT / 'inspection.json').write_text(json.dumps(report, indent=2) + '\n')
    print('PREPARED ACES', len(vertices), 'vertices;', len(indices) // 3, 'triangles;', len(blob), 'bytes')


if __name__ == '__main__':
    main()
