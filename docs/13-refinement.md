# 13 · Refinement (solver step C)

Greedy placement (12) got every relation right in 3D, but it never looked
through the camera: 6 pairs of objects overlapped **on screen** (one hiding
part of another). It also never went back on a decision.

Refinement fixes both. It lets **every object move a little at a time**,
all together, downhill on an **energy**: one number that measures how bad
the layout is. This is **gradient descent**. It's the same method as step 7 of the
label-placement notes this project started from, with more terms.

Code: `layout.cpp` (`refine`, `energy`, `gradient`, `look`).

---

## 1. The idea in one picture

Think of every object as a ball on a bumpy landscape whose height is the
energy. Gradient descent means: look at which way is downhill, take a small
step that way, repeat. The balls roll until they settle in a valley, where
all the pushes and pulls cancel out.

```
p  ←  p − η · ∇E          η (eta) = step size,  ∇E = the gradient
```

The **gradient** ∇E is, for each object, the 3D arrow pointing the way the
energy **grows** fastest. Stepping against it lowers the energy.

## 2. The energy: four terms

```
E = spring + push + relations + screen
```

| Term | Formula | Weight | What it wants |
|---|---|---|---|
| spring | β · \|p − home\|² | β = 1 | stay near the spot greedy chose |
| push | γ · max(0, r_i + r_j + gap − d)² | γ = 10 | keep `gap` of space between every pair |
| relations | ρ · max(0, how broken)² | ρ = 10 | keep every relation true |
| screen | ν · max(0, a_i + a_j + m − s)² | ν = 8000 | don't hide each other on screen |

Every term is **zero when things are fine** and grows as the **square** of
how wrong they are. Squares have two useful properties: they have a smooth
slope (so the gradient is easy), and they punish big mistakes much more
than small ones.

The **spring** is what stops everything flying apart: every other term
would be happiest with objects miles away from each other.

The most important object (the cube) doesn't move. It's the center of the
scene.

---

## 3. Each gradient, worked out

The rule used everywhere: if E = w · x² then dE/dx = 2 · w · x, and
chain rule for what's inside x.

### Spring

```
E = β |p − home|²
∇E = 2β (p − home)                pulls back towards home, harder the farther it's gone
```

### Push (a pair i, j)

```
d    = |p_i − p_j|
need = r_i + r_j + gap
E    = γ (need − d)²              only when d < need
∇_i E = −2γ (need − d) · (p_i − p_j)/d
```

`(p_i − p_j)/d` is the unit arrow from j to i. The minus sign means the
gradient points *towards* j, so stepping against it moves i **away** from j.
j gets the opposite arrow.

### Relations

**Direction** (e.g. `a above b`, dir = (0, 1, 0)):

```
along  = dot(p_a − p_b, dir)
broken = (r_a + r_b) − along       only when along < r_a + r_b
E      = ρ · broken²
∇_a E  = −2ρ · broken · dir        steps move a further in the dir direction
```

**near** (too far apart):

```
broken = d − r_a − r_b − near_limit      only when > 0
∇_a E  = 2ρ · broken · (p_a − p_b)/d    steps pull a towards b
```

### Screen: what the camera sees

First, where does an object appear? Build the camera's axes like `look_at`
(04): forward, right, up. For an object at p, with v = p − eye:

```
depth  = dot(v, forward)            how far in front of the camera
x      = dot(v, right) / depth      its spot on screen (similar triangles, 05)
y      = dot(v, up)    / depth
radius = r / depth                  how big its circle looks
```

These are "tan-angle" units: multiply by `focal` and you get NDC (05).

For a pair, with s = the distance between their screen spots:

```
need_s = radius_i + radius_j + m          m = 0.02 (a little space between circles)
E      = ν (need_s − s)²                  only when s < need_s
```

The gradient: moving p_i a small step along `right` moves its screen spot
along x by (step / depth). So a direction on screen (qx, qy) is the world
direction `right·qx + up·qy`, scaled by 1/depth:

```
∇_i E = −2ν (need_s − s) · (right·qx + up·qy)/s · (1 / depth_i)
```

(q = screen spot of i − screen spot of j.) The objects slide **sideways as
seen from the camera**. Depth is treated as fixed while doing this, which
is a small approximation that works well because the objects mostly move
sideways anyway.

---

## 4. Worked examples

### Example 1: spring + push, two iterations, on paper

One dimension so it's easy. Object A (r = 1) is fixed at x = 0. Object B
(r = 1) was put at x = 2 by greedy, so its home is 2. gap = 0.4, so they
want `need = 1 + 1 + 0.4 = 2.4` apart. β = 1, γ = 10, η = 0.01.

**Iteration 1**, B at x = 2:

```
spring gradient = 2·1·(2 − 2)          =  0
push gradient   = −2·10·(2.4 − 2)·(+1) = −8       (+1 = the arrow from A to B)
∇E              = −8
x ← 2 − 0.01·(−8) = 2.08
energy:  before 10·0.4² = 1.6,   after 1·0.08² + 10·0.32² = 0.0064 + 1.024 = 1.0304 ✓ lower
```

**Iteration 2**, B at x = 2.08:

```
spring = 2·(2.08 − 2)       =  0.16
push   = −2·10·(2.4 − 2.08) = −6.4
∇E     = −6.24
x ← 2.08 + 0.0624 = 2.1424
energy = 0.1424² + 10·0.2576² = 0.0203 + 0.6636 = 0.6839 ✓ lower again
```

**Where does it settle?** Where the two gradients cancel:

```
2β(x − 2) = 2γ(2.4 − x)
x − 2 = 10(2.4 − x)
11x = 26  →  x = 2.364
```

So B ends up 2.364 from A, not the full 2.4. The spring is still pulling a
little. **Soft constraints balance; they don't obey exactly.** That's why a
stronger weight goes on the terms that matter most (γ = 10 against β = 1),
and why the report checks the result afterwards.

### Example 2: the screen push, one step

A simple camera: eye (0, 0, 10) looking at the origin, so forward = (0, 0, −1),
right = (1, 0, 0), up = (0, 1, 0).

A (r = 1) at (0.5, 0, 0) and B (r = 1) at (−0.5, 0, −2), one behind the other:

```
A: v = (0.5, 0, −10)   depth 10   x = 0.05     radius 0.1
B: v = (−0.5, 0, −12)  depth 12   x = −0.0417  radius 0.0833

s      = 0.05 − (−0.0417) = 0.0917
need_s = 0.1 + 0.0833 + 0.02 = 0.2033      s < need_s → they overlap on screen
E      = 8000 · (0.2033 − 0.0917)² = 8000 · 0.1117² = 99.8
```

The screen arrow from B to A is (1, 0), which in the world is `right` =
(1, 0, 0):

```
∇_A E = −2·8000·0.1117 · (1, 0, 0) / 10 = (−178.7, 0, 0)
∇_B E = +2·8000·0.1117 · (1, 0, 0) / 12 = (+148.9, 0, 0)
```

Stepping against these moves A to the **right** and B to the **left**.
They slide apart sideways, as seen from the camera. B moves a bit less
because it's farther away: the same sideways step matters less on screen
when something is far away.

## 5. Choosing the numbers

**The weights say what matters.** The first try used ν = 500 and still
left 2 pairs overlapping on screen, because the springs won. Trying a few
values:

| ν | pairs overlapping on screen | final energy | spring part |
|---|---|---|---|
| 500 | 2 | 5.48 | 3.10 |
| 2000 | 1 | 8.44 | 6.77 |
| **8000** | **0** | 9.98 | 9.35 |

A bigger ν hides less but moves things farther from where greedy put them.
8000 is the smallest of these that gets everything visible.

**The step size has to be small enough.** With η = 0.02 the energy went
**up** now and then near the end: the steps were so big they jumped over the
valley's bottom. Halving it to η = 0.01 (and doing 500 steps instead of
300) fixed that. `refine` now counts every step where the energy went up, as
a self-check. It allows for float rounding: a float has about 7 significant
digits, so an energy around 10 wobbles by a few millionths even at the bottom.

## 6. Result

From the report ([scene3-refined.txt](images/scene3-refined.txt)):

```
layout (refined): 9 objects, 0 overlapping pairs, 0 pairs overlapping on screen
  7 of 7 relations satisfied
  refinement (energy should go down):
    step   0   E = 227.6333   (spring 0.0000, push 0.0000, relations 0.0000, screen 227.6333)
    step   1   E =  68.1100   (spring 2.7921, ...                          screen 65.3179)
    step  50   E =   9.9942   (spring 9.3628, ...                          screen 0.6313)
    step 500   E =   9.9782   (spring 9.3498, ...                          screen 0.6284)
    steps where the energy went up: 0 of 500
```

At step 0 all the energy is in the screen term (things hide each other). By
step 50 most of it has been traded for a little spring energy (things moved
a bit), and after that it barely changes: it has settled.

| | naive (A) | greedy (B) | refined (C) |
|---|---|---|---|
| overlapping pairs (3D) | 36 | 0 | **0** |
| pairs overlapping on screen | 36 | 6 | **0** |
| relations satisfied | 0 of 7 | 7 of 7 | **7 of 7** |

| greedy | refined |
|---|---|
| ![greedy](images/scene3-greedy.png) | ![refined](images/scene3-refined.png) |

Compare the two:

- the **sphere** came out from behind the cylinder
- the **tetrahedron** (red), still "behind cube", rose until you can see it
  above the cube
- the **octahedron** moved out from behind the cube's edge

## What's still missing

The camera is still fixed at (0, 6, 16). If the scene were bigger, parts
of it would be off screen, and the AI shouldn't have to pick a camera
either. That's step D ([14](14-auto-camera.md)).

---

## Try it on paper

In example 1, do iteration 3 (B at x = 2.1424). Is the step smaller or
bigger than before, and why?

Answer: spring = 2·0.1424 = 0.2848, push = −2·10·(2.4 − 2.1424) = −5.152,
∇E = −4.867, x ← 2.1424 + 0.0487 = 2.1911. The step is smaller (0.049 against
0.062). The spring pulls back harder the farther B goes, and the push weakens as
the gap closes, so it slows down as it approaches 2.364.
