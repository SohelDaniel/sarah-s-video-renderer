# 17 · Parent and child

Until now every object moved on its own. But lots of things belong to
something else: a ring around a planet, a moon around a planet that's
itself orbiting a sun, a comet that hits a planet and sticks to it. So an
object can now have a **parent**, and follow it around.

Code: `object.h/.cpp` (`attach_to`, `stick_to`, `position_at`).

---

## 1. Two kinds of link

| Link | Meaning | World position at time t |
|---|---|---|
| `attach_to(parent)` | belongs to the parent from the start; its own position is **relative** to the parent | parent(t) + own(t) |
| `stick_to(parent, T)` | moves on its own until T, then keeps the same offset and rides along | own(t), plus parent(t) − parent(T) once t ≥ T |

`own(t)` is where the object's own timeline (09) puts it.

### attach_to: worked example

Scene 2's ring: `ring.attach_to(&planet_two)`, with its own position left
at (0, 0, 0), so it sits right where the planet is.

```
planet_two at t = 0:     (−7, 0, 0)        →  ring = (−7, 0, 0) + (0, 0, 0) = (−7, 0, 0)
planet_two at t = 5:     a quarter orbit the other way round...
```

Planet two orbits −6.283 rad (one turn backwards) in 20 s. At t = 5,
f = 0.25, so the angle is −90°. Turning (−7, 0, 0) with rotate_y(−90°):

```
x' =  x·cos + z·sin = −7·0 + 0·(−1) = 0
z' = −x·sin + z·cos = −(−7)·(−1) + 0 = −7
planet_two = (0, 0, −7)    →    ring = (0, 0, −7) ✓ still around it
```

**Before**, the ring got a copy of the planet's orbit, and the two had to be
kept identical by hand (scene 2 in the old `main.cpp`). **Now** it's one
line, and if the planet's motion changes, the ring follows automatically.

### stick_to: worked example

A comet moves on its own and sticks to a planet at T = 12 s:

```
comet at 12 s (own):    (4, 0, 0)
planet at 12 s:         (3, 0, 0)          →  the offset is (1, 0, 0)
planet at 15 s:         (0, 0, 3)
comet at 15 s = own(15) + (planet(15) − planet(12))
              = (4, 0, 0) + ((0, 0, 3) − (3, 0, 0))
              = (1, 0, 3)                  →  still exactly (1, 0, 0) from the planet ✓
```

(After T, the comet's own timeline has finished, so own(15) = own(12).)

## 2. Chains

`position_at(t)` asks the parent for **its** `position_at(t)`, and the
parent asks its own parent, and so on:

```
moon = planet(t) + moon_own(t)
     = (star(t) + planet_own(t)) + moon_own(t)
```

So a moon on a planet on a moving star just works, as deep as you like.
(A loop, "a attached to b, b attached to a", would ask forever. The solver
reports motion cycles before they get this far, see 18.)

## 3. Why only the position?

In most engines a child inherits its parent's **whole** model matrix:

```
world = M_parent · M_child             (translate · rotate · scale of the parent, then the child's)
```

Here it inherits only the parent's **position**:

```
world = translate(parent position) · M_child
```

That's on purpose:

- **Spin.** An orbiting planet also turns, so the same side faces the sun
  (09). With full inheritance, a moon would be dragged round one extra time
  per orbit, and a ring would wobble with every turn.
- **Scale.** A planet scaled to 0.6 would shrink its ring to 0.6 of its own
  size too. With position only, the ring's size means what it says.

It's the simplest link that's still correct for orbits, rings and things
that stick. Full inheritance (for example a wheel spinning on a moving car)
can be added later as a third kind of link.

## 4. In the code

```cpp
vec3 object::position_at(float t)const{
	vec3 own = motion.at(t).position;
	switch(follows){
		case link::none:     return own;
		case link::attached: return parent->position_at(t) + own;
		case link::stuck:
			if(t < stick_time) return own;
			return own + (parent->position_at(t) - parent->position_at(stick_time));
	}
	return own;
}
```

`model_matrix()` and the bounds circles use this position. Because the
timeline can be asked about **any** time (09), working out "where was the
parent at 12 s" costs nothing special.

---

## Try it on paper

A star moves from (0, 0, 0) to (10, 0, 0) over 10 s. A planet is attached
to it with its own position fixed at (3, 0, 0). A moon is attached to the
planet with its own position (0, 1, 0). Where is the moon at t = 4?

Answer: star(4) = (4, 0, 0), planet = (4, 0, 0) + (3, 0, 0) = (7, 0, 0),
moon = (7, 0, 0) + (0, 1, 0) = (7, 1, 0).
