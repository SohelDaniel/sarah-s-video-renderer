# 01 · Vectors and matrices

Everything the engine does is built from two tools: **vectors** (a point or
a direction) and **matrices** (a way to move, turn or stretch many points at
once).

Code: `vec3.h/.cpp`, `mat4.h`

---

## 1. vec3: three numbers

```cpp
struct vec3{ float a, b, c; };   // x, y, z
```

A `vec3` can mean a **point** ("the corner is at (1, 1, 1)") or a
**direction** ("the light comes from up and to the right"). Same numbers,
different meaning.

The axes, as the engine uses them:

```
        +y (up)
         |
         |
         +------ +x (right)
        /
       /
     +z (towards you, out of the screen)
```

### Adding, subtracting, scaling

Done one coordinate at a time:

```
(1, 2, 3) + (10, 20, 30) = (11, 22, 33)
(5, 5, 5) - (1, 2, 3)    = (4, 3, 2)
(1, 2, 3) · 2            = (2, 4, 6)
```

`b - a` is **the arrow from a to b**. For example, from the camera (0, 0, 4)
to the origin: (0, 0, 0) - (0, 0, 4) = (0, 0, -4), which means 4 units in the -z direction.

### Length and normalize

```
length(v) = √(x² + y² + z²)
normalize(v) = v / length(v)      (same direction, length 1)
```

Example: v = (3, 4, 0)

```
length    = √(9 + 16 + 0) = √25 = 5
normalize = (3/5, 4/5, 0) = (0.6, 0.8, 0)
```

A length-1 vector is called a **unit vector**. We use them for pure
directions (the light, a face's normal, the camera's axes).

### Dot product

```
dot(v, w) = v.x·w.x + v.y·w.y + v.z·w.z
```

For unit vectors, `dot` is the **cosine of the angle between them**:

| dot | angle | meaning |
|---|---|---|
| 1 | 0° | same direction |
| 0 | 90° | perpendicular |
| -1 | 180° | opposite |

Example: dot((1, 0, 0), (0.6, 0.8, 0)) = 0.6. The angle is cos⁻¹(0.6) ≈ 53°.

Used for: brightness (08), the camera matrix (04), "which side of a
plane" tests.

### Cross product

```
cross(v, w) = ( v.y·w.z - v.z·w.y,
                v.z·w.x - v.x·w.z,
                v.x·w.y - v.y·w.x )
```

Gives a vector **perpendicular to both** v and w. Its direction follows
the right-hand rule.

Example: cross((0, 1, 0), (0, 0, 1))

```
x = 1·1 - 0·0 = 1
y = 0·0 - 0·1 = 0
z = 0·0 - 1·0 = 0      → (1, 0, 0)
```

up × towards-you = right. ✓ Used for: a triangle's normal (08), the
camera's right and up axes (04).

### det: the 2D cross product

```cpp
static float det(vec3 v, vec3 w){ return v.x*w.y - v.y*w.x; }
```

Only looks at x and y. It's twice the **signed area** of the triangle made
by v and w: positive if w is counter-clockwise from v, negative if
clockwise. Used for everything in rasterization (07).

Example: det((4, 0), (0, 3)) = 4·3 - 0·0 = 12, so the triangle
(0,0), (4,0), (0,3) has area 6. ✓

---

## 2. mat4: 4×4 numbers

A matrix is a machine that turns one point into another. We use 4×4
matrices, stored row by row: `m[r*4 + c]`.

### Multiplying a matrix by a point

Each row of the matrix, dotted with the point, gives one coordinate of the result:

```
| a b c d |   | x |   | a·x + b·y + c·z + d·w |
| e f g h | · | y | = | e·x + f·y + g·z + h·w |
| i j k l |   | z |   | i·x + j·y + k·z + l·w |
| m n o p |   | w |   | m·x + n·y + o·z + p·w |
```

Code: `vec4<T> operator*(const vec4<T>& v)` in `mat4.h`.

### Why 4 numbers for a 3D point? (homogeneous coordinates)

A 3×3 matrix can rotate and stretch, but it can't **move** a point: every
output is some mix of x, y, z, with no "+ 5" allowed. Adding a 4th number,
`w = 1`, fixes that: the last column gets multiplied by 1 and added on.

```
| 1 0 0 5 |   | 2 |   | 2 + 5 |   | 7 |
| 0 1 0 0 | · | 3 | = |   3   | = | 3 |
| 0 0 1 0 |   | 4 |   |   4   |   | 4 |
| 0 0 0 1 |   | 1 |   |   1   |   | 1 |
```

The point moved 5 along x. ✓

- **point** → w = 1 (gets moved by translation)
- **direction** → w = 0 (a direction has no position, so moving it does nothing)

`w` gets a second job in projection (05).

### Multiplying two matrices

`A * B` is a new matrix that does **B first, then A**:

```
(A · B) · p = A · (B · p)
```

Entry (row r, column c) of A·B = dot(row r of A, column c of B).
Code: `mat4 operator*(const mat4& other)` in `mat4.h`.

**Order matters.** A·B is usually *not* B·A. "Turn then move" and "move
then turn" end up in different places (see 03).

### Identity

```
| 1 0 0 0 |
| 0 1 0 0 |
| 0 0 1 0 |
| 0 0 0 1 |
```

Changes nothing. `mat4<float>::identity()`.

---

## Try it on paper

1. normalize((0, 5, 0)) = ?
2. dot((1, 2, 3), (4, 5, 6)) = ?
3. cross((1, 0, 0), (0, 1, 0)) = ?
4. Move the point (1, 1, 1) with the translation matrix above (5 along x).

Answers: 1. (0, 1, 0) · 2. 4 + 10 + 18 = 32 · 3. (0, 0, 1) · 4. (6, 1, 1)
