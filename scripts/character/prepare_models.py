"""Prepare downloaded astronaut candidates; run with Blender --disable-autoexec.

Inputs, creator credits and licenses live beside each model under USER_IO.
The InspectionWalk clip probes deformation; it is not production locomotion.
"""
import json
import math
from pathlib import Path

import bmesh
import bpy
from mathutils import Matrix, Quaternion, Vector

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'USER_IO/astronaut_vis'
MIXAMO = {
    'hips': 'mixamorig:Hips', 'head': 'mixamorig:Head',
    'left_thigh': 'mixamorig:LeftUpLeg', 'left_shin': 'mixamorig:LeftLeg',
    'left_foot': 'mixamorig:LeftFoot', 'right_thigh': 'mixamorig:RightUpLeg',
    'right_shin': 'mixamorig:RightLeg', 'right_foot': 'mixamorig:RightFoot',
    'left_arm': 'mixamorig:LeftArm', 'right_arm': 'mixamorig:RightArm',
}
ASTRODEV = {
    'hips': 'Bone', 'head': 'Bone.001',
    'left_thigh': 'Bone.010', 'left_shin': 'Bone.011', 'left_foot': 'Bone.012',
    'right_thigh': 'Bone.015', 'right_shin': 'Bone.016', 'right_foot': 'Bone.017',
    'left_arm': 'Bone.005', 'right_arm': 'Bone.020',
}


def load(key):
    folder = OUT / 'models' / key
    if key == 'astrodev':
        bpy.ops.wm.open_mainfile(filepath=str(folder / 'source.blend'),
                                 load_ui=False, use_scripts=False)
    else:
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.fbx(filepath=str(folder / 'source.fbx'))
        for image in bpy.data.images:
            image.filepath = str(folder / 'source.png')
            image.reload()
    for ob in list(bpy.data.objects):
        if ob.type not in ('MESH', 'ARMATURE'):
            bpy.data.objects.remove(ob, do_unlink=True)
    meshes = [o for o in bpy.data.objects if o.type == 'MESH']
    rigs = [o for o in bpy.data.objects if o.type == 'ARMATURE']
    assert len(meshes) == len(rigs) == 1
    mesh, arm = meshes[0], rigs[0]
    for ob in (mesh, arm):
        ob.hide_set(False)
        ob.hide_render = False
        ob.animation_data_clear()
    for bone in arm.pose.bones:
        bone.matrix_basis = Matrix.Identity(4)
    bpy.context.view_layer.update()
    return folder, mesh, arm


def bounds(mesh):
    points = [mesh.matrix_world @ v.co for v in mesh.data.vertices]
    lo = Vector([min(p[i] for p in points) for i in range(3)])
    hi = Vector([max(p[i] for p in points) for i in range(3)])
    return lo, hi


def remove_pack(key, mesh):
    """Remove only the identified rear pack; retain all limb topology/weights."""
    before = len(mesh.data.vertices)
    bm = bmesh.new()
    bm.from_mesh(mesh.data)
    bm.verts.ensure_lookup_table()
    original_boundary = sum(e.is_boundary for e in bm.edges)
    if key == 'polygonal-cosmonaut':
        # The creator's pack is one disconnected 178-vertex component.
        pending = [bm.verts[0]]
        component = set(pending)
        while pending:
            v = pending.pop()
            for e in v.link_edges:
                other = e.other_vert(v)
                if other not in component:
                    component.add(other)
                    pending.append(other)
        assert len(component) == 178, 'Source topology changed; reinspect the pack'
        bmesh.ops.delete(bm, geom=list(component), context='VERTS')
    elif key == 'astrodev':
        # This pack is attached to the torso. Remove its protruding vertices,
        # protect helmet vertices, then close the resulting torso boundary.
        lo, hi = bounds(mesh)
        mid = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))
        scale = 2 / (hi.z - lo.z)
        head_group = mesh.vertex_groups['Bone.001'].index
        deform = bm.verts.layers.deform.active
        selected = []
        for v in bm.verts:
            p = (mesh.matrix_world @ v.co - mid) * scale
            if p.y > .17 and .65 < p.z < 1.46 and v[deform].get(head_group, 0) < .5:
                selected.append(v)
        assert 70 < len(selected) < 160
        bmesh.ops.delete(bm, geom=selected, context='VERTS')
        edges = [e for e in bm.edges if e.is_boundary]
        caps = bmesh.ops.holes_fill(bm, edges=edges, sides=0)['faces']
        assert caps, 'The removed pack must leave a closable torso boundary'
        uv = bm.loops.layers.uv.active
        for face in caps:
            for loop in face.loops:
                loop[uv].uv = (.15, .85)  # Original palette's white suit swatch.
        bmesh.ops.triangulate(bm, faces=caps)
        bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
        assert sum(e.is_boundary for e in bm.edges) == original_boundary == 0
    bm.to_mesh(mesh.data)
    bm.free()
    mesh.data.update()
    return {'removed_vertices': before - len(mesh.data.vertices),
            'method': {'astrodev': 'Remove integrated rear pack and cap torso using original palette',
                       'polygonal-cosmonaut': 'Remove disconnected rear-pack component',
                       'polygonal-astronaut': 'No backpack in the original model'}[key]}


def normalize(key, mesh, arm):
    lo, hi = bounds(mesh)
    scale = 2 / (hi.z - lo.z)
    center = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))
    rotation = Matrix.Rotation(math.pi if key == 'polygonal-astronaut' else 0, 4, 'Z')
    node = bpy.data.objects.new('CandidateOrigin', None)
    bpy.context.scene.collection.objects.link(node)
    for ob in (mesh, arm):
        if ob.parent is None:
            ob.parent = node
            ob.matrix_parent_inverse = Matrix.Identity(4)
    node.matrix_world = rotation @ Matrix.Scale(scale, 4) @ Matrix.Translation(-center)
    bpy.context.view_layer.update()


def make_material(key, folder, mesh):
    if key == 'astrodev':
        image = bpy.data.images.load(str(folder / 'palette.png'))
        mat = bpy.data.materials.new('Original AstroDev palette')
        mat.use_nodes = True
        tex = mat.node_tree.nodes.new('ShaderNodeTexImage')
        tex.image = image
        mat.node_tree.links.new(tex.outputs['Color'], mat.node_tree.nodes['Principled BSDF'].inputs['Base Color'])
        mesh.data.materials.clear()
        mesh.data.materials.append(mat)
    for mat in mesh.data.materials:
        assert mat and mat.use_nodes
        bsdf = mat.node_tree.nodes.get('Principled BSDF')
        assert bsdf
        bsdf.inputs['Roughness'].default_value = .55
    for image in bpy.data.images:
        if image.type == 'IMAGE' and image.size[0]:
            image.pack()


def make_walk(arm, semantic):
    """A conservative, authored FK loop probes both knees and ankles."""
    base_arms = {}
    for side in ('left', 'right'):
        b = arm.pose.bones[semantic[side + '_arm']]
        world_rest = arm.matrix_world @ b.bone.matrix_local
        direction = (arm.matrix_world.to_3x3() @ b.bone.vector).normalized()
        target = Vector((math.copysign(.22, direction.x), 0, -.975)).normalized()
        delta = direction.rotation_difference(target)
        q = world_rest.to_quaternion()
        base_arms[b.name] = q.inverted() @ delta @ q
    arm.animation_data_create()
    action = bpy.data.actions.new('InspectionWalk')
    arm.animation_data.action = action
    scene = bpy.context.scene
    scene.render.fps = 30
    scene.frame_start, scene.frame_end = 1, 31
    for frame in range(1, 32):
        phase = 2 * math.pi * (frame - 1) / 30
        for bone in arm.pose.bones:
            bone.rotation_mode = 'QUATERNION'
            bone.rotation_quaternion = base_arms.get(bone.name, Quaternion())
        for side, offset in [('left', 0), ('right', math.pi)]:
            swing = math.sin(phase + offset)
            knee = .60 * max(0, swing)
            for role, angle in [('thigh', -.35 * swing), ('shin', knee), ('foot', .35 * swing - knee)]:
                b = arm.pose.bones[semantic[side + '_' + role]]
                rest = arm.matrix_world @ b.bone.matrix_local
                axis = rest.to_3x3().inverted() @ Vector((1, 0, 0))
                b.rotation_quaternion = Quaternion(axis.normalized(), angle)
                b.keyframe_insert('rotation_quaternion', frame=frame)
        for bone_name in base_arms:
            arm.pose.bones[bone_name].keyframe_insert('rotation_quaternion', frame=frame)
    for curve in action.fcurves:
        for point in curve.keyframe_points:
            point.interpolation = 'LINEAR'
    scene.frame_set(1)
    return action


def audit(mesh, arm, semantic):
    for side in ('left', 'right'):
        chain = [arm.data.bones[semantic[side + '_' + part]] for part in ('thigh', 'shin', 'foot')]
        assert chain[1].parent == chain[0] and chain[2].parent == chain[1]
        assert all(b.length > .005 for b in chain)
    deform_groups = {g.index for g in mesh.vertex_groups if g.name in arm.data.bones}
    for v in mesh.data.vertices:
        weights = sorted([(g.group, g.weight) for g in v.groups
                          if g.group in deform_groups and g.weight > 0],
                         key=lambda g: g[1], reverse=True)[:4]
        total = sum(weight for _, weight in weights)
        assert total > 0, ('Unweighted vertex', v.index)
        for group in mesh.vertex_groups:
            group.remove([v.index])
        for group, weight in weights:
            mesh.vertex_groups[group].add([v.index], weight / total, 'REPLACE')
    samples, ankles = [], {side: [] for side in ('left', 'right')}
    for frame in range(1, 32):
        bpy.context.scene.frame_set(frame)
        dg = bpy.context.evaluated_depsgraph_get()
        evaluated = mesh.evaluated_get(dg)
        points = [evaluated.matrix_world @ v.co for v in evaluated.data.vertices]
        assert all(math.isfinite(x) for p in points for x in p)
        assert all(p.length < 4 for p in points), 'Unbounded deformation'
        samples.append(points)
        for side in ankles:
            ankles[side].append(arm.matrix_world @ arm.pose.bones[semantic[side + '_foot']].head)
    travel = {side: max((p - points[0]).length for p in points) for side, points in ankles.items()}
    assert all(d > .05 for d in travel.values()), travel
    deformation = max((p - rest).length for points in samples for p, rest in zip(points, samples[0]))
    assert .05 < deformation < 1.2, deformation
    bpy.context.scene.frame_set(1)
    mesh.data.calc_loop_triangles()
    return {'vertices': len(mesh.data.vertices), 'triangles': len(mesh.data.loop_triangles),
            'bones': len(arm.data.bones), 'semantic_bones': semantic,
            'weighted_vertices': len(mesh.data.vertices), 'max_influences': 4,
            'finite_pose_samples': len(samples),
            'ankle_travel_m': travel, 'max_vertex_travel_m': deformation,
            'animations': ['InspectionWalk'], 'clip_duration_s': 1,
            'backpack_present': False,
            'bones_detail': [{'name': b.name, 'parent': b.parent.name if b.parent else None,
                             'head': list(b.head_local), 'tail': list(b.tail_local)} for b in arm.data.bones]}


def main():
    for key in ('astrodev', 'polygonal-astronaut', 'polygonal-cosmonaut'):
        folder, mesh, arm = load(key)
        pack = remove_pack(key, mesh)
        normalize(key, mesh, arm)
        make_material(key, folder, mesh)
        semantic = ASTRODEV if key == 'astrodev' else MIXAMO
        make_walk(arm, semantic)
        report = audit(mesh, arm, semantic)
        report['backpack_preparation'] = pack
        source = json.loads((folder / 'source.json').read_text())
        for ob in (mesh, arm):
            ob['asset_creator'] = source['creator']
            ob['asset_source'] = source['page']
            ob['asset_license'] = source['license']
            ob['asset_changes'] = pack['method'] + '; scale/orientation, four-weight normalization and InspectionWalk'
        # Factory reset for FBX imports restores preferences as well.
        bpy.context.preferences.filepaths.save_version = 0
        bpy.ops.wm.save_as_mainfile(filepath=str(folder / 'astronaut.blend'))
        bpy.ops.object.select_all(action='DESELECT')
        for ob in bpy.data.objects:
            ob.select_set(True)
        bpy.ops.export_scene.gltf(filepath=str(folder / 'astronaut.glb'),
                                  export_format='GLB', use_selection=True,
                                  export_animations=True, export_force_sampling=True,
                                  export_nla_strips=False,
                                  export_nla_strips_merged_animation_name='InspectionWalk',
                                  export_all_influences=False,
                                  export_extras=True)
        (folder / 'inspection.json').write_text(json.dumps(report, indent=2) + '\n')
        print('PREPARED', key, report['triangles'], 'triangles,', report['bones'],
              'bones;', pack, 'ankle travel:', report['ankle_travel_m'], flush=True)


if __name__ == '__main__':
    main()
