#!/usr/bin/env python3
"""Exercise controlled live plans via the common native inspection fixture."""
import runpy
from pathlib import Path
import sys
import numpy as np
from measure import occlusion_depth
# Pixel-center ground is a metre farther away, but the same triangle plane.
# A root on/above it remains visible; behind-plane and parallel rays fail.
view = np.diag([1., 1., -1., 1.]); view[1, 3] = -2
points = np.array([[0, 0, 30], [0, .005, 30], [0, -.2, 30], [0, 2, 30]])
ground = np.tile([0., 0., 31.], (4, 1)); normals = np.tile([0., 1., 0.], (4, 1))
depth, valid = occlusion_depth(points, ground, normals, view, 1)
assert abs(depth[0]) < 1e-12 and depth[1] < 0 and depth[2] > .15 and not valid[3]
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.argv.append('--matched')
runpy.run_path(str(Path(__file__).resolve().parents[1] / 'test_inspection.py'), run_name='__main__')
