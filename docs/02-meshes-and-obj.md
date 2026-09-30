# 02 · Meshes and .obj files

A **mesh** is a shape made of flat triangles. It's stored as two lists:

- **vertices**: the corner points
- **faces**: triangles, each one says "connect corners number i, j, k"

Corners are shared: the cube has 8 corners but 12 triangles, and each corner
is used by several triangles.

Code: `mesh.h/.cpp`. Shapes: `shapes/*.obj`.

---

## The .obj file format

A plain text format from the 1990s that almost every 3D program can read
and write. Here's the whole `shapes/cube.obj`:

```
v -1 -1 -1      ← vertex 1
v -1 -1  1      ← vertex 2
v -1  1 -1      ← vertex 3
v -1  1  1      ← vertex 4
v  1 -1 -1      ← vertex 5
v  1 -1  1      ← vertex 6
v  1  1 -1      ← vertex 7
v  1  1  1      ← vertex 8
f 2 6 8         ← triangle: vertices 2, 6, 8
f 2 8 4
...             (12 lines of f in total)
```

- lines starting with `v` = one vertex (x, y, z)
- lines starting with `f` = one triangle (three vertex numbers)
- **numbers start at 1**, not 0, so the code subtracts 1 before using them
  as an index (`render.cpp`: `int index1 = t[0]-1;`)
- `f 1/4/7 ...` style lines also exist (texture/normal numbers after the
  `/`); the loader keeps only the number before the first `/`

All shapes in `shapes/` are **centered at (0, 0, 0)** and fit inside -1..1.
That matters: rotating and scaling happen around (0, 0, 0), so a centered
shape turns in place (see 03).

## Winding: which side is the outside?

The three corners of a face are listed **counter-clockwise when seen from
outside**. Take `f 2 6 8`:

```
vertex 2 = (-1, -1, 1)
vertex 6 = ( 1, -1, 1)
vertex 8 = ( 1,  1, 1)       all have z = 1: this is the front face
```

Seen from the front (from +z looking back), 2 → 6 → 8 goes bottom-left →
bottom-right → top-right: counter-clockwise. ✓

The **normal** (the arrow sticking straight out of the face) comes from
the cross product of two edges:

```
edge1 = v6 - v2 = (2, 0, 0)
edge2 = v8 - v2 = (2, 2, 0)
cross(edge1, edge2) = (0·0 - 0·2,  0·2 - 2·0,  2·2 - 0·2) = (0, 0, 4)
normalize → (0, 0, 1)      points out of the front, towards +z ✓
```

If the file listed them clockwise, the normal would point **into** the cube.
The winding is used twice later: to skip faces pointing away from the camera
(07) and to work out brightness (08).

## What the loader checks

`mesh::mesh(file)` reads the file line by line and throws a clear error if:

- the file can't be opened
- a face has fewer than 3 numbers
- a face uses a vertex number that doesn't exist (e.g. `f 1 2 99` in an
  8-vertex file)

Failing loudly at load time is much better than a crash later during
drawing. That will matter a lot when an AI writes the scenes.

## Bounding radius (used by the solver)

The **bounding radius** is the distance from the center (0, 0, 0) to the
farthest vertex. A sphere with that radius holds the whole shape, however
it's rotated.

```
radius = max over all vertices of  length(v)
```

For the cube, every corner is like (1, 1, 1):

```
length = √(1 + 1 + 1) = √3 ≈ 1.732
```

| Shape | Bounding radius |
|---|---|
| cube | √3 ≈ 1.732 |
| sphere, torus, octahedron, pyramid, cone, cylinder | computed when loaded (printed in the scene report) |

When an object is drawn at size s, its radius is `s · radius`. A cube at
size 2 needs a sphere of radius 3.46.

Code: `mesh::bounding_radius()`, computed once in the constructor. More in
[11](11-scene-description.md).

---

## Try it on paper

1. `f 1 5 6` is the bottom face (y = -1). Work out its normal with
   cross(v5 - v1, v6 - v1). Which way should it point?
2. What's the bounding radius of a shape whose farthest vertex is (0, 2, 0)?

Answers: 1. v1 = (-1,-1,-1), v5 = (1,-1,-1), v6 = (1,-1,1). edge1 = (2,0,0),
edge2 = (2,0,2). cross = (0·2 - 0·0, 0·2 - 2·2, 2·0 - 0·2) = (0, -4, 0) →
(0, -1, 0), pointing down, out of the bottom. ✓ · 2. 2
