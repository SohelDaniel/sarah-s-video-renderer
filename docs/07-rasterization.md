# 07 · Rasterization: which pixels does a triangle cover?

After projection every triangle is three points on the image. Rasterization
decides which whole pixels are inside it.

Code: `render.cpp` `draw_mesh`, `fill`.

---

## 1. Transform each vertex once

A vertex is shared by several triangles (about 6 on the sphere).
`draw_mesh` transforms and projects every vertex **once** up front, then
each triangle just looks up its three corners:

```cpp
for(int i = 0;i<count;i++){
	world_verts[i] = transform_point(model_matrix, model.vertex(i));
	visible[i] = project(world_verts[i], screen_verts[i]);
}
```

## 2. Signed area and back-face culling

The signed area of a triangle on screen (twice the area, really):

```
area = det(s2 - s1, s3 - s1)
```

`.obj` faces are counter-clockwise seen from outside (02). The viewport
**flips y** (06), which turns counter-clockwise into clockwise, so a face
**facing the camera has a negative area** here. A positive area means we're
looking at its back side, so it's skipped:

```cpp
if(area >= 0.0f) return;
```

On a closed shape you never see a back face anyway (a front face is always
in the way), so this throws away about half the triangles for free.

## 3. Only look inside the bounding box

No need to test every pixel of the image: only those between the
triangle's smallest and largest x and y (clamped to the image edges).

## 4. Barycentric weights: inside or outside?

For a pixel with center c, compute three weights:

```
w1 = det(s3 - s2, c - s2) / area      (the small triangle opposite corner 1)
w2 = det(s1 - s3, c - s3) / area      (opposite corner 2)
w3 = det(s2 - s1, c - s1) / area      (opposite corner 3)
```

Each one is the area of the small triangle made by the pixel and one edge,
divided by the whole triangle's area. So:

- they always **add up to 1**
- all three **≥ 0 exactly when the pixel is inside**
- one of them negative → the pixel is on the wrong side of that edge → outside

Pixel centers are at (i + 0.5, j + 0.5): the middle of the little square.

---

## Worked example: a tiny triangle, pixel by pixel

Screen corners: **s1 = (1, 1), s2 = (1, 5), s3 = (7, 1)**.

**Area:**

```
s2 - s1 = (0, 4)
s3 - s1 = (6, 0)
area = 0·0 - 4·6 = -24          negative → facing us → draw it
```

**Pixel (2, 2), center c = (2.5, 2.5):**

```
w1 = det((7,1)-(1,5), (2.5,2.5)-(1,5)) / -24 = det((6,-4), (1.5,-2.5)) / -24
   = (6·(-2.5) - (-4)·1.5) / -24 = (-15 + 6) / -24 = 0.375
w2 = det((1,1)-(7,1), (2.5,2.5)-(7,1)) / -24 = det((-6,0), (-4.5,1.5)) / -24
   = (-6·1.5 - 0) / -24 = 0.375
w3 = det((1,5)-(1,1), (2.5,2.5)-(1,1)) / -24 = det((0,4), (1.5,1.5)) / -24
   = (0 - 4·1.5) / -24 = 0.25

sum = 1 ✓   all ≥ 0 → INSIDE
```

**Pixel (5, 3), center (5.5, 3.5):**

```
w1 = det((6,-4), (4.5,-1.5)) / -24 = (-9 + 18) / -24 = -0.375   negative → OUTSIDE
```

(w2 = 0.625 and w3 = 0.75. The sum is still 1, but one weight is below
zero, so the pixel is past the long edge.)

**All pixels, # = inside** (x from 0 to 8, y from 0 down to 6):

```
    x: 0 1 2 3 4 5 6 7 8
y 0    . . . . . . . . .
y 1    . # # # # # . . .
y 2    . # # # # . . . .
y 3    . # # . . . . . .
y 4    . # . . . . . . .
y 5    . . . . . . . . .
```

12 pixels. The bounding box was x 1..7, y 1..5 (35 tests), so the other 23
were rejected by the weights.

## Edge slack

A pixel exactly on an edge shared by two triangles should give w = 0. With
floats it can come out as -0.0000001 for **both** triangles, leaving a
one-pixel gap. The code accepts anything above -0.00001:

```cpp
const float edge_slack = -1e-5f;
```

## The weights are useful for more than inside/outside

Any value known at the three corners can be blended to the pixel with the
same weights:

```
value at pixel = w1·value1 + w2·value2 + w3·value3
```

That's how depth is found at every pixel (08). Colors, normals and texture
coordinates use the same formula.

---

## Try it on paper

Is pixel (1, 4), center (1.5, 4.5), inside? Compute w1.

Answer: w1 = det((6,-4), (0.5,-0.5)) / -24 = (-3 + 2) / -24 = 0.042.
w2 = det((-6,0), (-5.5,3.5)) / -24 = -21 / -24 = 0.875. w3 = det((0,4),
(0.5,3.5)) / -24 = -2 / -24 = 0.083. All ≥ 0, so inside, and it's the `#` at y 4. ✓
