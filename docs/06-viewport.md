# 06 · Viewport: NDC → pixels

NDC runs from -1 to 1. The image runs from 0 to width (x) and 0 to height
(y), with **y going down**, because image rows are stored top to bottom. The
viewport matrix converts one into the other.

Code: `transform.h` `viewport`.

---

## The matrix

```
x_px =  (width/2)  · x_ndc + width/2
y_px = -(height/2) · y_ndc + height/2
z    =  z_ndc                          (kept only for the depth test)
```

```
viewport = | w/2    0    0   w/2 |
           |  0   -h/2   0   h/2 |
           |  0     0    1    0  |
           |  0     0    0    1  |
```

Check the corners:

| NDC (x, y) | pixel (640×480) | where |
|---|---|---|
| (-1, 1) | (0, 0) | top-left |
| (1, -1) | (640, 480) | bottom-right |
| (0, 0) | (320, 240) | center |

The **minus** on y is the flip: NDC y = +1 (top) → pixel y = 0 (top row).

---

## Worked example (the running example)

NDC (0.53613, 0.47656), image 640×480:

```
x_px =  320 · 0.53613 + 320 = 171.56 + 320 = 491.56
y_px = -240 · 0.47656 + 240 = -114.37 + 240 = 125.63
```

**The cube's corner (1, 1, 1) lands at pixel (491.6, 125.6)**: right side,
about a quarter of the way down. That's the end of the running example's
trip through the pipeline:

```
(1, 1, 1) → world (3, 2, -5) → camera (3, 2, -9) → NDC (0.536, 0.477) → pixel (491.6, 125.6)
```

Pixels aren't whole numbers yet. Deciding which whole pixels get colored
is rasterization (07).

---

## Why it's a separate step

Everything before this doesn't care about the image size. If you change the
window to 1280×960, only this matrix changes. That's why the camera's
projection works in the -1..1 box and the viewport is applied last.

---

## Try it on paper

Where do NDC (0.5, -0.5) and (-1, 0) land on a 640×480 image?

Answers: (480, 360) and (0, 240), the middle of the left edge.
