# 16 · Motion placement (solver step E)

Steps A–D place things that **stand still**. But these scenes are
animations, and an AI writes motion just as loosely as it writes
positions: "planet orbits sun", "comet flies past sun". It doesn't say
how far out, along which line, or what's in the way.

Step E gives every moving object a **path** and checks the paths **over
time**, so nothing passes through anything at any moment.

Code: `motion.h/.cpp` (paths, the planner, time sampling), `solver.h/.cpp`
(still + moving together), `scene_spec.h` (`orbits`, `flies_past`).

---

## 1. What the AI writes

```cpp
spec.add("sun",     "shapes/sphere.obj", yellow, size_word::big, 10);
spec.add("planet1", "shapes/octahedron.obj", red).orbits("sun", 1.0f, 0.0f, 20.0f);   // 1 turn, 0 → 20 s
spec.add("comet",   "shapes/pyramid.obj", white).flies_past("sun", 4.0f, 10.0f);     // 4 → 10 s
```

| Motion | What the solver decides |
|---|---|
| `orbits(X, turns, start, end)` | the circle's radius, and where on it to start |
| `flies_past(X, start, end)` | the straight line: which side of X, how far |

## 2. Still first, then moving

```
the description
   │  split_motion: who moves?
   ├──► still objects ──► layout (steps A–D) ──► positions      ─┐
   └──► moving objects ─────────────────────────► motion_plan ───┴─► paths
```

A moving object needs something that **stands still** to move around.
`split_motion` checks every motion, and anything it can't use is reported
while that object stands still instead:

| Problem | Message (shortened) |
|---|---|
| unknown name | `there is no object called "sunn" (it stands still instead)` |
| around itself | `it can't move around itself` |
| end ≤ start | `it ends before it starts` |
| around something that **moves** | `moving around something that moves isn't supported yet` |
| a still object placed relative to a mover | `comet moves, so it can't be used to place things (relation ignored)` |

The fourth one is the moon orbiting a planet that orbits the sun. It needs
**parent/child transforms**: the moon's path is relative to the planet,
whose path is relative to the sun. That's a later feature, so for now it's
reported clearly instead of half-working.

## 3. A path's position at time t

Both kinds use the progress `f` from 09: 0 before `start`, 1 after `end`,
a straight line in between.

**Orbit:** a circle in the horizontal plane around the center c:

```
angle = start_angle + 2π · turns · f
p(t)  = c + R · ( cos(angle), 0, −sin(angle) )
```

That's exactly what `rotate_y` (03) does to the point (R, 0, 0), so when
`world` animates it with `object::rotate_around`, the object moves along
precisely the circle the solver checked.

Example: planet1, R = 2.6, 1 turn over 0 → 20 s, start angle 0. At t = 5:

```
f = 5/20 = 0.25,  angle = 2π · 0.25 = 90°
p = (2.6 · cos 90°, 0, −2.6 · sin 90°) = (0, 0, −2.6)      a quarter turn: behind the sun
```

**Fly-by:** a straight line from `from` to `to`:

```
p(t) = lerp(from, to, f)                  (09)
```

## 4. Checking over time: how often is enough?

Collisions are found by looking at the scene at many moments ("samples").
The danger: two objects could pass through each other **between** two
samples, and nobody would notice. So how far apart can the samples be?

**The argument:**

1. Let `v_max` be the fastest any object moves. In a time `dt`, nothing
   moves more than `v_max · dt`.
2. So the distance between two objects changes by at most `2 · v_max · dt`
   in that time (both could move straight towards each other).
3. Every moment is at most `dt/2` from a sample. So at the nearest sample,
   the distance is off by at most `2 · v_max · dt/2 = v_max · dt`.
4. Choose
   ```
   dt = gap / (4 · v_max)        →   off by at most gap/4
   ```
5. A sample flags a pair whenever they're **closer than (r₁ + r₂ + gap/4)**.

If two objects overlap at any moment (distance < r₁ + r₂), then at the
nearest sample their distance is less than r₁ + r₂ + gap/4, so they're
flagged. **No collision can slip between samples.** The price: a near miss
(less than gap/4 = 0.1 of empty space) is flagged too, and that's fine
because we want some space anyway.

**Scene 4's numbers** (from the report):

```
comet:   from (−7.53, 0, 0) to (7.53, 0, 0) in 6 s  →  15.06 / 6 = 2.51 per second   (the fastest)
planet2: radius 2.67, 2 turns in 20 s               →  2π · 2.67 · 2 / 20 = 1.68 per second
dt = 0.4 / (4 · 2.51) = 0.0398 s                    →  about 500 samples over 20 s
```

The report says `checked every 0.0398 s (fastest moves 2.5107 per second)`. ✓

### Worked check: when does the comet hit the sun?

The comet (r = 0.87) goes along x: x(t) = −7.53 + 15.06 · (t − 4)/6. It's
flagged once its distance to the sun's center (r = 1.4) is below
0.87 + 1.4 + 0.1 = 2.37:

```
−7.53 + 2.51 · (t − 4) = −2.37    →    t − 4 = 5.16 / 2.51 = 2.057    →    t = 6.057 s
```

The first sample after that is 153 · 0.0398 = 6.09 s, and the report says
`comet hits sun, first at 6.09 s`. ✓

## 5. Step E1: the naive paths

The simplest paths, checking nothing:

- **orbit:** as small as possible without touching what it goes round:
  `R = r_object + r_center + gap`, starting on the +x side
- **fly-by:** straight along x, right through the middle of what it
  passes

Scene 4 (`./main 4 naive`, `lazy_motion_scene` in `test_scenes.cpp`):

- a sun, and a **rock** near it (greedy puts it at (2.6, 0, 0))
- **planet1** and **planet2** orbiting the sun (1 and 2 turns), with no
  distance given
- a **comet** flying past the sun from 4 s to 10 s
- a **moon** orbiting planet1, which isn't supported yet

The report ([scene4-naive.txt](images/scene4-naive.txt)):

```
motion (naive): 3 moving objects, 9 colliding pairs
    planet1    orbits sun, 0.00-20.00 s: radius 2.60, 1.00 turns
    planet2    orbits sun, 0.00-20.00 s: radius 2.67, 2.00 turns
    comet      flies_past sun, 4.00-10.00 s: from (-7.53, 0.00, 0.00) to (7.53, 0.00, 0.00)
  collisions:
    planet1 hits rock, first at 0.00 s (closest: inside each other by 1.60)
    planet1 hits planet2, first at 0.00 s (closest: inside each other by 1.60)
    comet hits sun, first at 6.09 s (closest: inside each other by 2.24)
    ...
  problems with motions in the description:
    moon orbits planet1: planet1 moves too, and moving around something that moves isn't supported yet (it stands still instead)
```

Planet1's smallest orbit, 1.4 + 0.8 + 0.4 = 2.6, is exactly where greedy put
the rock: they start **in the same place**. Both planets' orbits are nearly
the same size, so they're on top of each other at the start too, and
planet2 laps planet1. The comet flies straight through the sun.

| 0 s | 3 s | 7 s | 12 s |
|---|---|---|---|
| ![](images/scene4-naive-0.png) | ![](images/scene4-naive-3.png) | ![](images/scene4-naive-7.png) | ![](images/scene4-naive-12.png) |

The circles now go red **in whichever frame** two spheres overlap
(`render::finish` checks every pair each frame), so collisions blink red as
they happen. At 0 s the rock and both planets are one red tangle; at 7 s
the comet is inside the sun.

The camera only frames the still objects, so moving ones can leave the
picture. That gets fixed in E4.

---

## Try it on paper

Planet2 (R = 2.67, 2 turns over 0 → 20 s, start angle 0). Where is it at
t = 2.5 s?

Answer: f = 0.125, angle = 2π · 2 · 0.125 = 90°, so p = (0, 0, −2.67).
Planet1 at that time: f = 0.125, angle = 45°, p = (1.84, 0, −1.84). Distance
between them = √(1.84² + 0.83²) = 2.02, more than 0.8 + 0.87 = 1.67. At this
moment they don't touch: planet2 has pulled ahead.
