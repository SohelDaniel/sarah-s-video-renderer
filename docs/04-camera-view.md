# 04 · Camera view: world → camera

The **view matrix** moves the whole world so the camera ends up at (0, 0, 0)
looking down the **-z** axis. After this step, "in front of the camera" just
means "negative z", which makes the next step (projection) simple.

Code: `transform.h` `look_at`, `camera.h` `view()`.

---

## Step 1: build the camera's own three axes

Given where the camera is (`eye`), what it looks at (`target`), and which
way is roughly up (`up`, usually (0, 1, 0)):

```
back   = normalize(eye - target)      from the target back to the camera
right  = normalize(cross(up, back))   the camera's right
cam_up = cross(back, right)           the camera's true up
```

- The camera looks along **-back**.
- `cam_up` is recomputed because the `up` you gave might not be exactly
  perpendicular to `back` (for example when the camera looks slightly down).
- All three are unit length and perpendicular to each other.

## Step 2: put them in the rows

A point's coordinate along an axis is `dot(axis, point - eye)`: "how far
along this axis is it, measured from the camera?" The matrix does all
three dot products at once:

```
view = | right.x   right.y   right.z   -dot(right,  eye) |
       | cam_up.x  cam_up.y  cam_up.z  -dot(cam_up, eye) |
       | back.x    back.y    back.z    -dot(back,   eye) |
       |   0         0         0              1          |
```

The last column is the "- eye" part, pulled out:
dot(axis, p - eye) = dot(axis, p) - dot(axis, eye).

---

## Worked example (the running example)

Camera: eye = (0, 0, 4), target = (0, 0, 0), up = (0, 1, 0).

```
back   = normalize((0,0,4) - (0,0,0))  = (0, 0, 1)
right  = cross((0,1,0), (0,0,1))       = (1·1 - 0·0, 0·0 - 0·1, 0·0 - 1·0) = (1, 0, 0)
cam_up = cross((0,0,1), (1,0,0))       = (0·0 - 1·0, 1·1 - 0·0, 0·0 - 0·1) = (0, 1, 0)

last column: -dot((1,0,0), eye) = 0
             -dot((0,1,0), eye) = 0
             -dot((0,0,1), eye) = -4
```

```
view = | 1 0 0  0 |
       | 0 1 0  0 |
       | 0 0 1 -4 |
       | 0 0 0  1 |
```

The camera is just 4 units back, so the view matrix just slides everything
4 units away.

Our corner, at world (3, 2, -5):

```
(3, 2, -5 - 4) = (3, 2, -9)
```

**Camera space: (3, 2, -9).** 3 to the right, 2 up, 9 in front (negative z = in front).

## A less trivial camera

eye = (4, 0, 4), looking at the origin from the front-right:

```
back   = normalize((4, 0, 4))          = (0.707, 0, 0.707)
right  = cross((0,1,0), back)          = (0.707, 0, -0.707)
cam_up = cross(back, right)            = (0, 1, 0)
```

The point (1, 0, 1):

```
p - eye = (-3, 0, -3)
x = dot(right,  p - eye) = 0.707·(-3) + (-0.707)·(-3) =  0
y = dot(cam_up, p - eye) =                                0
z = dot(back,   p - eye) = 0.707·(-3) +   0.707·(-3)  = -4.24
```

(0, 0, -4.24): dead center, 4.24 in front. ✓

---

## The one thing that breaks it

If the camera is **exactly above or below** the target, `back` is parallel
to `up`, and `cross(up, back) = (0, 0, 0)`. Normalizing a zero vector gives
nothing useful, so the image comes out wrong. Keep the eye slightly off,
e.g. (0, 14, 4) instead of (0, 14, 0). The auto camera (14) always picks a
direction that's never straight up or down.

---

## Try it on paper

With eye = (0, 0, 4), where does the world point (0, 0, 4) end up? And
(0, 0, 10)?

Answers: (0, 0, 0), the camera itself. (0, 0, 6): positive z, which means
**behind** the camera, so it won't be drawn (see 05).
