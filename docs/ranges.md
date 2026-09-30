# Ranges and settings

A quick lookup: what values make sense for every setting and in-between
number.

---

## Rotation: `rotate(rot_y, rot_x)`

| Value | Degrees | Meaning |
|---|---|---|
| `0` | 0° | no rotation |
| `0.785` | 45° | an eighth of a turn |
| `1.571` | 90° | a quarter turn |
| `3.142` | 180° | half a turn |
| `6.283` | 360° | full turn = same as 0 |
| negative | | the other way |

- **Radians**: radians = degrees × 3.14159 / 180
- Any number works; it repeats every 6.283 (in an orbit, 12.566 = two full turns)
- A cube looks the same every 1.571 (90°), because it's symmetric

## Camera: `cam.move(eye)`, `cam.point_at(target)`

Shapes fit inside -1..1, so their corners are up to ~1.73 from the center.

| Distance from target | What you get |
|---|---|
| less than ~1.8 | inside the shape → broken image |
| 2 – 3 | very close, strong perspective, may be cut off |
| **3 – 8** | **a normal view of one shape** |
| 8+ | the shape gets small (good for whole scenes) |

- Direction matters: `(0,0,4)` front, `(4,0,4)` front-right, `(3,3,3)` corner from above
- **Never exactly above or below the target** (like `(0, 6, 0)`): see 04
- `eye` must not equal `target`

## Lens settings (`camera.h`)

| Setting | Default | Allowed | Normal |
|---|---|---|---|
| `up` | `(0, 1, 0)` | not parallel to eye → target | leave it |
| `fov_y` | 50° (0.873 rad) | between 0 and 180° | 30°–90°; smaller = zoomed in |
| `z_near` | 0.1 | > 0 and < z_far | 0.1 |
| `z_far` | 100 | > z_near | 100 |
| `width`, `height` | 640, 480 | ≥ 1 | |

## Inside the pipeline

| Thing | Range |
|---|---|
| NDC x, y | -1..1 is on screen, outside is off |
| NDC z (depth) | -1 = z_near, 1 = z_far |
| Pixel x | 0 .. width-1 (left → right) |
| Pixel y | 0 .. height-1 (top → bottom) |
| Barycentric weights | 0..1 inside the triangle, they add to 1 |
| Brightness | 0.15 (ambient) .. 1.0 |
| `px::Pixel(r, g, b, a)` | each 0..255, a = 255 is solid |
| Animation progress f | 0..1 (clamped) |
| cos, sin, dot of unit vectors | -1..1 |
