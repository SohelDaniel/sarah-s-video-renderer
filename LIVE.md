# How the live player works

A study guide for the "video" part of the renderer: how a scene written in
`main.cpp` becomes a moving picture in a window. Covers the math, the C++
syntax, the design, and questions you might get asked about it.

`MATH.md` covers the pipeline for ONE picture (rotation matrices, the view
matrix, projection, viewport). This file is everything on top of that.

---

## 0. The big picture

Before, `main` was a **photographer**: set things up, take a picture, change
things, take another. You got 3 separate PNGs and nothing in between.

Now `main` is a **film script**: it says how everything starts and what
changes *between second a and second b*. Nothing moves while `main` runs. At
the end it hands the script to a `player`, which plays it back live:

```
main.cpp                          player.cpp (about 120 times a second)
────────                          ─────────────────────────────────────
"cube starts at 45°"              t = seconds since start
"from 2s to 5s, turn to 1.6"  →   everything: where are you at time t?
"from 7s to 10s, camera flies"    draw a brand-new picture (your rasterizer)
player(12).play(...)              show it in the window
```

**Every frame is drawn from scratch.** Nothing is remembered from the last
frame; the motion comes from `t` being slightly bigger each time.

### Which file does what

| File | Job | New? |
|---|---|---|
| `timeline.h` | `lerp`, `time_span`, `timeline<State>`: the math of "things over time" | new |
| `object.h/.cpp` | an object's `pose`, and its instant + timed `move/rotate/scale/rotate_around` | changed |
| `camera.h/.cpp` | the camera's `viewpoint`, and instant + timed `move/point_at` | changed |
| `render.h/.cpp` | the rasterizer. Now made once and wiped with `begin()` each frame | changed a bit |
| `window.h/.cpp` | the ONLY file that uses SDL: opens a window, shows a `px::Image` | new |
| `player.h/.cpp` | the frame loop: clock → update → draw → show | new |
| `main.cpp` | the two scenes (the scripts) | changed |

### Who uses whom

```
main ──► player ──► window   (SDL lives only in here)
            │
            ├──► camera ──► timeline<viewpoint>
            ├──► object ──► timeline<pose>
            └──► render  ──► px::Image, depth buffer   (your rasterizer)
```

Arrows only go one way. `render` doesn't know about windows, `object` doesn't
know about time *passing*, `window` doesn't know about triangles. Each part
can be understood (and tested) on its own.

---

## 1. The frame loop (`player.cpp`)

This loop is the heart of every game and every real-time graphics program:

```cpp
while(screen.is_open()){
	t = std::chrono::duration<float>(clock::now() - started).count();
	if(t > seconds) break;

	cam.update(t);                                    // 1. where is everything at time t?
	for(object* o : scene) o->update(t);

	renderer.begin(cam);                              // 2. wipe, set up the camera
	for(const object* o : scene) o->draw(renderer);   // 3. rasterize everything

	screen.show(renderer.picture());                  // 4. put it on screen
	frames++;
}
```

### Why real time and not a frame counter?

You *could* say "every frame, add 0.016 s" (1/60). But then:

- a computer that only manages 30 frames per second plays the video in
  **slow motion** (12 seconds of script takes 24 real seconds)
- a 120 Hz screen plays it at **double speed**

Asking the clock "how much real time has passed?" means the video always
takes exactly 12 seconds. A slow machine just shows *fewer* frames of the
same motion. That's what "frame-rate independent" means.

### Why `steady_clock`?

There are different clocks in `<chrono>`:

- `system_clock`: the wall clock. It can **jump** (daylight saving,
  internet time sync), which would make the animation skip or run backwards.
- `steady_clock`: only ever goes forward at a steady rate. Right for timing.

### What stops the loop running a million times a second?

**vsync** (set in `window.cpp`). `show()` waits until the screen is ready for
its next refresh before returning. Your Mac's screen refreshes 120 times a
second, so the loop runs 120 times a second. That's the `played 1436 frames
in 12.0 s (119.6 frames per second)` message at the end.

### How much time does a frame actually take?

Measured without the window (so without vsync waiting), on this Mac:

| Scene | Triangles | Time to draw one frame |
|---|---|---|
| 1 (cube, sphere, torus) | 12 + 960 + 1024 = 1996 | ~1.15 ms |
| 2 (solar system) | 960 + 20 + 8 + 1024 + 6 = 2018 | ~0.55 ms |

A frame at 120 Hz has 8.3 ms, so the rasterizer uses about 1/8 of it. The
CPU could do ~900 frames per second; vsync is what holds it at 120.
(Scene 2 is faster even with the same triangle count because the objects are
further away: fewer pixels covered = less work in `fill()`. The cost of a
rasterizer depends on **pixels filled**, not just triangles.)

---

## 2. The math of time (`timeline.h`)

### 2.1 Progress: how far through a change are we?

A change runs from second `start` to second `end`. At time `t`:

```
        t - start
f  =  ───────────        then clamp f to 0..1
       end - start
```

Example: `one.rotate(1.6f, 0.35f, 2.0f, 5.0f)`, so start = 2, end = 5.

| t | (t - 2) / 3 | after clamp | meaning |
|---|---|---|---|
| 0 | -0.67 | **0** | not started, no change yet |
| 2 | 0 | **0** | just starting |
| 3.5 | 0.5 | **0.5** | halfway |
| 5 | 1 | **1** | done |
| 9 | 2.33 | **1** | done, stays at the end value |

**Clamping** (`std::clamp(f, 0.0f, 1.0f)`) is what makes "before" and "after"
work. Without it, at t = 9 you'd get f = 2.33 and the cube would keep
turning past its target forever.

A zero-length span (`start == end`) would divide by 0, so `progress` handles
it separately: it's 0 before `start` and 1 from `start` on, an instant jump.

### 2.2 Lerp: blending two values

**Lerp** = **l**inear int**erp**olation:

```
lerp(from, to, f)  =  from + (to - from) · f
```

Read it as "start at `from`, then go `f` of the way towards `to`".

```
lerp(10, 20, 0)    = 10 + 10·0    = 10    (all from)
lerp(10, 20, 0.5)  = 10 + 10·0.5  = 15    (halfway)
lerp(10, 20, 1)    = 10 + 10·1    = 20    (all to)
```

It works the same on a `vec3`: every coordinate separately. The sphere in
scene 1 moves from (-3, 0, -1) to (-1.8, 1.4, 0.5) between seconds 3 and 6.
At t = 4, f = (4 - 3)/3 = 0.333:

```
to - from     = (-1.8 - -3,  1.4 - 0,  0.5 - -1)  = (1.2, 1.4, 1.5)
      · 0.333 = (0.4, 0.467, 0.5)
+ from        = (-3 + 0.4,  0 + 0.467,  -1 + 0.5)  = (-2.6, 0.467, -0.5)
```

A third of the way there. ✓ Positions lerped over time move in a **straight
line at constant speed**.

Lerping an **angle** gives a steady spin: lerp(0.785, 1.6, f) is a turn at
constant speed from 45° to about 92°.

### 2.3 The timeline: replaying the script

`timeline<State>` holds:

- `initial`: the state at second 0
- `steps`: a list of `(time_span, change)` pairs, sorted by start time

To get the state at any time t, `at(t)` **starts again from `initial` and
replays every step**, each with its own progress:

```cpp
State state = initial;
for(const step& s : steps){
	s.what(state, s.when.progress(t));
}
return state;
```

A change always blends from **whatever the state is when its turn comes**,
not from a value saved when it was scheduled. That one idea makes a lot work
for free:

- not started → f = 0 → `lerp(x, to, 0) = x` → does nothing
- finished → f = 1 → `lerp(x, to, 1) = to` → fully applied
- a change automatically starts **from where the previous one ended**

### Worked example: the comet's two legs (scene 2)

```cpp
comet.move(vec3(-9, 3, -4));                       // initial
comet.move(vec3( 8,-1,  3), 2.0f,  8.0f);           // leg 1
comet.move(vec3(-3, 4,  6), 9.0f, 15.0f);           // leg 2
```

Just the x coordinate, at a few times:

| t | start | leg 1 (f₁) | after leg 1 | leg 2 (f₂) | after leg 2 = **x** |
|---|---|---|---|---|---|
| 0 | -9 | 0 | -9 | 0 | **-9** |
| 5 | -9 | 0.5 | -9 + 17·0.5 = -0.5 | 0 | **-0.5** |
| 8.5 | -9 | 1 | 8 | 0 | **8** (resting between legs) |
| 12 | -9 | 1 | 8 | 0.5 | 8 + (-11)·0.5 = **2.5** |
| 20 | -9 | 1 | 8 | 1 | **-3** |

Leg 2 starts from 8 (where leg 1 ended), even though `main` never said
"from 8". Nothing had to be stored or calculated ahead of time.

### Why sort by start time?

Replay order matters: leg 2 must be replayed *after* leg 1 to start from its
end. `add()` keeps the steps sorted by `start` so it doesn't matter what
order you write them in `main`. It uses `std::stable_sort`, which keeps
steps with the **same** start time in the order you wrote them.

### What if two changes overlap?

**Different properties** (a move and a rotate at the same time): no problem,
they touch different fields of the state.

**Same property** (two moves both running during 5-6 s): the later-starting
one blends from wherever the earlier one has got to, so it slowly takes
over. It won't crash, but it's hard to predict. Write your scripts so the same
property isn't changed twice at once.

### Why replay everything instead of "update a little each frame"?

The other way would be: each frame, nudge every object a bit from where it was
last frame. Replaying from `initial` is better here because:

- **no drift**: small float errors don't pile up over thousands of frames
- **you can jump to any time**: `at(17.5)` gives the exact picture at 17.5 s
  without playing the first 17 seconds (that's how the test frames were made)
- **simpler to reason about**: the state at time t depends only on t

The cost is that every frame redoes all the steps: with `k` steps, `k` small
function calls per object per frame. For a few dozen steps that's nothing
next to drawing 2000 triangles.

---

## 3. Objects over time (`object.h/.cpp`)

### 3.1 The pose

Before, an object stored its rotation as a **matrix**. You can't blend two
rotation matrices by lerping their numbers: halfway between two rotation
matrices usually isn't a rotation (the shape gets squashed). Angles *can* be
lerped. So an object now stores a `pose`:

```cpp
struct pose{
	vec3 position{0.0f, 0.0f, 0.0f};
	float rot_y = 0.0f;
	float rot_x = 0.0f;
	float size  = 1.0f;
};
```

and only builds the matrix when it's time to draw:

```cpp
translate(position) * rotate_y(rot_y) * rotate_x(rot_x) * scale(size)
```

Read right to left: scale first, then tip on x, then spin on y, then move
into place. Same as before, just built from numbers each frame.

### 3.2 Two versions of every method

```cpp
one.rotate(0.785f, 0.35f);              // right away: how it STARTS
one.rotate(1.6f,   0.35f, 2.0f, 5.0f);  // over time: from second 2 to 5
```

This is **function overloading**: same name, different parameters, and
the compiler picks the right one by the arguments.

- The instant version changes `motion.initial` (the state at second 0).
- The timed version calls `motion.add(...)` with a **lambda** saying what to do
  for a given progress `f`:

```cpp
void object::rotate(float rot_y,float rot_x,float start,float end){
	motion.add({start, end}, [rot_y, rot_x](pose& p,float f){
		p.rot_y = lerp(p.rot_y, rot_y, f);
		p.rot_x = lerp(p.rot_x, rot_x, f);
	});
}
```

`move`, `rotate`, `scale` go **to** a value. `rotate_around` swings **by**
an angle (you say "go round twice", not "end up at angle X").

### 3.3 Orbiting: `rotate_around`

To turn a point `p` around a center `c` (not around the origin):

```
1. shift so c is the origin:   p - c
2. turn around the origin:     turn · (p - c)
3. shift back:                 c + turn · (p - c)
```

(That's `translate(c) · turn · translate(-c)` from the comment in `object.h`,
done directly on the position.)

**Example:** planet one starts at (5, 0, 0) and orbits the sun at (0, 0, 0).
2 turns = 12.566 rad over 20 s. At t = 2.5: f = 0.125, angle = 12.566 · 0.125
= 1.571 rad = 90°. Using `rotate_y` from `MATH.md` (x' = x·cos + z·sin,
z' = -x·sin + z·cos), cos 90° = 0, sin 90° = 1:

```
p - c  = (5, 0, 0)
x' =  5·0 + 0·1 =  0
z' = -5·1 + 0·0 = -5
c + (0, 0, -5) = (0, 0, -5)
```

A quarter orbit: from the right of the sun to behind it. ✓

For a timed orbit the angle is multiplied by `f` first, so the planet swings
smoothly: at f = 0.5 it has gone half the angle, and so on.

**The object also turns by the same angle** (`rot_y += turn_y`), so the same
side keeps facing the center, like the Moon does to Earth.

### 3.4 Why adding angles is only *sometimes* exact

`rot_y += turn_y` is exact when the orbit turns around y only:

```
rotate_y(a) · rotate_y(b) = rotate_y(a + b)          ✓ (same axis: angles add)
```

But with an x part in the orbit, the object's orientation would really be
`turn_y · turn_x · rotate_y(rot_y) · rotate_x(rot_x)`, and that **can't** be
written as one `rotate_y(something) · rotate_x(something)` in general,
because rotations around different axes don't commute (see "Order matters" in
`MATH.md`). Adding angles is then a close approximation. All orbits in
the two scenes are around y, so they're exact.

(The real fix is to store orientation as a **quaternion** and blend with
**slerp**. That's how game engines do it, and a good next step.)

---

## 4. The camera over time (`camera.h/.cpp`)

Same pattern as the object, with a smaller state:

```cpp
struct viewpoint{
	vec3 eye;     // where the camera is
	vec3 target;  // what it looks at
};
```

`cam.move(...)` lerps `eye`, `cam.point_at(...)` lerps `target`. Each frame
`view()` builds the `look_at` matrix from the current viewpoint.

In scene 1, seconds 7-10 lerp **both** at once. The camera flies to its new
spot while its gaze slides from the cube to the torus, so it turns smoothly
instead of snapping.

The lens (`fov_y`, `z_near`, `width`...) stays public: it's plain settings
that don't change over time, so there's nothing to protect.

**Careful:** the camera must never be exactly above or below its target
(see `MATH.md`, section 4). A lerped camera path can *pass through* such a
point even if both ends are fine. Scene 2's overhead shot uses (0, 14, **4**),
not (0, 14, 0), for this reason.

---

## 5. The renderer, reused (`render.h/.cpp`)

Before, `camera::take()` made a new `render` for every picture. At 120 pictures
per second that's 120 new images and depth buffers per second (each ~1.2 MB)
being allocated and thrown away.

Now:

- the **constructor** allocates the image and depth buffer **once**
- **`begin(cam)`** runs at the start of every frame:
  - `image.Clear(background)` → every pixel back to the background color
  - fill `depth` with infinity → "nothing drawn here yet"
  - `view_projection = projection · view` for where the camera is *now*

Forgetting to clear the depth buffer is a classic bug: new triangles would
fail the depth test against last frame's leftovers and parts of the scene
would vanish.

The actual rasterizing (`draw_mesh`, `project`, `fill`) is **unchanged**.

---

## 6. Showing pixels in a window (`window.h/.cpp`)

C++ itself can't open a window. Only the operating system can, and SDL is
a library that asks it for us the same way on Mac, Linux and Windows.
**SDL does no drawing for us.** It's used for exactly three things:

```
1. open a window
2. take our finished pixel array and put it on screen
3. tell us when the close button is clicked
```

### The pieces

| SDL thing | Our name | What it is |
|---|---|---|
| `SDL_Window` | `handle` | the window itself |
| `SDL_Renderer` | `painter` | copies images onto the window |
| `SDL_Texture` | `screen` | an image stored **on the graphics card** |

### Every frame, in `show()`

```cpp
SDL_UpdateTexture(screen, nullptr, image.Data(), pitch);  // our pixels → graphics card
SDL_RenderTexture(painter, screen, nullptr, nullptr);     // texture → fill the window
SDL_RenderPresent(painter);                               // show it (waits for vsync)
```

The graphics card is only used as a **display**. It stretches a finished
picture onto the screen. Every pixel's color was decided by `render.cpp`.

### Why the pixel format just works

`px::Pixel` is 4 bytes in memory: `r, g, b, a` (the `static_assert` in
`pixel.h` guarantees exactly 4). `SDL_PIXELFORMAT_RGBA32` means "4 bytes per
pixel, in the order r, g, b, a". Same layout → the whole image is copied as
one block of memory with no converting.

### Pitch

`pitch` = bytes per row = `width × 4` = 640 × 4 = 2560. SDL needs it because
some images have padding at the end of each row; ours doesn't.

### The event queue

The OS sends programs a stream of events (mouse moved, key pressed, close
clicked). `is_open()` empties that queue every frame with `SDL_PollEvent`.
If a program stops reading its events, macOS decides it has frozen and shows
the spinning beach ball. That's why the loop calls `is_open()` every frame,
even though we only care about the close button.

### vsync and double buffering

The window has two pictures: the one on screen, and the one being prepared.
`SDL_RenderPresent` swaps them at the moment the screen refreshes (**vsync**),
so you never see half of one frame and half of the next ("tearing").

### Nearest-neighbour scaling

A Retina screen has 2× more real pixels than the window's size. `SDL_SCALEMODE_NEAREST`
blows each of our pixels up into a sharp 2×2 square, instead of blurring them.

---

## 7. C++ syntax used, explained

### Templates: `timeline<State>`

```cpp
template<typename State>
class timeline{ ... State initial; ... };
```

One class written once, used for two different types:
`timeline<pose>` (in `object`) and `timeline<viewpoint>` (in `camera`). The
compiler makes a separate copy of the class for each `State`. This is why
all of `timeline` is in the header: the compiler needs the full code at the
point where you use it with a new type. (`mat4<T>` works the same way.)

### Lambdas

```cpp
[to](pose& p,float f){ p.position = lerp(p.position, to, f); }
```

A lambda is a small function without a name, written right where it's
needed.

- `[to]`: the **capture list**. It copies `to` into the lambda, so it still
  has the value later, long after `move()` has returned.
- `(pose& p, float f)`: its parameters, like any function
- `{ ... }`: its body

Capturing **by value** (`[to]`) matters here. Capturing by reference (`[&to]`)
would point at the parameter `to`, which stops existing when `move()`
returns. The lambda would then read garbage when it runs 5 seconds later.

### `std::function`

```cpp
using change = std::function<void(State& state,float progress)>;
```

A box that can hold **anything callable** with that signature: a normal
function, or a lambda with any captures. Each lambda has its own unique
hidden type, so this is how different lambdas can live in one
`std::vector`.

`using change = ...` just gives the long type a short name.

### Function overloading

```cpp
void move(vec3 to);                        // instant
void move(vec3 to,float start,float end);  // over time
```

Same name, different parameter lists. The compiler picks by the arguments
you pass.

### `::scale` (the scope operator)

Inside `object`, a plain `scale(...)` means the member function
`object::scale`. The leading `::` means "the one in the global scope": the
matrix function from `transform.h`.

### Aggregate initialization: `{start, end}`

```cpp
motion.add({start, end}, ...);
```

`time_span` is a plain struct with two fields, so `{start, end}` builds one
field by field, in order. There's no constructor to write.

### Default member values

```cpp
float size = 1.0f;
vec3 position{0.0f, 0.0f, 0.0f};
```

Every new `pose` starts with these values without any constructor.

### `const` member functions

```cpp
State at(float t)const;
vec3 get_position()const;
```

The `const` at the end promises this function doesn't change the object. You
can call it on a `const` object, and the compiler stops you if you
accidentally modify something inside.

### `explicit`

```cpp
explicit player(float seconds);
```

Without `explicit`, C++ would silently turn a lone `float` into a `player`
wherever one is expected (e.g. `void f(player p); f(12.0f);` would compile).
`explicit` forbids that. Good habit for any one-argument constructor.

### Forward declarations

```cpp
struct SDL_Window;   // in window.h
class object;        // in camera.h
```

"This type exists; you'll see the details later." That's enough to use a
**pointer** or **reference** to it. `window.h` uses this so that files
including it (like `player.cpp`) don't need SDL at all. Only `window.cpp`
includes `<SDL3/SDL.h>`.

### RAII and deleted copying

```cpp
window(const std::string& title,int width,int height);  // opens it
~window();                                              // closes it
window(const window&) = delete;
window& operator=(const window&) = delete;
```

**RAII** ("Resource Acquisition Is Initialization") means the constructor
grabs the resource (the OS window) and the destructor gives it back. So the window
closes automatically when `play()` ends, whether it ends normally, by
`break`, or by an exception. You can never forget to close it.

`= delete` forbids copying. If you could copy a `window`, there would be two
objects holding the same OS window, and both destructors would close it:
the second one on something already destroyed.

### `static` on a free function

```cpp
static void orbit(pose& p, ...);   // in object.cpp
```

`static` here means "only visible inside this `.cpp` file". It's a private
helper, so no other file can call it or clash with its name.

### `<chrono>` time

```cpp
using clock = std::chrono::steady_clock;
const clock::time_point started = clock::now();
float t = std::chrono::duration<float>(clock::now() - started).count();
```

- `clock::now()` → a moment in time (`time_point`)
- `now - started` → a length of time (a `duration`) in the clock's own units
- `duration<float>(...)` → convert it to seconds, as a float
- `.count()` → get the plain number out

### `std::stable_sort` with a lambda

```cpp
std::stable_sort(steps.begin(), steps.end(),
	[](const step& a,const step& b){ return a.when.start < b.when.start; });
```

The lambda answers "should `a` come before `b`?". `stable` means equal
elements keep their original order.

### The ternary operator

```cpp
std::string which = argc > 1 ? argv[1] : "1";
```

`condition ? if_true : if_false`. Picks the scene from the command line,
or scene 1 if none was given.

---

## 8. Design (the OOP part)

| Principle | Where it shows up |
|---|---|
| **Single responsibility** | `render` draws, `window` displays, `timeline` does time math, `player` runs the loop. Each class has one reason to change. |
| **Encapsulation** | An object's `motion` and `now` are private. You can only change them through `move/rotate/...`, so `initial` and `now` can't get out of sync. |
| **Composition over inheritance** | `object` *has a* `timeline<pose>`, `camera` *has a* `timeline<viewpoint>`. No class hierarchy, and the time logic is shared by using one template in both places. |
| **Hide dependencies** | Only `window.cpp` knows SDL exists. Switching to another window library means rewriting one file. |
| **RAII** | `window` opens in its constructor and closes in its destructor. |
| **Separate the "what" from the "when"** | `main` only *describes* the scene; `player` decides *when* things are drawn. The same script could be played in a window, saved as frames, or (later) sent to a video file. |

---

## 9. Scene 2, the solar system, as a timeline

```
second:    0    2    4    6    8   10   12   14   16   18   20
           |----|----|----|----|----|----|----|----|----|----|
planet 1   ============ orbit twice (12.566 rad) =============
planet 2   ======== orbit once the other way (-6.283) ========
its ring   ============ the exact same orbit =================
sun        ==== grow 1.1 → 1.35 ====|==== shrink → 1.1 ======
comet           == leg 1 ===  == leg 2 ====
comet           ======= tumble ==========
camera                    ==swoop==  ==rise overhead==
```

What it shows that scene 1 doesn't:

- **orbits over time** (`rotate_around` with start/end)
- **chaining**: the sun's two scales and the comet's two legs each start from
  where the previous one ended
- **overlapping properties**: the comet moves and tumbles at the same time
- **two objects moving as one**: the ring gets the same orbit as planet two,
  so it stays around it

### Why the ring needs its own orbit

There's no way to say "the ring is attached to planet two" yet. If planet two
got a new path, the ring would need the same change copied by hand. Real
engines use a **scene graph**: each object can have a parent, and its model
matrix becomes `parent's model matrix × its own`. Then a moon orbiting a
planet orbiting a sun is just three parent-child links. Good next feature.

---

## 10. Limits, and good next steps

| Limit | Next step |
|---|---|
| Motion starts and stops sharply (constant speed) | **Easing**: pass `f` through a curve before lerping. Smoothstep: `f = f*f*(3 - 2*f)`. One line in `time_span::progress`. |
| Objects can't be attached to each other | **Scene graph**: parent pointers, multiply model matrices down the chain |
| Angles only add exactly around one axis | **Quaternions + slerp** for orientation |
| Triangles partly behind the camera are dropped whole | **Near-plane clipping**: cut the triangle at z_near instead |
| Flat shading: one color per triangle | **Gouraud / Phong shading**: interpolate normals with the barycentric weights you already compute |
| Only plays in a window | Save every frame and join them with ffmpeg, or write frames at a fixed time step (1/60 s) for a smooth, exact video |
| One thread | Split the image into strips and fill them on several CPU cores |

---

## 11. Questions you might be asked

**Why write a rasterizer on the CPU when GPUs exist?**
To understand what the GPU does. The transform, perspective divide, triangle
setup, barycentric fill and depth test are all visible in `render.cpp`
instead of hidden in hardware.

**What does the GPU do in this project?**
Almost nothing. It gets a finished picture each frame and stretches it onto
the screen. SDL is only used for the window.

**How does the animation stay the right speed on a slow computer?**
Positions come from real elapsed time (`steady_clock`), not from counting
frames. A slow computer shows fewer frames of the same motion.

**How is a timed change applied?**
Progress f = (t − start)/(end − start), clamped to 0..1, then lerp from the
current value to the target by f. Each frame the timeline replays all changes
from the initial state in start-time order, so each change starts from where
the one before it ended.

**Why not lerp rotation matrices?**
A blend of two rotation matrices isn't generally a rotation (it squashes the
shape). Lerping angles works for simple cases, and quaternion slerp is the
proper general solution.

**What happens if you forget to clear the depth buffer each frame?**
Old depth values stay, so new triangles fail the depth test against things
that aren't there anymore, and parts of the scene disappear.

**Where does the time per frame go?**
About 1 ms per frame for ~2000 triangles at 640×480, mostly in `fill()`
(pixels inside triangles). The cost follows the number of pixels covered,
which is why the same triangles cost less when they're further away.

**Why is `window` non-copyable?**
It owns an OS resource. Two copies would both close the same window in
their destructors.

---

## 12. Running it

```
make run              build and play scene 1 (12 s)
make run SCENE=2      build and play scene 2, the solar system (20 s)
./main 2              same, if it's already built
make clean            delete the binary
```

Needs SDL3: `brew install sdl3`. The Makefile finds it with `pkg-config`.
Close the window at any time to stop early. When it ends, it prints how
many frames it drew and the frames per second.
