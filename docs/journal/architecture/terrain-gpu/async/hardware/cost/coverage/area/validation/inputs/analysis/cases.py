"""Known camera-space planes, finite silhouettes and production-scale transforms."""
import math
import numpy as np


def matrix_values(matrix):
    return np.asarray(matrix, np.float32).reshape(-1, order='F').astype(float).tolist()


def scene(angle, occlusion=False, transformed=False):
    angle = math.radians(angle)
    u, v = np.array([math.cos(angle), 0., math.sin(angle)]), np.array([0., 1., 0.])
    root_camera = np.array([0., 0., -20.])
    surface = np.array([root_camera+x*u+y*v for x, y in ((-80, -80), (80, -80), (80, 80), (-80, 80))])
    front = np.array([[-.9, -.7, -3.], [1.25, -.6, -3.], [.2, 1.3, -3.]])/6 if occlusion else np.empty((0, 3))
    scale = 1000. if transformed else 1.
    root_body = np.array([-.17, .95, -.26]) if transformed else root_camera
    model, view = np.eye(4), np.eye(4)
    if transformed:
        a, b = math.radians(20), math.radians(45)
        model[:3, :3] = [[1, 0, 0], [0, math.cos(a), -math.sin(a)], [0, math.sin(a), math.cos(a)]]
        model[:3, 3] = [9.4, 0, 0]
        view[:3, :3] = [[math.cos(b), 0, math.sin(b)], [0, 1, 0], [-math.sin(b), 0, math.cos(b)]]
        view[:3, 3] = root_camera/scale-view[:3, :3]@(model[:3, :3]@root_body+model[:3, 3])
    projection = np.zeros((4, 4))
    near, far = .1/scale, 500./scale
    projection[0, 0], projection[1, 1] = 1., 16/9
    projection[2, 2], projection[2, 3] = -(far+near)/(far-near), -2*far*near/(far-near)
    projection[3, 2] = -1
    # The exact reference uses metres, so its clipping projection has metre depth.
    reference_projection = projection.copy();reference_projection[2, 3] *= scale
    vm = view@model
    vertices = []
    for triangle, color in ((surface[[0, 1, 2]], [1, 2, 1]), (surface[[0, 2, 3]], [1, 2, 1])):
        for point in triangle:
            body = root_body+np.linalg.solve(vm[:3, :3], (point-root_camera)/scale)
            vertices.extend([*body, 0, 0, 1, *color])
    for point in front:
        body = root_body+np.linalg.solve(vm[:3, :3], (point-root_camera)/scale)
        vertices.extend([*body, 0, 0, 1, 2, 0, 0])
    return {'schema': 1, 'viewport': [640, 360], 'model': matrix_values(model), 'view': matrix_values(view),
            'projection': matrix_values(projection), 'root_body': root_body.astype(np.float32).astype(float).tolist(),
            'meters_per_radius': scale, 'vertices': np.asarray(vertices, np.float32).astype(float).tolist(),
            'reference': {'surface_camera_m': surface.tolist(), 'root_camera_m': root_camera.tolist(),
                          'occluder_camera_m': front.tolist(), 'projection': matrix_values(reference_projection)}}


def fixtures():
    return {'front': scene(0), 'oblique': scene(65), 'silhouette': scene(65, occlusion=True),
            'scaled-grazing': scene(82, occlusion=True, transformed=True)}
