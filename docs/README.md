# How the engine works

These pages explain every part of the engine: the idea, the math, the code,
and a **worked example with small numbers** you can do on paper with a
calculator. Read them in order the first time; each part uses the one before.

## The pipeline

```
 SCENE                     SOLVER                                 RENDERER (every frame)
 ─────                     ──────                                 ──────────────────────
 .dan text               11 scene description                   03 model transform   (shape → world)
 (written by AI)           12 greedy placement       still        04 camera view       (world → camera)
   │                       13 refinement             objects      05 projection        (camera → clip)
   └─► 21 parser ─────────►14 auto camera                         19 near-plane clip   (cut what's behind)
                           16 motion paths           moving       06 viewport          (NDC → pixels)
                           18 intended collisions    objects      07 rasterization     (which pixels)
                           15 stress tests (checking all of it)   08 depth + shading   (in front? how bright?)
                                    │
                                    ▼
                    world: objects + parent/child links (17) + timelines (09)
                                    │
                                    ▼
                    10 live player + window, 20 walking around + looping
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
| 11 | [Scene description](11-scene-description.md) | How does the AI describe a scene? What's the baseline? | `scene_spec.h`, `layout.cpp`, `world.cpp` |
| 12 | [Greedy placement](12-greedy-placement.md) | How do relations become positions? | `layout.cpp` |
| 13 | [Refinement](13-refinement.md) | How is the layout polished? (gradient descent) | `layout.cpp` |
| 14 | [Auto camera](14-auto-camera.md) | Where should the camera go? | `layout.cpp`, `world.cpp` |
| 15 | [Stress tests](15-stress-tests.md) | Does the solver survive sloppy input? What broke? | `test_scenes.cpp`, `solver_test.cpp` |
| 16 | [Motion placement](16-motion-placement.md) | How do moving things avoid each other? | `motion.cpp`, `solver.cpp` |
| 17 | [Parent and child](17-parent-child.md) | How does a ring follow its planet? | `object.cpp` |
| 18 | [Intended collisions](18-intended-collisions.md) | What if things are meant to hit? | `motion.cpp` |
| 19 | [Near-plane clipping](19-near-plane-clipping.md) | What if a triangle reaches behind the camera? | `render.cpp` |
| 20 | [Walking around (and looping)](20-controls-and-looping.md) | How do WASD and the mouse work? | `fly_camera.cpp`, `window.cpp`, `player.cpp` |
| 21 | [The dan language](21-scene-language.md) | How does text become a scene? How are mistakes reported? | `scene_parser.cpp`, `scenes/` |
| 22 | [Live reload](22-live-reload.md) | How does editing the file update the window? What does the AI read? | `live_scene.cpp`, `frame_source.h` |
| 23 | [Easing](23-easing.md) | How do things speed up and slow down? | `timeline.h`, `motion.cpp` |
| 24 | [Fades](24-fades.md) | How do things fade? Why is see-through hard? | `render.cpp`, `object.cpp` |
| 25 | [Anti-aliasing and video](25-antialiasing-and-video.md) | Why are edges jagged? How is an mp4 made? | `render.cpp`, `main.cpp` |
| 26 | [Lines and arrows](26-lines-and-arrows.md) | How is a smooth arrow drawn, and hidden behind things? | `render.cpp`, `world.cpp` |
| 27 | [Text](27-text.md) | How are letters stored and drawn? | `font8x8.h`, `render.cpp` |
| 28 | [Labels](28-labels.md) | How do words sit next to objects without overlapping or flickering? | `label_layout.cpp`, `world.cpp` |
| 29 | [Outline fonts](29-outline-fonts.md) | How does a font file become smooth letters? | `font.cpp` |
| 30 | [Math](30-math.md) | How does our small TeX lay out a formula? | `math_layout.cpp` |
| 31 | [Room for words](31-room-for-words.md) | How does the solver leave room for titles and labels? | `layout.cpp`, `solver.cpp` |
| 32 | [Flying past movers](32-flyby-movers.md) | How does something fly past a thing that is itself moving? | `motion.cpp`, `solver.cpp` |
| 33 | [Framing by sampling](33-framing-by-sampling.md) | How does the camera fit where moving things really are? | `motion.cpp` |
| 34 | [Any size, and HD](34-hd.md) | How does a scene become a 1080p picture without changing its layout? | `world.cpp`, `render.cpp` |
| 35 | [Mixing as light](35-linear-light.md) | Why does mixing bytes make edges too dark, and how is it fixed? | `srgb.h`, `render.cpp` |
| 36 | [Smooth shading](36-smooth-shading.md) | How do curved shapes look round while cubes stay sharp? | `mesh.cpp`, `render.cpp` |
| 37 | [Vector paths](37-vector-paths.md) | How do letters become outlines that can be animated? | `vpath.cpp`, `render.cpp` |
| 38 | [Write](38-write.md) | How does a formula draw itself in, letter by letter? | `vpath.cpp`, `world.cpp` |
| 39 | [Transform](39-transform.md) | How does one formula turn into another, with matching letters gliding? | `vpath.cpp`, `world.cpp` |
| 40 | [Flat shapes](40-flat-shapes.md) | How can a drawing look 2D from the camera but be 3D when you walk round it? | `shapes2d.cpp`, `render.cpp` |
| 41 | [Create](41-create.md) | How does a shape draw itself in along its outline? | `shapes2d.cpp`, `object.cpp` |
| 42 | [Graphs](42-graphs.md) | How does "sin(x)" become axes, ticks and a curve? | `expression.cpp`, `shapes2d.cpp` |
| 43 | [Morph](43-morph.md) | How does a square melt into a circle without twisting? | `shapes2d.cpp`, `object.cpp` |
| 44 | [Draw-in](44-draw-in.md) | How does a 3D object trace its edges and then fill in? | `mesh.cpp`, `object.cpp` |
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
./main 3 <step>       scene 3: a "lazy AI" scene, placed by one step of the solver
./main 4 <step>       scene 4: a "lazy AI" animation, orbits and a fly-by (16)
./main 5              scene 5: collisions that are meant to happen (18)
./main scenes/x.dan   a .dan file (21); save it again while it plays and it reloads (22)
./main clip on|off    standing inside a scene, with or without clipping (19)
./main walk <prefix>  pictures of walking round scene 3 with pretend keys (20)
                      in any window: Tab = walk around (WASD + mouse), Esc = back
./main stress <name>  one of the stress test scenes (15)
make test             solve every stress scene, check the rules
make stills           writes the solver pictures in docs/images/
```
