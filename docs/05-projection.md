# 05 · Projection: why far things look small

The **projection matrix** plus the **divide by w** turn camera space into
**NDC** (normalized device coordinates): a cube from -1 to 1 on every axis.
Anything inside that cube is on screen.

Code: `transform.h` `perspective`, `render.cpp` `project`.

---

## The idea: similar triangles

A point at depth d in front of the camera, at height y, lands on a screen
(1 unit in front) at height **y / d**:

```
                           point
                          * (height y)
                   *     |
screen      *  y/d |     |
      *            |     |
camera ------------+-----+
            1            d
```

Twice as far → half as high. That's all perspective is. In camera space
"in front" means negative z, so **d = -z**.

## The problem: a matrix can't divide

A matrix only multiplies and adds. So the trick is:

1. the matrix **copies -z into w** (the bottom row is `0 0 -1 0`)
2. the renderer divides x, y, z by w afterwards: the **perspective divide**

```
perspective = | focal/aspect   0      0   0 |
              |      0       focal    0   0 |
              |      0         0      A   B |
              |      0         0     -1   0 |
```

## focal: the lens

```
focal = 1 / tan(fov_y / 2)
```

It scales things so the top edge of the field of view lands exactly on
y = 1. A **smaller** fov gives a **bigger** focal, which means zoomed in.

`aspect = width / height`. x is divided by it so a square stays square on a
wide image.

## A and B: keeping depth for the depth test

The third row keeps a depth value. We want, after dividing by w = -z:

```
z = -z_near  →  -1   (closest you can see)
z = -z_far   →  +1   (farthest you can see)
```

With z_ndc = (A·z + B) / (-z), solving those two equations gives:

```
A = (z_far + z_near) / (z_near - z_far)
B = 2 · z_far · z_near / (z_near - z_far)
```

---

## Worked example (the running example)

Camera-space point: **(3, 2, -9)**. Settings: fov_y = 50°, 640×480,
z_near = 0.1, z_far = 100.

**The matrix numbers:**

```
focal  = 1 / tan(25°) = 1 / 0.46631 = 2.14451
aspect = 640 / 480    = 1.33333
focal / aspect        = 1.60838
A = (100 + 0.1) / (0.1 - 100)     = -1.00200
B = 2 · 100 · 0.1 / (0.1 - 100)   = -0.20020
```

**Multiply (clip space):**

```
x_clip = 1.60838 · 3            =  4.82514
y_clip = 2.14451 · 2            =  4.28901
z_clip = -1.00200·(-9) - 0.20020 =  8.81782
w      = -(-9)                   =  9          ← the depth
```

**Divide by w (NDC):**

```
x_ndc = 4.82514 / 9 = 0.53613
y_ndc = 4.28901 / 9 = 0.47656
z_ndc = 8.81782 / 9 = 0.97976
```

All three are between -1 and 1, so the point is **on screen**. It's right of
center (0.54) and above center (0.48).

z_ndc = 0.98 looks close to "far" (1) even though 9 is small next to
z_far = 100. That's normal: this mapping spends most of its precision
close to the camera, and the depth test only needs the **order** to be right.

---

## Behind the camera

If w ≤ 0, the point is at or behind the camera. Dividing by it would flip
the point to the wrong side of the screen, so `project` returns false and
triangles using it aren't drawn:

```cpp
if(clip.w <= 0.0f) return false;
```

(Better would be cutting the triangle at z_near, called **near-plane
clipping**. That's needed before you can walk around inside a scene; it's
planned for the next round.)

---

## Try it on paper

Same camera. Where does the camera-space point (3, 2, -18), twice as far,
land in NDC x and y?

Answer: x = 1.60838·3 / 18 = 0.268, y = 2.14451·2 / 18 = 0.238. Exactly
half of before: twice as far, half as far from the center. ✓
