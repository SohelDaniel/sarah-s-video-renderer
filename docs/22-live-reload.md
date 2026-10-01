# 22 · Live reload, and the report the AI reads

A `.dan` file plays live, and **saving it again updates the window** while
it keeps playing, even while you're walking around in it. Every time the file
is read, the engine writes a **report** next to it. That's the loop an AI will
work in:

```
AI writes scene.dan ──► engine notices, parses, solves, shows it
        ▲                          │
        └──── reads scene.dan.report  (errors with "did you mean", collisions, hidden things, ...)
```

```
./main scenes/impact.dan          then edit scenes/impact.dan and save it
```

Code: `live_scene.h/.cpp`, `frame_source.h`, `player.cpp`, `main.cpp`.

---

## 1. Noticing that the file changed

Every file has a "last modified" time, which the system updates whenever
something writes to it. `live_scene` remembers that time when it reads the
file, and every **half second** compares it with the current one:

```cpp
auto changed = std::filesystem::last_write_time(path, error);
if(current && changed == stamp) return false;   // nothing new
stamp = changed;
return load();                                  // read it again
```

**Why every half second, and not every frame?** At 120 frames per second,
checking every frame would look at the file 120 times a second for no
reason. Twice a second costs one tiny system call per 60 frames, and half a
second is faster than anyone notices.

**Polling vs. being told.** The other way is to ask the operating system to
**tell** you when a file changes: FSEvents on macOS, inotify on Linux,
ReadDirectoryChangesW on Windows. That's three different APIs for one
feature. Polling a timestamp is portable C++17, and plenty fast here.

## 2. Keep the last good scene

An AI (or you) will save broken versions: a typo, or a file caught half
written in the middle of saving. If that blanked the window, every mistake
would throw away what you were looking at. So:

```
new version parses and solves  →  swap it in
new version has errors         →  keep showing the old one, and say so in the report
```

The report then ends with `(the last good version is still on screen)`.

If the **very first** version is broken, there's nothing good yet, so the
window shows an empty scene, keeps watching the file, and the report says
what to fix.

## 3. The report

`<file>.report`, written on every load, holds:

1. parser **errors** and **notes** (21), with line, column and "did you mean"
2. the full solver report: layout (11–15), motion (16–18), camera (14)

For example, after a bad save:

```
error: line 14, col 8: expected a shape after '=', got 'spher' (did you mean sphere?)
(the last good version is still on screen)
```

and after a good one, the usual `layout (framed): ... 0 overlapping pairs ...`,
`motion (framed): ... 0 colliding pairs`, `planned hit: comet touches planet at
12.00 s (gap 0.00) as planned`, and so on. Everything the AI needs to know
about how its scene turned out is in plain text. `*.dan.report` files are in
`.gitignore`: they're outputs, not sources.

## 4. In the code: a frame source

The player used to take one fixed camera and list of objects. A live file
**replaces** its whole scene on reload, so the player now asks a
**frame source** for them every frame:

```cpp
class frame_source{
public:
	virtual ~frame_source() = default;
	virtual camera& cam() = 0;
	virtual const std::vector<object*>& objects() = 0;
	virtual float seconds() = 0;
	virtual void poll(){}        // a chance to change the scene, once per frame
};
```

There are two kinds of frame source:

| Source | What it is |
|---|---|
| `fixed_source` | the scenes built in C++ (1–5): the same camera and objects forever |
| `live_scene` | a `.dan` file: `poll()` checks the file, and on a change builds a new `world` and swaps it in |

The player loop doesn't know which one it has:

```cpp
source.poll();
camera& cam = source.cam();
const std::vector<object*>& scene = source.objects();
```

This is **polymorphism**: one interface, several implementations, chosen at
run time through **virtual** functions. `= 0` makes a function **pure
virtual**: every kind of source must provide its own. The `virtual`
destructor makes sure deleting through a `frame_source*` cleans up the
right kind of object.

**Ownership:** `live_scene` holds its scene in a `std::unique_ptr<world>`.
On reload, the new world is built **first**, then
`current = std::move(next);` swaps it in, and the old world (its meshes,
objects and camera) is deleted automatically. If building the new one fails,
`current` is never touched.

**Walking survives a reload.** The free camera (20) lives in the player, not
in the scene. So when the scene is swapped, you stay where you walked to.

## 5. Tests

```
live reload:
  PASS  reads the file when it starts (1 object)
  PASS  writes the solver's report next to it (engine_test_live.dan.report)
  PASS  nothing changed: no reload
  PASS  the file was saved again: reloaded (2 objects)
  PASS  a broken version: the last good scene stays (still 2 objects)
  PASS  ... and the report says what's wrong, and that the old scene is still showing
```

The test sets each version's modified time a second apart on purpose:
some file systems only store times to the nearest second (or two), and two
saves within the same second would look like no change.

---

## Try it

1. `./main scenes/impact.dan`
2. Open `scenes/impact.dan` in an editor, add `box = cube small teal near sun`, and save.
3. Watch the window: the box appears, placed by the solver.
4. Now type `spher` somewhere and save: the window keeps the last good
   version, and `scenes/impact.dan.report` says what's wrong.
