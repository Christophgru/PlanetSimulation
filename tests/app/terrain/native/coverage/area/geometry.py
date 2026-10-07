"""Independent exact polygon/circle surface-area references for GPU fixtures."""
import math
import numpy as np
from area import BANDS


def clip_halfplane(polygon, normal, offset=0):
    result = []
    if not len(polygon):
        return np.empty((0, len(normal)))
    previous = polygon[-1]
    a = np.dot(previous, normal)+offset
    for point in polygon:
        b = np.dot(point, normal)+offset
        if (a >= 0) != (b >= 0):
            result.append(previous+(point-previous)*a/(a-b))
        if b >= 0:
            result.append(point)
        previous, a = point, b
    return np.asarray(result).reshape(-1, len(normal))


def clip_frustum(polygon, projection):
    # Clip in homogeneous space without perspective division or pixel samples.
    for axis in range(3):
        for sign in (-1, 1):
            equation = projection[3]+sign*projection[axis]
            polygon = clip_halfplane(polygon, equation[:3], equation[3])
    return polygon


def convex_intersection(polygon, boundary):
    for a, b in zip(boundary, np.roll(boundary, -1, axis=0)):
        edge = b-a
        normal = np.array([-edge[1], edge[0]])
        polygon = clip_halfplane(polygon, normal, -np.dot(normal, a))
    return polygon


def circle_polygon_area(polygon, radius):
    """Exact signed segment/sector integration, split at circle crossings."""
    if radius <= 0 or not len(polygon):
        return 0.
    result = 0.
    for a, b in zip(polygon, np.roll(polygon, -1, axis=0)):
        edge = b-a
        aa, bb, cc = np.dot(edge, edge), 2*np.dot(a, edge), np.dot(a, a)-radius**2
        cuts = [0., 1.]
        discriminant = bb*bb-4*aa*cc
        if aa and discriminant > 0:
            cuts += [t for t in ((-bb-math.sqrt(discriminant))/(2*aa), (-bb+math.sqrt(discriminant))/(2*aa)) if 0 < t < 1]
        cuts.sort()
        for start, end in zip(cuts, cuts[1:]):
            p, q = a+start*edge, a+end*edge
            cross = p[0]*q[1]-p[1]*q[0]
            if np.dot((p+q)/2, (p+q)/2) <= radius**2:
                result += cross/2
            else:
                result += radius**2*math.atan2(cross, np.dot(p, q))/2
    return abs(result)


def exact_bands(scene):
    projection = np.array(scene['reference']['projection']).reshape(4, 4, order='F')
    vm = np.array(scene['view']).reshape(4, 4, order='F')@np.array(scene['model']).reshape(4, 4, order='F')
    scale = scene['meters_per_radius']
    vertices = np.array(scene['vertices']).reshape(-1, 9)[:, :3]
    camera = (vertices@vm[:3, :3].T+vm[:3, 3])*scale
    # Use the actual float-rounded input geometry and matrices, not the ideal
    # pre-quantization construction. No sampled GPU area/plane enters this oracle.
    root = (vm[:3, :3]@np.array(scene['root_body'])+vm[:3, 3])*scale
    # Rounding can make the two originally coplanar triangles differ slightly.
    # Integrate each actual triangle on its own plane, rather than flattening a quad.
    return np.sum([surface_bands(camera[start:start+3], root, camera[6:], projection)
                   for start in (0, 3)], axis=0).tolist()


def surface_bands(surface, root, foreground, projection):
    u = surface[1]-surface[0];u /= np.linalg.norm(u)
    n = np.cross(u, surface[2]-surface[0]);n /= np.linalg.norm(n)
    v = np.cross(n, u)
    root_plane = root-n*np.dot(root-surface[0], n)
    perpendicular = np.dot(root-surface[0], n)
    local = lambda p: np.column_stack(((p-root_plane)@u, (p-root_plane)@v))
    boundary = local(clip_frustum(surface, projection))
    shadow = np.empty((0, 2))
    if len(foreground):
        front = clip_frustum(foreground, projection)
        # Clip background by projected foreground edge halfplanes. Projecting
        # foreground vertices onto a grazing plane fails across its horizon.
        assert np.max(-front[:, 2]) < np.min(-clip_frustum(surface, projection)[:, 2])
        screen = front[:, :2]*np.diag(projection)[:2]/(-front[:, 2:3])
        shadow_camera = clip_frustum(surface, projection)
        for a, b in zip(screen, np.roll(screen, -1, axis=0)):
            edge = b-a
            equation = [-edge[1]*projection[0, 0], edge[0]*projection[1, 1], edge[0]*a[1]-edge[1]*a[0]]
            shadow_camera = clip_halfplane(shadow_camera, np.array(equation))
        shadow = local(shadow_camera)
    def disk(radius):
        r = math.sqrt(max(0, radius**2-perpendicular**2))
        return circle_polygon_area(boundary, r)-circle_polygon_area(shadow, r)
    return [disk(hi)-disk(lo) for lo, hi in BANDS]


def reference_checks():
    square = np.array([[-2., -2.], [2., -2.], [2., 2.], [-2., 2.]])
    assert abs(circle_polygon_area(square, 1)-math.pi) < 1e-12
    assert abs(circle_polygon_area(square, 10)-16) < 1e-12
    half = clip_halfplane(square, np.array([1., 0.]))
    assert abs(circle_polygon_area(half, 1)-math.pi/2) < 1e-12
    assert abs(circle_polygon_area(square[::-1], 1)-math.pi) < 1e-12
