# sarah's video renderer

A 3D engine written from scratch in C++17, built so that **an AI can write
the scenes**.

AI models are good at saying *what* should be in a scene ("a big cube, a
sphere next to it, a pyramid on top") but bad at picking *coordinates*:
things overlap, hide each other or end up off screen. So in this engine the
AI never writes numbers. It describes **objects and relations**, and a
**layout solver** works out where everything goes and where the camera
should stand. A CPU software rasterizer then draws it, live in a window.

```cpp
scene_spec spec;                    // (orange, blue, ... are px::Pixel colors)
spec.add("cube",    "shapes/cube.obj",    orange, size_word::big, 10);
spec.add("sphere",  "shapes/sphere.obj",  blue).near("cube");
spec.add("pyramid", "shapes/pyramid.obj", white).above("cube");
spec.add("torus",   "shapes/torus.obj",   teal).left_of("sphere");
spec.add("octahedron", "shapes/octahedron.obj", grey).near("moon");   // a mistake: there is no moon
```

| no solver | greedy | + refinement | + auto camera |
|---|---|---|---|
| ![](docs/images/scene3-naive.png) | ![](docs/images/scene3-greedy.png) | ![](docs/images/scene3-refined.png) | ![](docs/images/scene3-framed.png) |
| 36 overlapping pairs | 0 overlaps, 6 hidden on screen | 0 hidden | fits the frame |

The circles are each object's bounding sphere: red = overlapping something.

## What's in it

**Renderer** (no graphics libraries; every pixel is computed on the CPU)
- `.obj` mesh loading with validation
- model, view and perspective projection matrices, written out by hand
- back-face culling, bounding-box triangle setup, barycentric rasterization
- z-buffer, flat Lambert shading
- ~1 ms per frame for ~2000 triangles at 640×480

**Live player**
- a frame loop driven by a real-time clock (frame-rate independent)
- a timeline of timed changes (move, rotate, scale, orbit) that's replayed
  from the start each frame, so you can jump to any moment
- SDL3 is used **only** to open a window and show the finished image

**Layout solver** (the part that's being proven out)
- objects as bounding spheres; relations: `near`, `left_of`, `right_of`,
  `above`, `below`, `in_front_of`, `behind`; size words; importance
- dependency ordering (topological sort) with cycle and typo reporting
- greedy placement over candidate spots
- gradient-descent refinement with spring, overlap, relation and
  **screen-space visibility** terms, plus a backtracking line search
- automatic camera framing from a view word, tightened by a binary search on
  the projected size
- **moving objects** (`orbits`, `flies_past`): exact orbit radii from
  point-to-circle distances, fly-by lines checked with point-to-segment
  distances, and time sampling that provably can't miss a collision
  ([docs/16](docs/16-motion-placement.md))
- a plain-text report of every overlap, hidden pair and relation verdict:
  the feedback the AI will read
- stress tests (`make test`): 13 adversarial scenes (crowded, cyclic,
  contradictory, typo-ridden, empty, moving) and 91 automatic checks. They
  found 4 bugs, each fixed in its own commit ([docs/15](docs/15-stress-tests.md))

## Build and run

Needs a C++17 compiler and SDL3 (`brew install sdl3`).

```
make run              scene 1: three objects animated over 12 s
make run SCENE=2      scene 2: a small solar system, 20 s
./main 3 framed       scene 3: the "lazy AI" scene above, fully solved
./main 3 greedy       ... or stopped after one step (naive, greedy, refined, framed)
./main 4              scene 4: a "lazy AI" animation with orbits and a fly-by, fully solved
./main 4 naive        ... or with naive paths (naive, orbits, flights, framed)
./main stress crowd   any of the stress test scenes (crowd, chain, cycle, typos, ...)
make test             solve every stress scene with every step and check the rules
make stills           regenerate the pictures and reports in docs/images/
```

## How it works

Everything is explained in **[docs/](docs/README.md)**, one page per part of
the pipeline. Each page has the idea, the code, and a worked example with
small numbers you can check on paper. One point is followed from the cube's
`.obj` file all the way to its pixel.

```
scene description ─► layout solver ─► world ─► model → view → projection → viewport → rasterize → depth + shade ─► window
   (docs/11)          (docs/12–14)              (docs/03)  (04)     (05)        (06)       (07)        (08)          (10)
```

## Layout

| File | What it is |
|---|---|
| `vec3`, `mat4.h`, `transform.h` | vectors, matrices, and every transform matrix |
| `mesh` | `.obj` loading, bounding radius |
| `render` | the rasterizer |
| `object`, `camera`, `timeline.h` | things in the scene and how they change over time |
| `player`, `window` | the frame loop and the SDL window |
| `scene_spec.h` | how a scene is described (what the AI will produce) |
| `layout` | the layout solver for still objects |
| `motion` | paths for moving objects, and collision checks over time |
| `solver` | runs both: still first, then moving |
| `test_scenes`, `solver_test.cpp` | the stress tests |
| `world` | turns a description + solved layout into objects and a camera |
| `pixel.h` | a small single-header image library (PNG/PPM output) |
| `shapes/` | test meshes |

## Next

- a small text format and parser for scene descriptions, and hooking up an
  AI to write them, with the solver's report sent back as feedback
- orbits around moving objects (a moon around a planet), with parent/child
  transforms
- near-plane clipping, then walking around the scene with WASD + mouse
- looping playback

## How this was built

The idea, the design and the direction are mine. I used Claude (an AI
model) as a pair programmer for writing code and docs. Everything is
documented down to the arithmetic so I can explain and defend every part.
