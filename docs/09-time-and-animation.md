# 09 · Time and animation

A scene says how everything **starts**, then what changes **between second a
and second b**. Every frame, the engine works out where everything is at
the current time t.

Code: `timeline.h`, `object.h/.cpp`, `camera.h/.cpp`.

---

## 1. Progress: how far through a change are we?

A change runs from `start` to `end`. At time t:

```
f = (t - start) / (end - start),   then clamped to 0..1
```

Example: `one.rotate(1.6f, 0.35f, 2.0f, 5.0f)` (start 2, end 5):

| t | (t - 2) / 3 | clamped | meaning |
|---|---|---|---|
| 0 | -0.67 | 0 | not started |
| 3.5 | 0.5 | 0.5 | halfway |
| 5 | 1 | 1 | done |
| 9 | 2.33 | 1 | done, stays at the end value |

Without the clamp, at t = 9 the cube would keep turning past its target
forever. A span with no length (start = end) is an instant jump: 0 before,
1 after.

## 2. Lerp: blending two values

```
lerp(from, to, f) = from + (to - from) · f
```

"Start at `from`, go `f` of the way to `to`."

```
lerp(10, 20, 0.5) = 10 + 10·0.5 = 15
```

On a `vec3`, it's done to every coordinate. Scene 1's sphere moves from
(-3, 0, -1) to (-1.8, 1.4, 0.5) between seconds 3 and 6. At t = 4,
f = 1/3:

```
to - from = (1.2, 1.4, 1.5)
· 0.333   = (0.4, 0.467, 0.5)
+ from    = (-2.6, 0.467, -0.5)
```

A third of the way, moving in a straight line at a constant speed.

## 3. The timeline: replay the script every frame

`timeline<State>` holds the starting state (`initial`) and a list of steps
(span + change), sorted by start time. To get the state at time t it
**starts over from `initial` and replays every step**, each only as far
as it has got:

```cpp
State state = initial;
for(const step& s : steps){
	s.what(state, s.when.progress(t));
}
```

A change blends from **whatever the state is when its turn comes**:

- not started → f = 0 → `lerp(x, to, 0) = x` → no change
- finished → f = 1 → fully applied
- a change automatically starts **where the one before it ended**

### Worked example: the comet's two legs (scene 2), x only

```
initial x = -9
leg 1: move to x =  8 between  2 s and  8 s
leg 2: move to x = -3 between  9 s and 15 s
```

| t | start | leg 1 f | after leg 1 | leg 2 f | **x** |
|---|---|---|---|---|---|
| 0 | -9 | 0 | -9 | 0 | **-9** |
| 5 | -9 | 0.5 | -9 + 17·0.5 = -0.5 | 0 | **-0.5** |
| 8.5 | -9 | 1 | 8 | 0 | **8** |
| 12 | -9 | 1 | 8 | 0.5 | 8 + (-11)·0.5 = **2.5** |
| 20 | -9 | 1 | 8 | 1 | **-3** |

Leg 2 starts from 8 even though nobody told it "from 8".

**Why replay instead of nudging things a bit each frame?**

- **No drift.** Tiny float errors don't pile up over thousands of frames.
- **Jump to any time.** `at(17.5)` gives the exact picture at 17.5 s. That's
  how the doc images are made, and how the solver will check animation
  paths for collisions.
- **Easy to reason about.** The state depends only on t.
- **Looping is free** (20): asking for t = 0 again after t = 19.99 just works.

**Overlaps:** different properties (move and rotate together) are fine.
Two changes to the *same* property at once blend into each other, which is
hard to predict, so avoid it.

## 4. Instant vs. over time

Every object and camera method has two versions (function overloading):

```cpp
one.rotate(0.785f, 0.35f);              // right away: sets the STARTING pose
one.rotate(1.6f,   0.35f, 2.0f, 5.0f);  // over time: a step from 2 s to 5 s
```

The timed one stores a **lambda** that knows how to apply itself for a given f:

```cpp
motion.add({start, end}, [rot_y, rot_x](pose& p,float f){
	p.rot_y = lerp(p.rot_y, rot_y, f);
	p.rot_x = lerp(p.rot_x, rot_x, f);
});
```

`move`, `rotate`, `scale` go **to** a value. `rotate_around` swings **by** an
angle.

## 5. Orbits: rotate around a point

To turn a point p around a center c:

```
p' = c + rotate(p - c)       shift so c is the origin, turn, shift back
```

Example: planet one at (5, 0, 0), orbiting (0, 0, 0), 2 turns (12.566
rad) over 20 s. At t = 2.5: f = 0.125, so the angle is 1.571 = 90°:

```
p - c = (5, 0, 0)
x' =  5·cos 90° + 0·sin 90° =  0
z' = -5·sin 90° + 0·cos 90° = -5
p' = (0, 0, -5)                a quarter of the way round, now behind the sun ✓
```

The object also turns by the same angle (`rot_y += angle`), so the same
side keeps facing the center, like the Moon. This is exact for orbits
around y: rotate_y(a)·rotate_y(b) = rotate_y(a + b). With an x part too, it's
only an approximation, because rotations around different axes don't
simply add up.

## Why angles and not matrices?

The halfway point between two rotation matrices usually isn't a rotation:
blend them number by number and the shape gets squashed. Angles blend
fine. (The general fix is quaternions and slerp, a later upgrade.)

---

## Try it on paper

`two.scale(1.0f, 3.0f, 6.0f)` with a starting size of 0.8. What's the size at
t = 4.5?

Answer: f = (4.5 - 3)/3 = 0.5 → lerp(0.8, 1.0, 0.5) = 0.9.
