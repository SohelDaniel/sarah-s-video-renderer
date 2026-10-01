# 25 · Anti-aliasing (and video)

## Part 1: anti-aliasing

Every edge in the pictures so far is a **staircase**. A pixel is either inside
a triangle or not (07), so a slanted edge becomes a row of steps. Add `--aa` to
any command and the edges come out smooth:

```
./main 3 framed picture.png --aa
./main scenes/impact.dan --aa          (live, too)
```

Code: `render.cpp` (the constructor, `finish`, `out`), `player.h` (`samples`),
`main.cpp` (`--aa`, `./main aa`).

### Why edges are jagged

A pixel is a little square, but the rasterizer only asks about **one point**
in it: its center (07). If the center is inside, the whole square gets the
triangle's color; if not, none of it does. An edge that cuts a pixel in
half gets all or nothing. That's **aliasing**: a smooth shape sampled too
coarsely.

The fix is to ask about **more points** per pixel and mix the answers.

### Supersampling: draw it bigger, then shrink it

With `samples = 2`, everything is drawn into an image **twice as wide and
twice as tall** (1280×960 instead of 640×480), with a depth buffer just as
big. The viewport (06) does it with no other change: it maps NDC to
2W × 2H pixels instead of W × H. Then, in `finish()`, each 2×2 block of small
pixels is **averaged** into one real pixel:

```
final pixel = (p₁ + p₂ + p₃ + p₄) / 4          for red, green and blue separately
```

That's a **box filter**: every small pixel in the block counts the same.

**Worked example:** an edge pixel where 2 of the 4 small pixels are orange
cube (210, 120, 55) and 2 are background (20, 20, 28):

```
red   = (210 + 210 + 20 + 20) / 4 = 115
green = (120 + 120 + 20 + 20) / 4 = 70
blue  = ( 55 +  55 + 28 + 28) / 4 = 41.5 → 42      (the code rounds: (sum + 2) / 4)
```

A color halfway between the cube and the background, exactly as much as the
edge covers it. Without supersampling that pixel would be fully orange or
fully dark.

| without | with `--aa` |
|---|---|
| ![](images/aa-off.png) | ![](images/aa-on.png) |
| ![](images/aa-off-zoom.png) | ![](images/aa-on-zoom.png) |

The bottom row is the same corner of the cube enlarged 4× (`./main aa`), each
real pixel blown up into a 4×4 square so you can see them. Without: hard steps.
With: every step has an in-between pixel, and the edge reads as a straight line.

### What it costs

Four times the pixels to fill, so about four times the work. Measured on scene 3:

| | time per frame |
|---|---|
| without | 1.29 ms |
| `--aa` (2×2) | 5.50 ms |

That's 4.3×, close to the expected 4 (the averaging adds a little). It's still
under the 8.3 ms a 120 Hz screen allows, so `--aa` works live too.

### Things drawn on top

The debug circles (11) are 1-pixel lines. Drawn into the big image, a
1-pixel line would be averaged with 3 background pixels and come out at a
quarter of its brightness. So they're drawn onto the **final** image instead,
after the averaging, with their projected position and radius divided by
`samples`. Lines and text (26, 27) do the same.

### Other ways

- **MSAA** (multisample, what GPUs usually do): tests coverage at several points
  per pixel, but shades each triangle **once** per pixel. That's cheaper than
  shading everything 4 times.
- **FXAA / SMAA**: draw normally, then find edges in the finished picture and
  blur along them. Very cheap, a bit soft.
- **Analytic coverage**: work out exactly how much of the pixel the edge
  covers. The lines in 26 do this.

Supersampling is the simplest and the most correct: it really does look at 4
points, so it handles every kind of edge (between objects, at depth changes,
inside see-through things) in the same way.

---

## Try it on paper

A pixel where 3 of the 4 small pixels are white (255) and 1 is black (0). With
`samples = 3` there are 9 small pixels: if 4 are white, what's the final value?

Answers: (3 · 255 + 0) / 4 = 191.25 → 191. With 9: 4 · 255 / 9 = 113.3 → 113.
