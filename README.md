# sarah's video renderer

A 3D engine written from scratch in C++17, built so that **an AI can write
the scenes**, and you can walk around inside them while they play.

AI models are good at saying *what* should be in a scene ("a big cube, a
sphere next to it, a pyramid on top") but bad at picking *coordinates*:
things overlap, hide each other or end up off screen. So in this engine the
AI never writes numbers. It writes **dan**, a small scene language (`.dan` files) of objects,
relations and motions, and a **solver** works out where everything goes,
which paths moving things take so they never collide (unless they're meant
to), and where the camera should stand. A CPU software rasterizer then draws
it, live in a window you can walk around in.

```
# scenes/lazy.dan
cube        = cube big orange important
sphere      = sphere blue near cube
pyramid     = pyramid white above cube
torus       = torus teal left_of sphere
octahedron  = octahedron small grey near moon     # a mistake: there is no moon

# scenes/impact.dan
planet = octahedron red orbits sun 1 turn 0s-20s
comet  = pyramid small white
comet hits planet at 12s from 8s sticks
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

- **near-plane clipping** (Sutherland–Hodgman in clip space), so you can
  stand inside a scene

**Live player**
- a frame loop driven by a real-time clock (frame-rate independent)
- a timeline of timed changes (move, rotate, scale, orbit) that's replayed
  from the start each frame, so you can jump to any moment, and looping is free
- **walk around** while it plays: Tab, then WASD + mouse (yaw/pitch fly
  camera, level movement, the same speed in every direction)
- **parent/child links**: a ring follows its planet, a moon its planet, a
  comet that hit something rides along with it
- SDL3 is used **only** to open a window, read the keys and mouse, and show
  the finished image

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
  point-to-circle distances (moons included, through a planet's "reach"),
  fly-by lines checked with point-to-segment distances, and time sampling
  that provably can't miss a collision ([docs/16](docs/16-motion-placement.md))
- **intended collisions** (`hits ... at 12s`): intercepts a moving target
  exactly on time, only that pair may touch, then it sticks
  ([docs/18](docs/18-intended-collisions.md))
- a plain-text report of every overlap, hidden pair and relation verdict:
  the feedback the AI will read
- stress tests: 15 adversarial scenes (crowded, cyclic, contradictory,
  typo-ridden, empty, moving, colliding on purpose) and 135 automatic checks.
  They found 4 bugs, each fixed in its own commit ([docs/15](docs/15-stress-tests.md))

**The dan language** ([docs/21](docs/21-scene-language.md))
- hand-written lexer and recursive-descent parser, building the same
  structures the C++ scenes do (a test proves they're identical)
- every error in one go, with line, column and a **did you mean** from edit
  distance (Damerau–Levenshtein, optimal string alignment): the feedback the
  AI will get

`make test` runs both test programs: the solver's 135 checks and the
engine's 28 (clipping, the fly camera, looping, the parser).

## Build and run

Needs a C++17 compiler and SDL3 (`brew install sdl3`).

```
make run              scene 1: three objects animated over 12 s
make run SCENE=2      scene 2: a small solar system, 20 s
./main 3 framed       scene 3: the "lazy AI" scene above, fully solved
./main 3 greedy       ... or stopped after one step (naive, greedy, refined, framed)
./main 4              scene 4: orbits, a moon, and a fly-by, fully solved
./main 4 naive        ... or with naive paths (naive, orbits, flights, framed)
./main 5              scene 5: collisions that are meant to happen
./main scenes/impact.dan    any .dan file: edit and save it while it plays, it reloads live
./main scenes/broken.dan    ... or one full of mistakes, to see the error messages
./main clip on|off    standing inside a scene, with or without near-plane clipping
./main stress crowd   any of the stress test scenes (crowd, chain, cycle, typos, ...)
make test             all the tests

In any window: Tab = walk around (WASD, mouse, Space/Shift up/down, Ctrl faster), Esc = back.
make stills           regenerate the pictures and reports in docs/images/
```

## How it works

Everything is explained in **[docs/](docs/README.md)**, one page per part of
the pipeline. Each page has the idea, the code, and a worked example with
small numbers you can check on paper. One point is followed from the cube's
`.obj` file all the way to its pixel.

```
.dan text ─► parser ─► scene description ─► solver ───────► world ─► model → view → projection → clip → viewport → rasterize → depth + shade ─► window
                (21)        (11)              (12–14, 16, 18)     (17)     (03)   (04)     (05)      (19)     (06)       (07)        (08)          (10, 20)
```

## Layout

| File | What it is |
|---|---|
| `vec3`, `mat4.h`, `transform.h` | vectors, matrices, and every transform matrix |
| `mesh` | `.obj` loading, bounding radius |
| `render` | the rasterizer |
| `object`, `camera`, `timeline.h` | things in the scene, how they change over time, parent/child links |
| `player`, `window`, `fly_camera` | the frame loop, the SDL window and input, the walking camera |
| `scene_parser`, `scenes/` | the scene language and example scene files |
| `scene_spec.h` | how a scene is described (what the AI will produce) |
| `layout` | the layout solver for still objects |
| `motion` | paths for moving objects, and collision checks over time |
| `solver` | runs both: still first, then moving |
| `test_scenes`, `solver_test.cpp`, `engine_test.cpp` | the tests |
| `world` | turns a description + solved layout into objects and a camera |
| `pixel.h` | a small single-header image library (PNG/PPM output) |
| `shapes/` | test meshes |

## Next

- hooking up an AI to write `.dan` files, with the parser's errors and the
  solver's report sent back so it can fix its own scenes
- flying past (not just orbiting) things that move
- full parent/child transforms (spin and scale too) for things like wheels on
  a moving car

## How this was built

The idea, the design and the direction are mine. I used Claude (an AI
model) as a pair programmer for writing code and docs. Everything is
documented down to the arithmetic so I can explain and defend every part.
