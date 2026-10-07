"""Analytic differential area on sampled live triangle planes (excluded analysis)."""
import numpy as np

BANDS = ((0, 5), (5, 15), (15, 30))


def differential_area(control, ground, plane):
    """Return eligible m² per pixel and ray-corrected root-relative distances.

    Matrices use the product's column-major convention and uniform body scale.
    The raster owns visibility/eligibility. This function neither fills missing
    silhouette samples nor infers a surface from scalar foliage counts.
    """
    width, height = control['viewport']
    assert ground.shape == plane.shape == (height, width, 4)
    assert np.isfinite(ground).all() and np.isfinite(plane).all()
    model, view, projection = [np.asarray(control[n], dtype=float).reshape(4, 4, order='F')
                               for n in ('model', 'view', 'projection')]
    linear = model[:3, :3]
    radius = np.linalg.norm(linear[:, 0])
    assert radius > 0 and np.allclose(linear.T @ linear, radius**2*np.eye(3), rtol=1e-5, atol=1e-8)
    assert np.allclose(projection[:2, 2:], 0) and projection[3, 2] == -1
    scale = control['meters_per_radius']
    root_body = np.asarray(control['root_body'], dtype=np.float32).astype(float)
    vm = view @ model
    camera_root = vm[:3, :3] @ root_body + vm[:3, 3]
    offset = ground[..., :3].astype(float)
    camera = offset @ vm[:3, :3].T/scale + camera_root
    normal = plane[..., :3].astype(float) @ vm[:3, :3].T/radius
    length = np.linalg.norm(normal, axis=2)
    normal = np.divide(normal, length[..., None], out=np.zeros_like(normal), where=length[..., None] > 0)
    x, y = np.meshgrid((2*(np.arange(width)+.5)/width-1)/projection[0, 0],
                       (2*(np.arange(height)+.5)/height-1)/projection[1, 1])
    ray = np.stack((x, y, -np.ones_like(x)), axis=2)
    divisor = np.sum(normal*ray, axis=2)
    support = (plane[..., 3] > 0) & (np.abs(divisor) > 1e-12)
    depth = np.divide(np.sum(camera*normal, axis=2), divisor, out=np.zeros_like(x), where=support)
    support &= depth > 0
    jacobian = np.divide(4*(depth*scale/radius)**2,
                         width*height*abs(projection[0, 0]*projection[1, 1])*np.abs(divisor),
                         out=np.zeros_like(depth), where=support)
    retention = np.divide(ground[..., 3], plane[..., 3], out=np.zeros_like(depth), where=plane[..., 3] > 0)
    assert np.all((retention >= 0) & (retention <= 1+1e-5))
    # Correct sampled interpolation to the original full-viewport center ray.
    delta = (ray*depth[..., None]-camera) @ np.linalg.inv(vm[:3, :3]).T*scale
    distance = np.linalg.norm(offset+delta, axis=2)
    return jacobian*np.clip(retention, 0, 1), distance


def band_areas(area, distance):
    return [float(area[(distance >= lo) & (distance < hi)].sum()) for lo, hi in BANDS]
