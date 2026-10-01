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

`split_motion` checks every motion. Anything it can't use is reported, and
that object stands still instead:

| Problem | Message (shortened) |
|---|---|
| unknown name | `there is no object called "sunn" (it stands still instead)` |
| around itself | `it can't move around itself` |
| end ≤ start | `it ends before it starts` |
| motions in a circle (a orbits b, b orbits a) | `the motions go round in a circle` |
| flying **past** something that moves | `flying past something that moves isn't supported yet` |
| a still object placed relative to a mover | `comet moves, so it can't be used to place things (relation ignored)` |

**Orbiting** something that moves is fine: a moon around a planet that goes
round the sun. The moon's circle travels along with its planet (17). (The
first version of step E reported it as "not supported yet", which is what
the stress test `motion_typos` still checks for the cases above.)

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
- a **moon** orbiting planet1 (3 turns)
- a **comet** flying past the sun from 4 s to 10 s

The report ([scene4-naive.txt](images/scene4-naive.txt)):

```
motion (naive): 4 moving objects, 8 colliding pairs
    planet1    orbits sun, 0.00-20.00 s: radius 2.60, 1.00 turns
    planet2    orbits sun, 0.00-20.00 s: radius 2.67, 2.00 turns
    comet      flies_past sun, 4.00-10.00 s: from (-7.53, 0.00, 0.00) to (7.53, 0.00, 0.00)
    moon       orbits planet1 (which moves), 0.00-20.00 s: radius 1.50, 3.00 turns
  collisions:
    planet1 hits rock, first at 0.00 s (closest: inside each other by 1.60)
    planet1 hits planet2, first at 0.00 s (closest: inside each other by 1.60)
    planet2 hits rock, first at 0.00 s (closest: inside each other by 1.60)
    planet2 hits moon, first at 1.08 s (closest: inside each other by 0.35)
    moon hits sun, first at 3.86 s (closest: inside each other by 0.60)
    planet2 hits comet, first at 5.26 s (closest: inside each other by 0.31)
    comet hits sun, first at 6.09 s (closest: inside each other by 2.24)
    comet hits rock, first at 7.37 s (closest: inside each other by 1.64)
```

Planet1's smallest orbit, 1.4 + 0.8 + 0.4 = 2.6, is exactly where greedy put
the rock, so they start **in the same place**. Both planets' orbits are nearly
the same size, so they're on top of each other at the start too, and planet2
laps planet1. The moon's circle (1.5 around planet1) swings it **into the sun**
whenever it's on the inner side. The comet flies straight through the sun.

| 0 s | 3 s | 7 s | 12 s |
|---|---|---|---|
| ![](images/scene4-naive-0.png) | ![](images/scene4-naive-3.png) | ![](images/scene4-naive-7.png) | ![](images/scene4-naive-12.png) |

The circles now go red **in whichever frame** two spheres overlap
(`render::finish` checks every pair each frame), so collisions blink red as
they happen. At 0 s the rock and both planets are one red tangle; at 7 s
the comet is inside the sun.

The camera only frames the still objects, so moving ones can leave the
picture: all 4 of them do at some point. That gets fixed in E4.

---

## 6. Step E2: orbit radii

An orbit is a whole **circle**, so it doesn't matter where the planet is
right now: if the circle passes too close to something, the planet will hit it
sooner or later. So the radius is picked once, for the whole circle.

### How close does a circle come to a point?

Orbit: center c, radius R, flat (horizontal). A still object at o.
Measure o from c:

```
h = horizontal distance from c to o   (x and z only)
v = height of o above c               (y only)
```

The circle's closest point to o is the one straight "out" towards it, so
in the plane through c, o and that point:

```
closest distance = √( (h − R)² + v² )
```

It must be at least `need = r + r_o + gap`. Square both sides and solve for
R. The circle is too close exactly when

```
|h − R| < w,      w = √(need² − v²)              (if need ≤ |v|, never too close)
```

so each still object **rules out every radius between h − w and h + w**.

**Two orbits round the same center**, with radii R and R_q: every point on one
circle is at least |R − R_q| from the other circle. They can never meet if
|R − R_q| ≥ r + r_q + gap, so an existing orbit rules out R_q − need ..
R_q + need.

**The pick:** the smallest R that is at least `r + r_center + gap` and not
inside any ruled-out range. The answer is always either that minimum or
the top end of some range, so only those values need to be tried.

### Moons: an object's reach

A planet with a moon is, for planning **its own** orbit, a bigger object: the
moon's circle reaches `R_moon + r_moon` from the planet's center. So in all
the formulas above, **r is the object's reach**:

```
reach(planet1) = max(r_planet1,  R_moon + reach(moon)) = max(0.8, 1.5 + 0.3) = 1.8
```

If the planet's orbit keeps its whole reach clear of everything, the moon
can't hit anything either, so it's still exact and needs no sampling. That's why
moons are planned **first** (deepest first): a planet's reach depends on its
moons' radii.

### Worked example: scene 4

Sun r = 1.4. The rock (r = 0.8) is at h = 2.6, v = 0, so w = need.

**The moon** (r = 0.3) goes round planet1 (r = 0.8). Nothing still can be in
its way (its circle moves with the planet), so it gets the smallest radius:
0.3 + 0.8 + 0.4 = **1.5**. That makes planet1's reach **1.8**.

**Planet1** (reach 1.8). Smallest radius: 1.8 + 1.4 + 0.4 = **3.6**.

| Rules out | need | range |
|---|---|---|
| rock | 1.8 + 0.8 + 0.4 = 3.0 | 2.6 − 3.0 .. 2.6 + 3.0 = **−0.4 .. 5.6** |

Try 3.6: inside ✗. Try 5.6: on the edge, allowed ✓ → **R = 5.6**.

**Planet2** (r = 0.87, no moons, so reach 0.87). Smallest: 0.87 + 1.4 + 0.4 = **2.67**.

| Rules out | need | range |
|---|---|---|
| rock | 0.87 + 0.8 + 0.4 = 2.07 | 0.53 .. 4.67 |
| planet1's orbit (5.6, reach 1.8) | 0.87 + 1.8 + 0.4 = 3.07 | 2.53 .. **8.67** |

Try 2.67 ✗ (both), 4.67 ✗ (planet1's orbit), 8.67 ✓ → **R = 8.67**.

The report ([scene4-orbits.txt](images/scene4-orbits.txt)):

```
motion (orbits): 4 moving objects, 4 colliding pairs
    planet1    orbits sun, 0.00-20.00 s: radius 5.60, 1.00 turns
    planet2    orbits sun, 0.00-20.00 s: radius 8.67, 2.00 turns
    moon       orbits planet1 (which moves), 0.00-20.00 s: radius 1.50, 3.00 turns
  collisions:
    comet hits sun, first at 6.06 s ...
    comet hits rock, first at 7.35 s ...
    planet2 hits comet, first at 9.83 s ...
    comet hits moon, first at 19.65 s ...
```

**8 → 4 colliding pairs**, and every one that's left involves the comet,
which still flies straight through (E3).

### Orbits around different centers

The rules above compare an orbit with still objects and with other orbits
**around the same center**. Two orbits around different centers can still
cross (the `motion_typos` stress test has one). Crossing isn't the problem;
being at the crossing **at the same time** is. So afterwards, any orbit that
still collides with another orbit (by sampling) is repaired:

1. start it somewhere else on its circle: 45°, 90°, ... (the circle stays
   the same, so nothing still can be in the way)
2. if no start works, make the circle 15% bigger and try again, now checking
   the still objects by sampling too

The report says what it changed, e.g. `b's orbit crossed another moving
object's path; it now starts at 45.00 degrees`.

| 0 s | 3 s | 7 s | 12 s |
|---|---|---|---|
| ![](images/scene4-orbits-0.png) | ![](images/scene4-orbits-3.png) | ![](images/scene4-orbits-7.png) | ![](images/scene4-orbits-12.png) |

---

## 7. Step E3: fly-by paths

A fly-by is a straight segment. The naive one went right through the
middle of the sun. Now the planner tries lines that **pass** it, in this
order:

```
sides:      in front (+z, towards the usual camera), above, below, behind
distances:  D = r_comet + r_sun + gap,  then ×1.5, ×2, ×3, ×4
```

and takes the first line that's clear of **both** kinds of things.

### Against still objects: exact

The closest point of a segment A → B to a point q:

```
s       = clamp( dot(q − A, B − A) / |B − A|² ,  0, 1 )     how far along the segment (0 = A, 1 = B)
closest = A + s · (B − A)
```

`dot(q − A, B − A) / |B − A|²` is the projection from 01: how far along the
line q is. Clamping keeps it on the segment, so past the ends, the end
itself is the closest point. The distance from `closest` to q must be at
least `r + r_q + gap`.

Example: the rock at q = (2.6, 0, 0), and the "above" line from
A = (−7.53, 2.67, 0) to B = (7.53, 2.67, 0):

```
B − A = (15.06, 0, 0),   q − A = (10.13, −2.67, 0)
s = (10.13 · 15.06) / 15.06² = 10.13 / 15.06 = 0.673
closest = (−7.53 + 0.673 · 15.06, 2.67, 0) = (2.6, 2.67, 0)      straight above the rock
distance = 2.67 ≥ need = 0.87 + 0.8 + 0.4 = 2.07   ✓ clear
```

### Against moving objects: sampling

Both things move, so there's no single formula. The candidate is checked
by time sampling, exactly as in section 4, over the whole video. That
includes the time **before** the fly-by starts and **after** it ends,
because the comet waits at its start and end points then, and something
could run into it there.

### Worked example: why not the line in front?

The first candidate is in front of the sun: z = 2.67, y = 0. Against the
still objects it's fine (the same numbers as above, just sideways). During
the flight itself (4–10 s), nothing gets in its way either. But the comet
**waits at its end point** (7.53, 0, 2.67) once it arrives, and that point is
in the orbit plane. At **19.04 s** the moon sweeps past it, only 0.10 away,
and at 19.14 s planet2 comes within 0.08. Both are below the gap/4 = 0.1 it
needs. ✗

That's why sampling covers the **whole** video, not just the flight.

The next candidate, **above** (y = 2.67, z = 0), is 2.67 above the orbit
plane. Everything moving in that plane would have to come within its own
need (planet2: 0.87 + 0.87 + 0.4 = 2.13, the moon: 0.3 + 0.87 + 0.4 = 1.57),
and it's always at least 2.67 away. That's clear at every moment. ✓

The report ([scene4-flights.txt](images/scene4-flights.txt)):

```
motion (flights): 4 moving objects, 0 colliding pairs
    planet1    orbits sun, 0.00-20.00 s: radius 5.60, 1.00 turns
    planet2    orbits sun, 0.00-20.00 s: radius 8.67, 2.00 turns
    comet      flies_past sun, 4.00-10.00 s: from (-7.53, 2.67, 0.00) to (7.53, 2.67, 0.00)
    moon       orbits planet1 (which moves), 0.00-20.00 s: radius 1.50, 3.00 turns
```

| | naive (E1) | orbits (E2) | flights (E3) |
|---|---|---|---|
| colliding pairs | 8 | 4 | **0** |

| 0 s | 3 s | 7 s | 12 s |
|---|---|---|---|
| ![](images/scene4-flights-0.png) | ![](images/scene4-flights-3.png) | ![](images/scene4-flights-7.png) | ![](images/scene4-flights-12.png) |

Every circle stays grey for the whole video. The stress tests now check
this too: **moving objects never collide** is a hard check for every scene
(15).

What's left: at 7 s the comet is at the top edge, cut off, and planet2's
wide orbit leaves the picture. The camera still frames only the still
objects.

---

## 8. Step E4: framing the paths

Step D (14) fits the camera around every **still** object. Now it also has
to fit every **path**:

| Path | Spheres that hold all of it |
|---|---|
| orbit (center c, radius R, object reach r) | **32 spheres** spread around the circle, each of radius r + 2R·sin(π/64) |
| a moon's orbit | nothing extra: the planet's reach already includes it |
| fly-by (A → B, object radius r) | **two spheres**, at A and at B, radius r |

**Why 32 spheres for an orbit?** One sphere of radius R + r around the center
would hold the whole ring too, but it's as tall as it is wide, and an orbit
is flat. That's a lot of empty space for the camera to make room for. 32
points around the circle are 360°/32 = 11.25° apart. Any point of the ring is
at most half a step from one of them, and **at most 2R·sin(π/64)** away (that's a safe bound for half a step's
chord). So growing
each sphere by that covers the whole ring.

**Why two spheres for a fly-by?** Every point of the segment lies between A
and B, and the distance from a point is "convex": along a straight line,
it's largest at one of the ends. So whatever fits both ends fits the whole
path.

`scene_solver` hands these to the layout (`include_in_frame`) and frames
again (`reframe`), with the tight binary search from 14. A new check walks
every path with the same sample step as section 4 and counts moving objects
that leave the picture at **any** moment.

### Worked example

```
planet1's ring:  R = 5.60, reach 1.8:  32 spheres of radius 1.80 + 2·5.60·sin(π/64) = 1.80 + 0.55 = 2.35
planet2's ring:  R = 8.67, reach 0.87: 32 spheres of radius 0.87 + 2·8.67·sin(π/64) = 0.87 + 0.85 = 1.72
comet's ends:    (±7.53, 2.67, 0),  radius 0.87

box in y:  from −2.35 (planet1's ring) to 2.67 + 0.87 = 3.53 (the comet)  →  center y = 0.59
farthest:  a point of planet2's ring, √(8.67² + 0.59²) + 1.72 = 8.69 + 1.72 = 10.40
sphere fit:  1.05 · 10.40 / sin(25°) = 25.85,   then the binary search → 19.90
```

| | flights (E3) | framed (E4) |
|---|---|---|
| what the camera fits | still objects only | still objects **and every path** |
| scene radius | 2.40 | **10.40** |
| camera distance | 4.06 | **19.90** (sphere fit: 25.85) |
| moving objects that leave the picture | 4 | **0** |

From [scene4-flights.txt](images/scene4-flights.txt) and
[scene4-framed.txt](images/scene4-framed.txt). The stress tests check it
too: **moving objects stay in the picture** is a hard check for every scene.

| 0 s | 3 s | 7 s | 12 s |
|---|---|---|---|
| ![](images/scene4-framed-0.png) | ![](images/scene4-framed-3.png) | ![](images/scene4-framed-7.png) | ![](images/scene4-framed-12.png) |

The objects still look smallish, and that's correct. The camera has to fit
the **whole** sweep of planet2's orbit and the comet's 15-unit path for the
entire video, not just one moment. At any single moment most of that room
is empty.

---

## 9. The whole of step E

| | E1 naive | E2 orbits | E3 flights | E4 framed |
|---|---|---|---|---|
| colliding pairs | 8 | 4 | 0 | **0** |
| moving objects leaving the picture | 4 | 4 | 4 | **0** |
| orbit radii (planet1, planet2, moon) | 2.60, 2.67, 1.50 | 5.60, 8.67, 1.50 | 5.60, 8.67, 1.50 | 5.60, 8.67, 1.50 |
| comet's path | through the sun | through the sun | above the sun | above the sun |

From a description that says only "orbits sun" and "flies past sun", the
planner found paths that never collide at any moment, including a moon
riding along on a moving planet, and keeps everything visible the whole time.

---

## Try it on paper

1. Planet2 (R = 2.67, 2 turns over 0 → 20 s, start angle 0). Where is it at
   t = 2.5 s?
2. An orbit around (0, 0, 0) for an object with r = 0.5. A still box-shaped
   rock with r = 1 sits at (3, 2, 0). Which radii does it rule out? (gap = 0.4)

Answer: f = 0.125, angle = 2π · 2 · 0.125 = 90°, so p = (0, 0, −2.67).
Planet1 at that time: f = 0.125, angle = 45°, p = (1.84, 0, −1.84). Distance
between them = √(1.84² + 0.83²) = 2.02, more than 0.8 + 0.87 = 1.67. At this
moment they don't touch: planet2 has pulled ahead.

2. h = 3, v = 2, need = 0.5 + 1 + 0.4 = 1.9. need ≤ |v| = 2, so it's never in
   the way: the orbit passes underneath with room to spare. (Check: at R = 3
   the closest distance is √(0 + 4) = 2 ≥ 1.9 ✓.)
