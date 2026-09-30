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
