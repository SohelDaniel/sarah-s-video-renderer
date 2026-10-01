# 08 · Depth and shading

Two questions for every pixel a triangle covers:

1. **Is something closer already drawn here?** (the depth buffer)
2. **How bright is it?** (shading)

Code: `render.cpp` `fill`, `render::begin`.

---

## 1. The depth buffer (z-buffer)

One float per pixel: "the closest depth drawn here so far". Before each
frame, `begin()` fills it with infinity, meaning nothing is drawn yet.

For each pixel inside a triangle:

```cpp
float z = w1 * s1[2] + w2 * s2[2] + w3 * s3[2];   // blend the corner depths (07)
if(z >= closest) continue;                         // something closer is already here
closest = z;                                       // we're the closest now
image.Draw(p.x, p.y, shaded);
```

Triangles can be drawn **in any order**, and the closest one still wins at
every pixel. No sorting needed.

### Worked example

Take the tiny triangle from 07 with corner depths z1 = 0.2, z2 = 0.4,
z3 = 0.6. At pixel (2, 2) the weights were (0.375, 0.375, 0.25):

```
z = 0.375·0.2 + 0.375·0.4 + 0.25·0.6 = 0.075 + 0.15 + 0.15 = 0.375
```

| Buffer holds | New z | Result |
|---|---|---|
| ∞ (empty) | 0.375 | drawn, buffer becomes 0.375 |
| 0.300 | 0.375 | 0.375 ≥ 0.300 → hidden, skipped |
| 0.500 | 0.375 | drawn, buffer becomes 0.375 |

(Blending NDC depth with screen-space weights is exact, because NDC z is a
straight-line function across the screen. Blending colors or textures this
way would need a perspective correction, but depth doesn't.)

### Why clear it every frame?

Forget to reset it and every new triangle is compared against **last
frame's** depths, so parts of the scene vanish wherever something used to
be closer. `render::begin` resets both the image and the depth buffer.

---

## 2. Flat shading (Lambert)

> **Since 36** the normal is blended across each triangle (smooth shading)
> and a small highlight is added on top, so curved shapes look round. The
> Lambert brightness below is still the base of it, and flat faces still
> get exactly these numbers plus a highlight of about 1.

Light hitting a surface head-on is brightest. At a slant it spreads over
more area, so it's dimmer. The brightness is the cosine of the angle
between the face's normal and the direction to the light, which is a dot
product (01):

```
brightness = ambient + (1 - ambient) · max(0, dot(normal, light_dir))
```

- `normal`: cross product of two edges, normalized (02). Computed in
  **world** space, so the light stays fixed in the world.
- `light_dir = normalize(0.4, 0.8, 0.6)`: up, a bit right, towards the viewer.
- `max(0, ...)`: a face turned away from the light gets 0, not negative light.
- `ambient = 0.15`: faces in shadow aren't pitch black.

The whole triangle gets **one** brightness ("flat" shading), which is why you
can see the individual faces on the sphere.

### Worked example (the running example's cube)

```
light_dir = (0.4, 0.8, 0.6) / √(0.16 + 0.64 + 0.36) = (0.4, 0.8, 0.6) / 1.07703
          = (0.37139, 0.74278, 0.55709)
```

The cube's face that ends up facing the camera: the model's left face
(normal (-1, 0, 0)) after rotate_y(90°) (03):

```
x' = -1·0 + 0·1 = 0,   z' = -(-1)·1 + 0·0 = 1   →  world normal (0, 0, 1)
dot((0,0,1), light) = 0.55709
brightness = 0.15 + 0.85 · 0.55709 = 0.62352
```

The cube's color is (230, 130, 60):

```
r = 230 · 0.62352 = 143.4 → 143
g = 130 · 0.62352 =  81.1 →  81
b =  60 · 0.62352 =  37.4 →  37
```

Its **top** face, normal (0, 1, 0): dot = 0.74278, brightness 0.78136,
red = 179. So the top is brighter than the front, because the light comes
mostly from above. ✓

---

## Try it on paper

A face with normal (0, -1, 0), pointing down. What's its brightness?

Answer: dot = -0.74278 → max(0, ...) = 0 → brightness = 0.15, just the ambient.
