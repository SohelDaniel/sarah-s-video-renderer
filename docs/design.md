# Design

Why the code is split up the way it is, and questions you might be asked
about it.

---

## Who uses whom

```
main ──► player ──► window        (SDL lives only in here)
            │
            ├──► camera ──► timeline<viewpoint>
            ├──► object ──► timeline<pose>
            └──► render  ──► px::Image + depth buffer   (the rasterizer)
```

Arrows go one way. `render` doesn't know windows exist, `object` doesn't
know about time passing, `window` doesn't know about triangles. Each part
can be understood, tested and replaced on its own.

## Principles and where they show up

| Principle | Where |
|---|---|
| **Single responsibility** | `render` draws, `window` displays, `timeline` does time math, `player` runs the loop, `mesh` loads shapes. Each class has one reason to change. |
| **Encapsulation** | An object's `motion` and `now` are private, and change only through `move/rotate/scale`, so they can't get out of sync. |
| **Composition over inheritance** | `object` *has a* `timeline<pose>`, `camera` *has a* `timeline<viewpoint>`. No class hierarchy; the shared time logic is one template. |
| **Hide dependencies** | Only `window.cpp` includes SDL. Switching window library = rewriting one file. |
| **RAII** | `window` opens in its constructor, closes in its destructor. |
| **Separate "what" from "when"** | `main` only describes the scene; `player` decides when frames are drawn. The same scene can play in a window or be saved as stills. |
| **Fail loudly, early** | The mesh loader checks every face index at load time; `main` reports errors instead of crashing. |

## The solver's pieces

```
scene_spec (what the AI wrote) ──► layout (pure math: spheres in, positions out) ──► world (meshes + objects + camera)
```

- **`layout` never touches files or triangles.** It gets names, relations and
  radii, and gives back positions and a report. That makes it easy to test
  and to reason about, and it doesn't care whether the scene came from C++
  or from a parser.
- **`world` owns everything it builds.** The meshes live in a map inside it,
  and the objects point at them, so they can't outlive their meshes.
- **Tests use the solver alone.** `solver_test` links only the layout,
  mesh loading and math: no window, no SDL, no drawing. Because the layout
  is pure math (spheres in, positions out), it can be tested in
  milliseconds.
- **Soft wishes, hard rules.** The energy balances wishes (relations, being
  visible); a direct correction afterwards enforces the one rule that can't
  be broken (no overlaps). See 15.
- **The language is just another way to fill the same structs.** The parser
  produces a `scene_spec`, exactly like the C++ builders do, and a test
  proves the two are identical for every scene. Nothing after it knows or
  cares where a scene came from.
- **Errors are written for the AI.** All of them at once, with a line, a
  column, and a guess at what was meant. The parser also avoids follow-on
  noise: one failed definition doesn't cause a pile of "unknown name" notes.
- **An interface for "where frames come from".** The player draws from a
  `frame_source`: a fixed C++ scene, a world, or a live `.dan` file that
  replaces its whole scene when the file changes. One loop, three kinds of
  source, chosen at run time (virtual functions, 22).
- **Layers drawn in a fixed order**, because each needs the one before:
  solid objects (they fill the depth buffer), see-through objects far to near
  (24), lines (they test against the depth, 26), the anti-aliasing average
  (25), then circles, leader lines and text on the final picture (27, 28).
- **The label layout is pure 2D math** with no drawing, so the original
  notes' worked examples run as tests against the real code (28).
- **Two clocks.** Scene time (what's where) loops; real time (how far you
  walked) doesn't. Mixing them up would make walking jump at the loop point.
- **Only `window.cpp` knows about keys.** It turns them into a plain
  `controls` struct, so the walking math (`fly_camera`) is tested without a
  keyboard, and the scripted camera's path is never touched by walking
  (`set_view`).
- **Borrow the file format, write the interesting part.** `stb_truetype`
  only reads the font file (which character is which outline, the widths,
  kerning). Turning curves into pixels (flattening, the winding rule,
  coverage) is our own code in `font.cpp` (29).
- **A formula is laid out once, drawn every frame.** `math_layout` turns
  text into a tree and the tree into a box of placed glyphs and bars: plain
  data, with no drawing. So the box sizes are tested as numbers (30), and
  `render` just paints the list.
- **The solver doesn't load fonts.** It estimates a label's width from its
  letters (31), so it stays pure math and fast to test. The real width only
  matters when drawing, and the label layout handles the difference.
- **Solve twice when things move.** Placing the still objects needs room for
  the paths, and the paths need the still objects' places. Going round once
  more (still, paths, still again, paths again) settles it (32).
- **Write sizes for one picture, scale for the rest.** Every on-screen size
  (text, gaps, line widths) is written for a picture 480 pixels tall, and
  multiplied by height / 480. The solver measures in tan units (angles), so
  it gives the same answer at any resolution (34).
- **Mix light, not bytes.** Colors are stored as sRGB bytes but mixed as
  linear light, in one place (`srgb.h`), so every blend in the renderer is
  right the same way (35).
- **Work out what's fixed once.** A mesh's corner normals don't change, so
  they're computed when it's loaded, not per frame (36).
- **Still things stay exactly as they were.** A letter that isn't being
  animated is drawn from its cached glyph, as before vector paths existed,
  so adding Write and Transform changed no picture; an animated letter is
  drawn from its loops, and when it stops it's a still letter again at the
  same spot, so nothing jumps (37).
- **Pure functions for the timing.** How far each letter is through its
  writing, and which piece matches which, are plain functions of numbers
  (`piece_progress`, `border_then_fill`, `match_pieces`), tested without
  drawing anything (38, 39).
- **Every step is measured.** The report counts overlaps in 3D, overlaps
  on screen and satisfied relations, so each step can be compared with the
  one before (the table at the end of 14). An improvement you can't
  measure is a guess.
- **Nothing is tuned for one scene.** When the closer camera broke the
  fixed step size, the fix was a line search that adapts itself, not a new
  magic number. The AI will write scenes nobody has tested.
- **Mistakes in the description are reported, not fatal.** A wrong name
  drops that one relation and says so. With an AI writing the scene, a clear
  message it can act on is worth more than a crash.

## Choices that aren't obvious

- **Rotation stored as angles, not a matrix.** Angles can be blended over
  time; matrices can't (the halfway matrix squashes the shape). See 09.
- **Replay the timeline from the start every frame** instead of updating a
  bit each frame: no drift, and you can jump to any time. See 09.
- **Transform each vertex once per object**, not once per triangle: a
  vertex is shared by ~6 triangles on the sphere. See 07.
- **Renderer made once and wiped each frame**, instead of a new one every
  frame: no allocating 1.2 MB 120 times a second. See 08.
- **Pixel layout matches the GPU texture format** (RGBA, 4 bytes), so each
  frame is one memory copy. See 10.

---

## Questions you might be asked

**Why write a rasterizer on the CPU when GPUs exist?**
To understand what the GPU does. The transform, perspective divide,
triangle setup, barycentric fill and depth test are all visible in
`render.cpp` instead of hidden in hardware.

**What does the GPU do in this project?**
Almost nothing. It receives a finished image each frame and stretches it
onto the screen.

**Why 4×4 matrices for 3D?**
A 3×3 can't move points. With w = 1 the 4th column adds a translation,
and w also carries the depth for the perspective divide.

**Why do far things look small, in one sentence?**
The projection copies the depth into w, and dividing x and y by w shrinks
things in proportion to how far away they are.

**How does back-face culling work here?**
The signed area of the projected triangle tells which way its corners go
round. Faces are stored counter-clockwise from outside, so facing-away
triangles come out with the opposite sign and are skipped.

**How is a pixel tested against a triangle?**
Barycentric weights from three 2D cross products, divided by the area; the
pixel is inside when all three are ≥ 0. The same weights blend the depth.

**How does the animation stay the right speed on a slow computer?**
Positions come from real elapsed time, not frame counts.

**What happens if you forget to clear the depth buffer?**
New triangles fail the depth test against last frame's values, and parts
of the scene disappear.

**Where does the frame time go?**
About 1 ms per frame for ~2000 triangles at 640×480, mostly filling pixels.
The cost follows pixels covered, which is why the same scene costs less
when it's farther away.
