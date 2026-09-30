# 03 · Model transform: shape → world

A mesh file describes a shape around (0, 0, 0) at size ~1. The **model
matrix** puts one copy of it into the world: how big, turned which way, and
where.

Code: `transform.h` (`scale`, `rotate_x`, `rotate_y`, `translate`),
`object.cpp` (`model_matrix`).

---

## The three matrices

### Scale: numbers on the diagonal

```
scale(s) = | s 0 0 0 |
           | 0 s 0 0 |
           | 0 0 s 0 |
           | 0 0 0 1 |
```

(x, y, z) → (s·x, s·y, s·z). Stretches away from (0, 0, 0).

### Rotate around y: x and z turn in a circle, y stays

```
rotate_y(θ) = |  cos θ  0  sin θ  0 |
              |    0    1    0    0 |
              | -sin θ  0  cos θ  0 |
              |    0    0    0    1 |

x' =  x·cos θ + z·sin θ
z' = -x·sin θ + z·cos θ
```

**The key idea: the columns are where the axes land.** Column 1
(cos θ, 0, -sin θ) is where the x axis ends up after turning. At θ = 90°
that's (0, 0, -1): the x axis now points away from you.

`rotate_x(θ)` is the same idea with y and z turning and x staying put.
Angles are in **radians**: radians = degrees · π / 180, so 90° = 1.5708.

### Translate: the last column

```
translate(tx, ty, tz) = | 1 0 0 tx |
                        | 0 1 0 ty |
                        | 0 0 1 tz |
                        | 0 0 0 1  |
```

Adds (tx, ty, tz), thanks to w = 1 (see 01).

## The order: scale, then rotate, then move

```cpp
model = translate(position) * rotate_y(rot_y) * rotate_x(rot_x) * scale(size);
```

Read **right to left** (01: A·B means B first):

1. **scale** while still centered at (0, 0, 0), so it grows evenly
2. **rotate** while still centered, so it turns in place
3. **move** into position last

Why this order? If you moved first, the rotation would swing the object
around the world's origin like a ball on a string. The same thing done on
purpose is how orbits work (09).

---

## Worked example (the running example)

The cube's corner **(1, 1, 1)**, with:

```
size     = 2
rot_y    = 90°   (1.5708 rad, cos = 0, sin = 1)
rot_x    = 0     (rotate_x(0) = identity, so skipped)
position = (1, 0, -3)
```

**Step 1: scale by 2**

```
(1, 1, 1) · 2 = (2, 2, 2)
```

**Step 2: rotate_y(90°)**

```
x' =  x·cos + z·sin =  2·0 + 2·1 =  2
y' =  y              =  2
z' = -x·sin + z·cos = -2·1 + 2·0 = -2
→ (2, 2, -2)
```

**Step 3: translate by (1, 0, -3)**

```
(2 + 1, 2 + 0, -2 - 3) = (3, 2, -5)
```

**World position: (3, 2, -5).**

### The same thing as one matrix

Multiplying the three matrices first gives one model matrix that does it all:

```
M = T · R · S = |  0  0  2   1 |
                |  0  2  0   0 |
                | -2  0  0  -3 |
                |  0  0  0   1 |
```

```
row 1:  0·1 + 0·1 + 2·1 + 1·1 =  3
row 2:  0·1 + 2·1 + 0·1 + 0·1 =  2
row 3: -2·1 + 0·1 + 0·1 - 3·1 = -5
→ (3, 2, -5) ✓ same answer
```

That's why the renderer builds M once per object and uses it for every
vertex: one matrix multiply per vertex instead of three.

---

## In the code: `pose`

An object doesn't store its matrix. It stores four plain numbers that can
be blended over time (09):

```cpp
struct pose{
	vec3 position;
	float rot_y, rot_x;
	float size;
};
```

and builds the matrix from them when it's drawn (`object::model_matrix`).

---

## Try it on paper

1. Put the corner (1, 1, 1) through size 1, rot_y 90°, position (0, 0, 0).
2. Now do the running example in the wrong order: scale by 2, then translate by (1, 0, -3), then
   rotate_y(90°). Compare with (3, 2, -5).

Answers: 1. (1, 1, -1) · 2. with size 2: (2,2,2) → move → (3, 2, -1) →
rotate: x' = 3·0 + (-1)·1 = -1, z' = -3·1 + (-1)·0 = -3 → (-1, 2, -3).
Completely different place. Order matters. ✓
