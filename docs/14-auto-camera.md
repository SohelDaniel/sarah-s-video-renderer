# 14 · Auto camera (solver step D)

Choosing a camera is placement too: where to stand, where to look, how far
back. AIs are as bad at it as at placing objects, so the description only
says **which side** to look from (a word), and the solver works out the
rest so that **the whole scene fits in the picture**.

Code: `layout.cpp` (`frame`, `view_direction`), `scene_spec.h` (`view_word`),
`world.cpp`.

---

## 1. The description: a view word

```cpp
spec.view = view_word::front_above;     // the default
```

| Word | Looks from (normalized) |
|---|---|
| `front` | (0, 0.1, 1): almost level, a little above |
| `front_above` | (0, 0.375, 1): in front and above (≈ 20° down) |
| `left_above` | (−0.7, 0.5, 0.7) |
| `right_above` | (0.7, 0.5, 0.7) |

There's no "straight above": `look_at` breaks when the camera is directly
over its target (04).

## 2. The steps

```
1. one sphere around the whole scene:   center c, radius R
2. the narrower field of view:          half = min(fov_x, fov_y) / 2
3. how far back so the sphere fits:     distance = 1.05 · R / sin(half)
4. the camera:                          eye = c + direction · distance,  target = c
```

### Step 1: a sphere around the whole scene

- **center:** the middle of the box around all the objects' spheres (smallest
  and largest x, y, z, each including its radius)
- **radius:** how far the farthest sphere reaches from that center:

```
R = max over objects of ( |p_i − c| + r_i )
```

This sphere holds every object's sphere, so it holds every object. (It's
not always the *smallest* such sphere. Welzl's algorithm finds that one,
but this is simple and close enough.)

### Step 2: the narrower field of view

`fov_y` (50°) is the vertical field of view. The horizontal one is wider on
a wide image:

```
fov_x = 2 · atan( tan(fov_y / 2) · aspect )
```

The scene sphere must fit in **both** directions, so the narrower one
decides.

### Step 3: the distance

The camera sees a cone with half-angle `half`. A sphere of radius R exactly
fits inside it when the cone's edge just touches the sphere:

```
                    ____
camera  .  half  _-'    '-_
         '.  _-'     R    \
            '._______._____|     the edge touches the sphere: a right angle
         '-._   distance  c|     between the radius and the edge
              '-._        /
                   '-.__.'
```

The radius meets the edge at a right angle, so in that right triangle:

```
sin(half) = R / distance     →     distance = R / sin(half)
```

Then ×1.05 for a little breathing room.

---

## 3. Worked example

Two spheres: A (r = 1) at (0, 0, 0) and B (r = 1) at (4, 0, 0). Camera:
fov_y = 50°, 640×480, `front_above`.

**Step 1:**

```
box: x from −1 to 5,  y from −1 to 1,  z from −1 to 1
c = (2, 0, 0)
R = max( |(0,0,0) − c| + 1,  |(4,0,0) − c| + 1 ) = max(3, 3) = 3
```

**Step 2:**

```
tan(25°) = 0.46631
fov_x = 2 · atan(0.46631 · 1.3333) = 2 · atan(0.62174) = 2 · 31.87° = 63.74°
half  = min(63.74°, 50°) / 2 = 25°
```

**Step 3:**

```
distance = 1.05 · 3 / sin(25°) = 3.15 / 0.42262 = 7.454
```

**Step 4:**

```
direction (front_above) = (0, 0.375, 1) / √(0.375² + 1²) = (0, 0.375, 1) / 1.06800 = (0, 0.35112, 0.93633)
eye = (2, 0, 0) + 7.454 · (0, 0.35112, 0.93633) = (2, 2.617, 6.979)
```

The camera stands 7.45 away, in front and a bit above, looking at (2, 0, 0).
That's **guaranteed** to fit, but it's loose: the two spheres sit side by
side, so the scene is twice as wide as it is tall, while the big sphere
around it is as tall as it is wide.

## 4. Tight framing: binary search

The sphere fit is a safe **upper bound**: at 7.454 everything fits. The
tightest distance is somewhere between 0 (the camera inside the scene) and
that. So keep the direction and **binary-search** the distance:

```
low = 0,  high = 7.454                    (high always fits, low never does)
repeat 30 times:
    mid = (low + high) / 2
    does everything fit at mid?    yes → high = mid      no → low = mid
camera distance = high
```

"Fits" means every sphere passes the off-screen test from 15 with 5% of the
picture kept free at each edge:
`|x| + r/depth ≤ 0.95 · edge_x` and `|y| + r/depth ≤ 0.95 · edge_y`.

Each step halves the range, so 30 steps shrink it about a billion times
(2³⁰). It works because "fits" only changes once: everything closer
than the answer doesn't fit, and everything farther does.

### The two-sphere example, step by step

| step | low | high | mid | fits at mid? |
|---|---|---|---|---|
| 1 | 0 | 7.454 | 3.727 | no |
| 2 | 3.727 | 7.454 | 5.590 | yes |
| 3 | 3.727 | 5.590 | 4.658 | no |
| 4 | 4.658 | 5.590 | 5.124 | yes |
| 5 | 4.658 | 5.124 | 4.891 | no |
| 6 | 4.891 | 5.124 | 5.008 | no |
| … | | | | |
| 30 | | **5.079** | | |

**Check it on paper.** At the answer, the spheres just touch the left and
right margins. Seen from the camera, each sphere's center is 2 to the side
(they're at x = 0 and 4, and the camera aims at x = 2), and its radius is 1,
both at depth ≈ D:

```
(2 + 1) / D = 0.95 · tan(25°) · 4/3 = 0.95 · 0.46631 · 1.3333 = 0.5907
D = 3 / 0.5907 = 5.079 ✓
```

**5.08 instead of 7.45**: the spheres look about 1.5× bigger, and they still
fit. The width is what limits it here, not the height, which is exactly what
the sphere fit couldn't see.

## 5. Where it fits in the solver

```
greedy  →  frame  →  refine  →  frame  →  refine  →  frame
```

- **Frame before refining**, because the screen term (13) needs to know
  where the camera will really be. Otherwise refinement would un-hide things
  for the wrong camera.
- **Frame again after**, because refining moved things.
- **Refine and frame once more.** The tight framing reacts to small moves
  (it's tight!). With only one round, the final camera moved enough that the
  `crowd` stress test went from 0 to 6 pairs hidden on screen. A second
  round lets refinement see the camera it's really going to get. The stress
  tests now check "nothing hidden on screen" for every scene, so this can't
  come back unnoticed.

Moving objects' paths are included too, since step E4 ([16](16-motion-placement.md)).

## 6. Result

From the report ([scene3-framed.txt](images/scene3-framed.txt)):

```
layout (framed): 9 objects, 0 overlapping pairs, 0 pairs overlapping on screen
  camera: from front_above, looking at (-0.29, 0.85, -0.27), scene radius 5.88, distance 12.80 (sphere fit: 14.62)
          eye at (-0.29, 5.35, 11.72)
```

Check the sphere fit: 1.05 · 5.88 / sin(25°) = 6.174 / 0.42262 = **14.61** ✓
(14.62 with the unrounded radius). The binary search then brings it in to
**12.80**.

| refined (fixed camera at (0, 6, 16)) | framed (camera placed by the solver) |
|---|---|
| ![refined](images/scene3-refined.png) | ![framed](images/scene3-framed.png) |

The fixed camera was 17.1 away (√(6² + 16²)) and pointed at the origin.
The solver's camera is 12.8 away and aims at the scene's real center, so the
scene is centered and fills the picture. On a scene that's bigger
than the fixed camera can see, it backs away instead.

### Something it showed up: the step size

With the closer camera, the screen term got steeper (it grows like
1/depth²), and refinement's fixed step size started overshooting: the
energy went up on 244 of 500 steps. That's what led to the backtracking line
search in 13, which works for any camera. The report now shows it worked:
the energy never went up.

## The whole solver, step by step

| | naive (A) | greedy (B) | refined (C) | framed (D) |
|---|---|---|---|---|
| overlapping pairs (3D) | 36 | 0 | 0 | **0** |
| pairs overlapping on screen | 36 | 6 | 0 | **0** |
| relations satisfied | 0 of 7 | 7 of 7 | 7 of 7 | **7 of 7** |
| camera | fixed | fixed | fixed | **fits the scene** |

From a description with **no coordinates at all**, and one mistake in it, to
a layout where nothing overlaps, nothing hides anything, every relation
holds, and the camera frames it. That's the core idea of the project
working.

---

## Try it on paper

The same two spheres, but on a tall image, 480×640 (aspect 0.75). Which field
of view decides now, and what's the distance?

Answer: fov_x = 2 · atan(0.46631 · 0.75) = 2 · 19.27° = 38.5°, narrower than
50°, so half = 19.27°. distance = 3.15 / sin(19.27°) = 3.15 / 0.330 = 9.54. A
tall picture is narrow, so the camera has to back off further.
