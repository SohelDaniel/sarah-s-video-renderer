# 11 · Scene description (solver step A)

The engine is built for scenes written by an **AI**. AIs are good at saying
*what* should be in a scene ("a big cube with a sphere next to it and a
pyramid on top"), but bad at picking *coordinates*: they overlap things,
put them off screen, or cram everything in one spot.

So the AI never writes coordinates. It writes **objects and relations**,
and the **layout solver** works out the numbers. This part covers the
description itself, and what you get if you *don't* solve anything
(the "naive" layout): the baseline every later step has to beat.

Code: `scene_spec.h`, `layout.h/.cpp`, `world.h/.cpp`, `main.cpp` (`lazy_ai_scene`).

---

## 1. What a description looks like

```cpp
scene_spec spec;
spec.add("cube",    "shapes/cube.obj",    orange, size_word::big, 10);
spec.add("sphere",  "shapes/sphere.obj",  blue).near("cube");
spec.add("pyramid", "shapes/pyramid.obj", white).above("cube");
spec.add("torus",   "shapes/torus.obj",   teal).left_of("sphere");
```

Each object has:

| Field | Meaning | Example |
|---|---|---|
| name | how others refer to it | `"sphere"` |
| mesh file | which shape | `"shapes/sphere.obj"` |
| color | r, g, b | `px::Pixel(80, 160, 230)` |
| size | a **word**, not a number | `size_word::big` |
| importance | higher = placed first, gets the best spot | `10` |
| relations | how it relates to others | `.near("cube")` |

A parser will later read the same thing from a text file. It will fill
exactly these structs, so everything built on them doesn't change.

### Size words

The AI says "big"; the engine decides what big means:

| Word | Scale |
|---|---|
| tiny | 0.3 |
| small | 0.5 |
| normal | 0.8 |
| big | 1.4 |
| huge | 2.0 |

### Relations

| Relation | Meaning |
|---|---|
| `near X` | close to X, any side |
| `left_of X` / `right_of X` | along x (−x is left) |
| `above X` / `below X` | along y |
| `in_front_of X` / `behind X` | along z (+z is towards the usual camera) |

## 2. Every object is a sphere (to the solver)

The solver doesn't look at triangles. It treats every object as its
**bounding sphere** (02): the smallest sphere around the center that holds
the whole mesh.

```
object radius = mesh bounding radius × size scale
```

| Object | Farthest vertex | Mesh radius | Size | Object radius |
|---|---|---|---|---|
| cube | (1, 1, 1) | √3 = 1.732 | big 1.4 | **2.42** |
| sphere | any point on it | 1 | normal 0.8 | **0.80** |
| pyramid | a base corner (1, −1, 1) | √3 = 1.732 | normal 0.8 | **1.39** |
| cone | a base rim point (1, −1, 0) | √2 = 1.414 | normal 0.8 | **1.13** |

Why spheres?

- **They don't care about rotation.** A turning box keeps changing its
  extents; a sphere is the same from every angle. The objects in scene 3
  spin, and their circles stay the same size.
- **One overlap test.** Two spheres overlap exactly when the distance
  between their centers is less than the sum of their radii:

```
overlap  ⇔  |p₁ − p₂| < r₁ + r₂
```

Example: the cube (r 2.42) at (0, 0, 0) and the sphere (r 0.80) at (3, 0, 0):

```
distance = 3,   r₁ + r₂ = 3.22,   3 < 3.22  → overlap
```

Move the sphere to (4, 0, 0): 4 ≥ 3.22 → no overlap, 0.78 of empty space
between the surfaces.

The price: a sphere is loose around flat or long shapes, so the solver may
leave more room than strictly needed. Boxes can come later for floors and
walls.

## 3. Names → indices, and catching mistakes

First, every relation's name is looked up (`resolve_names`). An AI *will*
make mistakes, so each one is reported clearly and the rest of the scene
still works:

```
octahedron near moon: there is no object called "moon" (relation ignored)
```

Referring to yourself ("cube near cube") is also caught.

## 4. Who goes first? (topological sort)

"sphere near cube" means the cube needs a position **before** the sphere can
be placed next to it. So objects are placed in **dependency order**. This is
a **topological sort**, done with **Kahn's algorithm**:

```
waiting[i] = how many of object i's references aren't placed yet
repeat until everything is placed:
    ready = objects with waiting = 0
    pick the most important one (ties: the one written first)
    place it
    everyone that refers to it: waiting − 1
```

### Worked example

```
cube     (importance 10)         waiting 0
sphere   near cube               waiting 1
pyramid  above cube              waiting 1
torus    left_of sphere          waiting 1
```

| Round | Ready | Picked | After it's placed |
|---|---|---|---|
| 1 | cube | **cube** | sphere 0, pyramid 0 |
| 2 | sphere, pyramid (tie → written first) | **sphere** | torus 0 |
| 3 | pyramid, torus | **pyramid** | |
| 4 | torus | **torus** | |

Order: cube, sphere, pyramid, torus. The torus waits for the sphere, which
waits for the cube.

### Cycles

"a left_of b" + "b left_of a": both wait for each other forever, and in
some round nothing is ready. The solver then picks the most important one
anyway, places it first, and reports it:

```
a is part of a cycle of relations (placed before the objects it refers to)
```

## 5. The naive layout: the baseline

`layout::method::naive` puts every object **exactly at the center of the
object its first relation points to**, ignoring sizes and everything else.
Objects without a relation go to (0, 0, 0). That's roughly what happens when
coordinates are guessed without looking: everything piles up.

## 6. Checking relations

After any layout, every relation gets a verdict (`layout::check`). With
`reach = r_a + r_b` (the center distance at which the two spheres just touch):

**Direction relations.** Measure how far `a` is from `b` along the axis:

| Relation | along = |
|---|---|
| a left_of b | b.x − a.x |
| a right_of b | a.x − b.x |
| a above b | a.y − b.y |
| a below b | b.y − a.y |
| a in_front_of b | a.z − b.z |
| a behind b | b.z − a.z |

```
along ≥ reach   → ok       (completely on that side, the spheres don't touch)
0 < along       → weak     (right side, but still overlapping sideways)
along ≤ 0       → FAILED   (wrong side, or level with it)
```

**near.** Measure the empty space between the surfaces:

```
surface gap = |a − b| − reach
gap < 0              → weak     (touching or inside each other)
0 ≤ gap ≤ 2          → ok
2 < gap ≤ 4          → weak     (near-ish)
gap > 4              → FAILED
```

Example, naive layout: the pyramid (r 1.39) "above" the cube (r 2.42), both
at (0, 0, 0):

```
along = 0 − 0 = 0 ≤ 0   → FAILED
```

The sphere "near" the cube, both at (0, 0, 0):

```
surface gap = 0 − 3.22 = −3.22 < 0   → weak
```

## 7. The result: scene 3, naive

Scene 3 (`lazy_ai_scene` in `main.cpp`) is written the way a lazy AI would
write it: five things all `near cube`, a pyramid `above cube`, a torus
`left_of sphere`, a tetrahedron `behind cube`, and one mistake
(`octahedron near moon`).

```
make run SCENE=3            (or ./main 3 naive)
```

![scene 3, naive](images/scene3-naive.png)

Everything is inside the cube. The **red circles** are the bounding
spheres of objects that overlap something (grey = fine). The report
([scene3-naive.txt](images/scene3-naive.txt)):

```
layout (naive): 9 objects, 36 overlapping pairs
  ...
  0 of 7 relations satisfied
  problems in the description:
    octahedron near moon: there is no object called "moon" (relation ignored)
```

9 objects give 9·8/2 = **36 pairs**, and every single pair overlaps. That's the
number to beat.

### How the circles are drawn

A sphere seen through a perspective camera looks (almost exactly) like a
circle. `render::finish` projects the sphere's center and a point on its
edge, straight up from the camera's point of view (`center + camera_up·r`),
and uses the distance between them on screen as the circle's radius.

Check the cube's circle on paper. The camera is at (0, 6, 16), the cube at the
origin:

```
distance to camera = √(6² + 16²) = √292 = 17.09
screen radius ≈ r · focal · (height/2) / distance
              = 2.42 · 2.14451 · 240 / 17.09 = 72.9 pixels
```

The big circle in the picture is about 146 pixels across. ✓

---

## The report is written for the AI

The AI can't see the picture. The report is the only way it can find out
what went wrong, so it says exactly which relation failed and why. Once the
AI is hooked up, this text goes straight back to it so it can fix its own
description.

---

## Try it on paper

1. The torus (r 0.80) is `left_of` the sphere (r 0.80). The sphere is at
   (4, 0, 0), the torus at (2.5, 0, 0). Verdict?
2. Order these by Kahn's algorithm: `a` (importance 1), `b near a`
   (importance 5), `c` (importance 3, no relations).

Answers: 1. along = 4 − 2.5 = 1.5, reach = 1.6: 0 < 1.5 < 1.6 → weak
(on the left, but still overlapping by 0.1). · 2. Round 1: a and c are ready,
c is more important → c. Round 2: a. Round 3: b. Order: c, a, b.
