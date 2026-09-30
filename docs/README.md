# How the engine works

These pages explain every part of the engine: the idea, the math, the code,
and a **worked example with small numbers** you can do on paper with a
calculator. Read them in order the first time; each part uses the one before.

## The pipeline

```
 SCENE                 PLACEMENT SOLVER                      RENDERER (every frame)
 ─────                 ────────────────                      ──────────────────────
 what exists,     ┌─► 11 scene description ─┐          ┌─► 03 model transform   (shape → world)
 how it relates   │   12 greedy placement   │          │   04 camera view         (world → camera)
 (written by AI)──┘   13 refinement         ├─► world ─┤   05 projection          (camera → clip → NDC)
                      14 auto camera        │          │   06 viewport            (NDC → pixels)
                                            │          │   07 rasterization       (which pixels a triangle covers)
                                            │          │   08 depth + shading     (what's in front, how bright)
                                            │          │
                      09 time + animation ──┘          └─► 10 live player + window (show it, ~120×/second)
```

Every picture is made from **meshes** (02): lists of corners and triangles.
All the math is built on **vectors and matrices** (01).

## The parts

| # | Part | The question it answers | Code |
|---|---|---|---|
| 01 | [Vectors and matrices](01-vectors-and-matrices.md) | What are the basic tools? | `vec3.h`, `mat4.h` |
| 02 | [Meshes and .obj files](02-meshes-and-obj.md) | How is a shape stored? | `mesh.h/.cpp`, `shapes/` |
| 03 | [Model transform](03-model-transform.md) | How is a shape sized, turned and placed? | `transform.h`, `object.cpp` |
| 04 | [Camera view](04-camera-view.md) | What does the world look like from the camera? | `transform.h` `look_at`, `camera.h` |
| 05 | [Projection](05-projection.md) | Why do far things look small? | `transform.h` `perspective` |
| 06 | [Viewport](06-viewport.md) | Where on the image does it land? | `transform.h` `viewport` |
| 07 | [Rasterization](07-rasterization.md) | Which pixels does a triangle cover? | `render.cpp` `fill` |
| 08 | [Depth and shading](08-depth-and-shading.md) | What's in front? How bright is it? | `render.cpp` `fill` |
| 09 | [Time and animation](09-time-and-animation.md) | How do things move over time? | `timeline.h`, `object.cpp`, `camera.cpp` |
| 10 | [Live player and window](10-live-player-and-window.md) | How does it become a live video? | `player.cpp`, `window.cpp` |
| | [Ranges and settings](ranges.md) | What values make sense? | `camera.h`, `object.h` |
| | [C++ notes](cpp-notes.md) | What does this syntax mean? | everywhere |
| | [Design](design.md) | Why is the code split up like this? | everywhere |

## The running example

Parts 03 → 08 follow **one point** all the way from a shape file to a pixel,
so you can check every step on paper:

```
the cube's corner (1, 1, 1)
  03  size 2, turned 90° on y, moved to (1, 0, -3)   →  world    (3, 2, -5)
  04  camera at (0, 0, 4) looking at (0, 0, 0)        →  camera   (3, 2, -9)
  05  fov 50°, 640×480                                →  clip     (4.825, 4.289, 8.818, 9)
                                                      →  NDC      (0.536, 0.477, 0.980)
  06  image 640×480                                   →  pixel    (491.6, 125.6)
```

Every number in these docs was checked against what the engine actually
computes.

## Running things

```
make run              scene 1: cube, sphere, torus (12 s video)
make run SCENE=2      scene 2: solar system (20 s)
```
