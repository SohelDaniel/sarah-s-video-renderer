# How the renderer's math works

A cheat sheet for when you forget. All matrices live in `transform.h`,
the camera in `camera.h`, and `main.cpp` is where you set rotation and eye.

---

## 0. The big picture

Every corner of a shape goes through this, in order:

```
shape coords --MODEL--> world --VIEW--> camera --PROJECTION--> clip --(÷ w)--> NDC --VIEWPORT--> pixels
              (rotate)          (eye)          (perspective)                           (image size)
```

| Step        | What it answers                          | Set in                       |
|-------------|------------------------------------------|------------------------------|
| MODEL       | How is the shape turned / placed?        | `main.cpp` → `model_matrix`  |
| VIEW        | Where is the camera, what does it see?   | `main.cpp` → `cam.eye`       |
| PROJECTION  | Make far things small (perspective)      | `camera.h` → `fov_y`         |
| ÷ w         | The actual "far = small" division        | `render.cpp` → `project`     |
| VIEWPORT    | Turn -1..1 into pixel positions          | image size in `main.cpp`     |

**Rotate moves the shape. Eye moves the camera.** They can look similar,
but the light is fixed in the world, so turning the shape changes which
face is bright, and moving the eye does not.

---

## 1. Rotation

### The one idea to remember

> **The columns of a matrix are where the axes land.**

Rotate the x axis and y axis by angle θ (counter-clockwise):

```
x axis (1,0)  lands on  ( cos θ, sin θ)
y axis (0,1)  lands on  (-sin θ, cos θ)     <- tilts LEFT, that's the minus
```

Put them in as columns:

```
R = | cos θ   -sin θ |
    | sin θ    cos θ |
```

Multiply out for a point (x, y):

```
x' = x·cos θ - y·sin θ
y' = x·sin θ + y·cos θ
```

### Example: rotate (2, 1) by 90°

cos 90° = 0, sin 90° = 1

```
| 0  -1 | | 2 |   | 0·2 - 1·1 |   | -1 |
| 1   0 | | 1 | = | 1·2 + 0·1 | = |  2 |
```

(2, 1) points right-and-up; a quarter turn left gives (-1, 2), up-and-left. ✓

### 3D = 2D with one axis frozen

The axis you rotate around doesn't move; the other two do the 2D thing.

**rotate_x(θ)**: x frozen, y and z turn
```
| 1    0       0    0 |
| 0  cos θ  -sin θ  0 |
| 0  sin θ   cos θ  0 |
| 0    0       0    1 |
```

**rotate_y(θ)**: y frozen, z and x turn
```
|  cos θ  0  sin θ  0 |
|    0    1    0    0 |
| -sin θ  0  cos θ  0 |
|    0    0    0    1 |
```

Why is the minus in the bottom-left for rotate_y? The axes go in a
circle, x → y → z → x. Around y, it's **z** that turns toward **x** (not x
toward z), so the roles are swapped compared to the 2D picture.

### Example: rotate_y(90°) on the point (1, 0, 0)

```
|  0  0  1 | | 1 |   |  0 |
|  0  1  0 | | 0 | = |  0 |
| -1  0  0 | | 0 |   | -1 |
```

Looking down from above: x pointed right, now it points away from you (-z). ✓

### Order matters

`rotate_y(a) * rotate_x(b)` means **x first, then y** (read right to left).

Take the point (0, 1, 0) and 90° for both:

```
rotate_x first:  (0,1,0) -> (0,0,1)    then rotate_y:  (0,0,1) -> (1,0,0)
rotate_y first:  (0,1,0) -> (0,1,0)    then rotate_x:  (0,1,0) -> (0,0,1)
```

Different answers → swapping the order changes the picture.

### Why 4×4 and not 3×3?

A 3×3 can rotate but can't **move** a point (no "+ number"). Add a 4th
coordinate w = 1 and put the move in the last column:

```
| 1 0 0 tx | | x |   | x + tx |
| 0 1 0 ty | | y |   | y + ty |
| 0 0 1 tz | | z | = | z + tz |
| 0 0 0  1 | | 1 |   |   1    |
```

---

## 2. The eye (camera / view matrix)

`look_at(eye, target, up)` moves the whole world so the camera sits at
(0, 0, 0) looking down -z.

### Step 1: build the camera's own 3 axes

```
back   = normalize(eye - target)     points from target back to the camera
right  = normalize(cross(up, back))  points to the camera's right
cam_up = cross(back, right)          the camera's true up
```

### Step 2: put them in the matrix

A point's position along an axis is `dot(axis, point - eye)`. The rows do
those dot products; the last column is the "- eye" part:

```
| right.x   right.y   right.z   -dot(right,  eye) |
| cam_up.x  cam_up.y  cam_up.z  -dot(cam_up, eye) |
| back.x    back.y    back.z    -dot(back,   eye) |
|   0         0         0              1          |
```

### Example A: eye = (0, 0, 4), target = (0, 0, 0), up = (0, 1, 0)

```
back   = normalize((0,0,4) - (0,0,0)) = (0, 0, 1)
right  = cross((0,1,0), (0,0,1))      = (1, 0, 0)
cam_up = cross((0,0,1), (1,0,0))      = (0, 1, 0)

last column: -dot(right,eye) = 0, -dot(cam_up,eye) = 0, -dot(back,eye) = -4
```

```
view = | 1 0 0  0 |
       | 0 1 0  0 |
       | 0 0 1 -4 |
       | 0 0 0  1 |
```

It just slides the world 4 back. The cube corner (1, 1, 1) becomes
**(1, 1, -3)**: 3 units in front of the camera (in front = negative z).

### Example B: eye = (4, 0, 4), looking at the origin from the side

```
back   = normalize((4,0,4))           = (0.707, 0, 0.707)
right  = cross((0,1,0), back)         = (0.707, 0, -0.707)
cam_up = cross(back, right)           = (0, 1, 0)
```

The point (1, 0, 1), the edge of the cube facing the camera:

```
p - eye = (-3, 0, -3)
x = dot(right,  p-eye) = 0.707·(-3) + (-0.707)·(-3) =  0
y = dot(cam_up, p-eye) =                                0
z = dot(back,   p-eye) = 0.707·(-3) +   0.707 ·(-3) = -4.24
```

(0, 0, -4.24): dead center, 4.24 units ahead. ✓

---

## 3. From camera to pixel (continuing Example A)

The corner is at (1, 1, -3) in camera space. Defaults: fov 50°, image 640×480.

### Projection

```
focal  = 1 / tan(fov/2) = 1 / tan(25°) = 2.1445
aspect = 640 / 480      = 1.3333

x_clip = focal/aspect · x = 1.6084 · 1 = 1.6084
y_clip = focal · y        = 2.1445 · 1 = 2.1445
w      = -z               = 3                  <- the depth, copied into w
```

### Divide by w (this is the "far = small" step)

```
x_ndc = 1.6084 / 3 = 0.536
y_ndc = 2.1445 / 3 = 0.715
```

Twice as far → w twice as big → half the size on screen.

### Viewport (NDC -1..1 → pixels)

```
x_px = 320·x_ndc + 320 = 320·0.536 + 320 = 491.6
y_px = 240 - 240·y_ndc = 240 - 240·0.715 =  68.4   (y flipped: image y goes down)
```

The corner lands at pixel **(492, 68)**: right side, near the top.

---

## 4. Ranges of everything

### Rotation: `rotate_x(θ)`, `rotate_y(θ)`

| Value      | Degrees | Meaning                     |
|------------|---------|-----------------------------|
| `0`        | 0°      | no rotation                 |
| `0.785`    | 45°     | eighth turn                 |
| `1.571`    | 90°     | quarter turn                |
| `3.142`    | 180°    | half turn                   |
| `6.283`    | 360°    | full turn = same as 0       |
| negative   |         | turns the other way         |

- Takes **radians**. `radians = degrees × 3.14159 / 180`
- Any number works; it repeats every 6.283.
- A cube looks the same every 1.571 (90°) because it's symmetric.

### Eye: `cam.eye = vec3(x, y, z)`

Shapes fit inside -1..1, so their corners are up to ~1.73 from the center.

| Distance from target | What you get                                |
|----------------------|---------------------------------------------|
| less than ~1.8       | inside/touching the shape → broken image    |
| 2 – 3                | very close, strong perspective, may cut off |
| **3 – 8**            | **normal view**                             |
| 8+                   | shape gets small                            |

- Direction matters too: `(0,0,4)` = front, `(4,0,4)` = side, `(3,3,3)` = corner from above.
- **Never exactly above or below the target** (like `(0, 6, 0)`): `back` is
  parallel to `up`, `cross` gives (0,0,0), the image comes out blank.
  Use `(0, 6, 0.001)` or change `cam.up` to `vec3(0, 0, -1)`.
- `eye` must not equal `target`.

### Other camera settings (`camera.h`)

| Setting   | Default        | Allowed                  | Normal range         |
|-----------|----------------|--------------------------|----------------------|
| `target`  | `(0, 0, 0)`    | anything ≠ eye           | center of the shape  |
| `up`      | `(0, 1, 0)`    | not parallel to eye→target | leave it           |
| `fov_y`   | 50° (0.873 rad)| between 0 and 180°       | 30°–90°; smaller = zoom in |
| `z_near`  | 0.1            | > 0 and < z_far          | 0.1                  |
| `z_far`   | 100            | > z_near                 | 100                  |

### Values inside the pipeline

| Thing                     | Range                              |
|---------------------------|------------------------------------|
| NDC x, y                  | -1..1 is on screen, outside is off |
| NDC z (depth)             | -1 = z_near, 1 = z_far             |
| Pixel x                   | 0 .. width-1  (left → right)       |
| Pixel y                   | 0 .. height-1 (top → bottom)       |
| Barycentric weights w1..3 | 0..1 inside the triangle, sum = 1  |
| Brightness                | 0.15 (ambient) .. 1.0              |
| `px::Pixel(r, g, b, a)`   | each 0..255, a = 255 is solid      |
| `cos`, `sin`              | -1..1                              |

---

## 5. Running it

```
make run              build and play scene 1 live in a window
make run SCENE=2      play scene 2 (the solar system)
make clean            delete the binary
```

The camera is now set with `cam.move(eye)` and `cam.point_at(target)`
instead of `cam.eye = ...` / `cam.target = ...`; the ranges above still apply.
How the live player, the timing and the window work: see `LIVE.md`.
