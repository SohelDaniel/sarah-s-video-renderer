# 27 · Text

> Since 29, titles and labels use smooth **outline** letters; this page is
> about the 8×8 bitmap font that came first, which is still there as
> `draw_bitmap_text` (and as the fallback if `fonts/` is missing). The
> picture below now shows the outline font.

An explanation needs words. The renderer can now draw **text**, and scenes can
have **titles** across the top:

```
title "What pulls on what"
title "the comet heads for the planet" 4s-12s
```

![a scene with titles](images/text-title.png)

Code: `font8x8.h` (the font), `render.cpp` (`draw_text`, `paint_text`,
`text_width`), `world.cpp` (titles), `scene_parser.cpp` (`title`).

---

## 1. A bitmap font: letters as bits

The simplest kind of font is a **bitmap font**: every character is a small
grid of on/off pixels. This one is 8×8, from **font8x8** by Daniel Hepper,
based on the classic IBM PC VGA font. It's **public domain**, so it can be
copied into the project as it is (`font8x8.h`, with its notice).

Each character is **8 bytes**, one per row, top to bottom. Each byte is 8
bits, one per pixel in that row. **The lowest bit is the leftmost pixel.**

### Reading 'A' by hand

'A' is character 65 (0x41):

```
{ 0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00 }
```

Write each byte in binary (highest bit first, the usual way), then **read
it backwards** to get left to right:

| row | hex | binary (bit 7 … bit 0) | pixels left → right (bit 0 … bit 7) |
|---|---|---|---|
| 0 | 0x0C | 0000 1100 | `. . X X . . . .` |
| 1 | 0x1E | 0001 1110 | `. X X X X . . .` |
| 2 | 0x33 | 0011 0011 | `X X . . X X . .` |
| 3 | 0x33 | 0011 0011 | `X X . . X X . .` |
| 4 | 0x3F | 0011 1111 | `X X X X X X . .` |
| 5 | 0x33 | 0011 0011 | `X X . . X X . .` |
| 6 | 0x33 | 0011 0011 | `X X . . X X . .` |
| 7 | 0x00 | 0000 0000 | `. . . . . . . .` |

There's the A. In code, "is pixel x of this row lit?" is one shift and one
AND:

```cpp
unsigned char bits = font8x8_basic[c][row];
if((bits >> x) & 1){ /* pixel x is lit */ }
```

`bits >> x` slides bit x down to the bottom; `& 1` keeps only that bit.
For row 0 of 'A', x = 2: 0x0C >> 2 = 0x03 = 0000 0011, & 1 = **1**, lit ✓.

## 2. Bigger letters: scale

8 pixels tall is tiny. At **scale** s, every lit font pixel becomes an s×s
square of real pixels, so a character is 8s × 8s and a string of n
characters is

```
width = n · 8 · s          height = 8 · s
```

Every character is the same width ("monospaced"), so measuring text is that
one multiplication.

### Titles fit the picture

A title is centered, as big as fits: scale 3, or smaller if it would run
off the sides (with 16 pixels kept free on each side, so 640 − 32 = 608):

```
"What pulls on what"               18 · 8 · 3 = 432 ≤ 608   → scale 3
"the comet heads for the planet"   30 · 8 · 3 = 720 > 608   → try 2: 30 · 8 · 2 = 480 → scale 2
x = (640 − width) / 2              centered: (640 − 480) / 2 = 80
```

The second one is 30 characters: the first version didn't do this, and it
ran off both sides of the picture.

## 3. Readable over anything

White text over a bright planet would disappear. So every piece of text is
drawn twice: first a **shadow** in see-through black (alpha 170, blended as
in 24), one font pixel down and right, then the text on top. The dark edge
keeps it readable over any color.

## 4. Where it's drawn

Text is queued (`draw_text`) and painted at the very end of `finish()`, **on
the final picture**: after the see-through objects (24), the lines (26), the
anti-aliasing average (25) and the circles. That way it's always on top, and
with `--aa` it isn't averaged: bitmap letters stay sharp, pixel for pixel.

Titles come from the scene (`title_spec`), and `world::draw_overlays` (26)
places them each frame, only during their time range.

## 5. Tests

```
text:
  PASS  row 0 of 'A' (0x0C) lights pixels 2 and 3: ..XX....
  PASS  row 4 of 'A' (0x3F) lights pixels 0 to 5: XXXXXX..
  PASS  'hello' at scale 3 is 5 x 8 x 3 = 120 pixels wide
  PASS  'title "Orbits" 0s-5s' is read as a title shown for the first 5 s
```

The first two draw a real 'A' and read the pixels back, checking the
lowest-bit-is-leftmost rule against the table above.

## 6. Limits, and what's next

- **Only plain ASCII** (the first 128 characters): anything else shows as `?`.
- **Blocky.** It's a pixel font: great for a retro look, but large letters
  show their squares. Smooth text needs an **outline font** (TrueType): the
  letters are curves, filled at any size with the same coverage idea as the
  lines in 26. The usual way is `stb_truetype.h`, a public-domain single
  header.
- **Math** (Manim's LaTeX): run `latex`, turn the result into outlines
  (`dvisvgm`), and fill those. That's the biggest piece left.
- **Titles don't move things out of the way.** The solver doesn't know a
  title is there, so an object could sit behind it. **Labels** (28) are the
  next step: text that belongs to an object, placed so it doesn't cover
  anything.

---

## Try it on paper

1. Row 1 of 'A' is 0x1E. Which pixels are lit?
2. How wide is the title "Easing" at the largest scale that fits?

Answers: 1. 0x1E = 0001 1110 → reading from bit 0: `. X X X X . . .`, pixels 1–4.
2. 6 · 8 · 3 = 144 ≤ 608, so scale 3: 144 pixels, starting at x = (640 − 144) / 2 = 248.
