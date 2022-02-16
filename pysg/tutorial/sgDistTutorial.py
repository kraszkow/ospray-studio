#Copyright 2021-2022 Intel Corporation
#SPDX-License-Identifier: Apache-2.0

import sys, numpy
from mpi4py import MPI
import pysg as sg
from pysg import Any, vec3f, Data, vec2i

sg.init(sys.argv)

mpiRank = 0
mpiWorldSize = 1
comm = MPI.COMM_WORLD
mpiRank = comm.Get_rank()
mpiWorldSize = comm.Get_size()

# specific rkcommon::math types needed for SG
pos = Any(vec3f((mpiWorldSize + 1.0) / 2.0, 0.5, -mpiWorldSize * 0.5))
dir = Any(vec3f(0.0, 0.0, 1.0))
up = Any(vec3f(0.0, 1.0, 0.0))


W = 1024
H = 768

window_size = Any(vec2i(W, H))
aspect = Any(float(W) / H)

vertex = numpy.array([
   [mpiRank, 0.0, 3.5],
   [mpiRank, 1.0, 3.0],
   [1.0 *(mpiRank + 1), 0.0, 3.0],
   [1.0 * (mpiRank + 1), 1.0, 2.5]
], dtype=numpy.float32)

color = numpy.array([
    [0.0, 0.0, 1.0, 1.0],
    [0.0, 0.0, 1.0, 1.0],
    [0.0, 0.0, 1.0, 1.0],
    [0.0, 0.0, 1.0, 1.0]
], dtype=numpy.float32)


index = numpy.array([
    [0, 1, 2], [1, 2, 3]
], dtype=numpy.uint32)


mat = numpy.array([0], dtype=numpy.uint32)

frame = sg.Frame()
frame.immediatelyWait = True
frame.createChild("windowSize", "vec2i", window_size)
renderer = sg.Renderer("mpiRaycast")
frame.createChildAs("renderer", "renderer_mpiRaycast")
world = frame.child("world")

cam = frame.child("camera")
cam.createChild("aspect", "float", aspect)
cam.createChild("position", "vec3f", pos)
cam.createChild("direction", "vec3f", dir)
cam.createChild("up", "vec3f", up)

transform = world.createChild("xfm", "transform")
geom = transform.createChild("mesh", "geometry_triangles")
geom.createChildData("vertex.position", Data(vertex))
geom.createChildData("vertex.color", Data(color))
geom.createChildData("index", Data(index))

lightsMan = sg.LightsManager()
lightsMan.addLight("ambientlight", "ambient")
lightsMan.updateWorld(world)

world.render()

# First frame will be "navigation" resolution.
# Render again for full sized frame.
frame.startNewFrame()
frame.startNewFrame()
frame.waitOnFrame()

if mpiRank == 0:
   frame.saveFrame("sgDistTutorial.png", 0)
