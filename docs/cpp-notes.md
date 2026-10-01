# C++ notes

Every piece of C++ syntax used in the engine that's worth explaining, with
where it's used.

---

## Files and headers

### `#pragma once`
At the top of every `.h`. It means "only include this file once per `.cpp`,
even if several headers include it". Without it, the same struct would be
defined twice and compilation would fail.

### Header (`.h`) vs source (`.cpp`)
The header says **what exists** (class, function signatures); the source
says **how it works**. Other files only include the header. Templates are
the exception: all their code lives in the header (see below).

### Forward declarations
```cpp
class object;        // camera.h
struct SDL_Window;   // window.h
```
"This type exists; the details come later." Enough to use a **pointer or
reference** to it. It breaks include cycles (`render.h` includes
`camera.h`, so `camera.h` can't include `render.h` back), and it keeps SDL
out of every file except `window.cpp`.

---

## Types

### `struct` vs `class`
Identical except for the default: `struct` members are public, `class`
members are private. The code uses `struct` for plain bundles of data
(`vec3`, `pose`, `time_span`) and `class` for things that protect their
insides (`object`, `camera`, `mesh`).

### Default member values
```cpp
float size = 1.0f;
vec3 position{0.0f, 0.0f, 0.0f};
```
Every new object starts with these values, with no constructor needed.

### Aggregate initialization
```cpp
motion.add({start, end}, ...);   // builds a time_span field by field
```

### `enum class`
```cpp
enum class size_word {tiny, small, normal, big, huge};
size_word s = size_word::big;
```
A type with a fixed list of named values. The `class` makes you write
`size_word::big`, not just `big`, so names from different enums can't
clash, and it won't silently turn into an int.

### `switch` on an `enum class`
```cpp
switch(s){
	case size_word::tiny:  return 0.3f;
	...
}
```
The compiler warns (`-Wall`) if a value of the enum is missing from the
switch, so adding a new size word can't be forgotten anywhere.

---

## Functions

### References and `const&`
```cpp
vec3 operator+(const vec3& v, const vec3& w);
```
`&` passes the original, not a copy. `const` promises not to change it. So
`const vec3&` means "read the caller's vec3 without copying it".

### `const` member functions
```cpp
vec3 get_position()const;
```
Promises the function doesn't change the object. It can be called on a
`const` object, and the compiler stops you if you accidentally modify
something.

### Operator overloading
```cpp
vec3 operator+(const vec3& v, const vec3& w);
mat4 operator*(const mat4& other) const;
float& operator[](int i);
```
Lets `a + b`, `M * p` and `v[0]` work on our own types, so the code reads
like the math.

### Function overloading
```cpp
void move(vec3 to);                        // instant
void move(vec3 to,float start,float end);  // over time
```
Same name, different parameters; the compiler picks one by the arguments.

### `::name`, the global scope
Inside `object`, `scale(...)` means the member `object::scale`.
`::scale(...)` means the global one from `transform.h`.

### Returning `*this` (chaining)
```cpp
object_spec& near(const std::string& other){ relations.push_back(...); return *this; }

spec.add("sphere", ...).near("cube").above("table");
```
Each call returns the same object, so the next call can be written right
after it. Reads like a sentence, and it's easy for an AI to write.

### Member initializer order
```cpp
std::map<std::string, mesh> meshes;   // world.h: declared first
layout solved;                        // declared second
...
world::world(...) : solved(spec, load_meshes(spec)) {}
```
Members are always built **in the order they're declared**, whatever order
the initializer list says. `load_meshes` fills `meshes`, so `meshes` has to
be declared above `solved`.

### `static` on a free function
```cpp
static void orbit(pose& p, ...);   // object.cpp
```
Only visible inside that one `.cpp`. A private helper.

### `explicit`
```cpp
explicit player(float seconds);
```
Stops C++ from silently turning a lone `float` into a `player`.

### `inline`
```cpp
inline mat4<float> translate(float x, float y, float z){ ... }
```
Needed for a function **defined** in a header: every `.cpp` that includes it
gets a copy, and `inline` tells the linker they're all the same function.

---

## Templates

```cpp
template<typename T>
class mat4 { T m[16]; ... };

template<typename State>
class timeline { State initial; ... };
```

Code written once for any type. `timeline<pose>` and
`timeline<viewpoint>` are two classes the compiler generates from the same
code. The compiler needs the full code wherever a new type is used, so
templates live entirely in headers.

### `std::initializer_list`
```cpp
mat4(std::initializer_list<T> values)
...
return mat4<float>{ 1, 0, 0, x,  0, 1, 0, y, ... };
```
Lets a matrix be written out row by row, like on paper.

---

## Lambdas and `std::function`

```cpp
[to](pose& p, float f){ p.position = lerp(p.position, to, f); }
```

A function with no name, written where it's needed.

- `[to]`: the **capture list**. It **copies** `to` into the lambda, so it's
  still there long after `move()` has returned. Capturing by reference
  (`[&to]`) would point at a variable that no longer exists by the time
  the lambda runs, 5 seconds later.
- `(pose& p, float f)`: parameters
- `{ ... }`: body

```cpp
using change = std::function<void(State& state, float progress)>;
```

`std::function` is a box that holds **anything callable** with that
signature. Every lambda has its own hidden type, so this is how different
lambdas fit in one `std::vector`. `using X = ...` gives a long type a short
name.

### Sorting with a lambda
```cpp
std::stable_sort(steps.begin(), steps.end(),
	[](const step& a, const step& b){ return a.when.start < b.when.start; });
```
The lambda answers "does a come before b?". **Stable** keeps equal elements
in their original order.

---

## Resources

### RAII
**Resource Acquisition Is Initialization**: the constructor grabs a resource
(a window, a file) and the destructor gives it back. `window` opens in its
constructor and closes in `~window()`, whether `play()` ends normally, by
`break` or by an exception. You can't forget to close it.

### Deleting copies
```cpp
window(const window&) = delete;
window& operator=(const window&) = delete;
```
A copy would mean two objects owning one OS window, and both would close
it.

### Exceptions
```cpp
throw std::runtime_error("could not open a window: " + error);
...
try { example_scene(); }
catch (const std::exception& e) { std::cerr << "error: " << e.what(); return 1; }
```
Errors travel up to the nearest `catch`. `main` catches everything, prints
it and exits with code 1 instead of crashing.

---

## The standard library bits

| Thing | Where | What for |
|---|---|---|
| `std::vector<T>` | everywhere | a growable array |
| `std::map<K, V>` | `world.cpp` | look up a mesh by file name. `try_emplace(key, args)` builds the value only if the key isn't there yet, so each mesh file is loaded once |
| `std::deque<T>` | `scene_spec.h` | like a vector, but adding to the end **never moves** existing elements, so a reference returned by `add()` stays valid. A vector may move everything to new memory when it grows |
| `std::ostringstream`, `<iomanip>` | `layout.cpp` | build the report as text: `std::fixed`, `std::setprecision(2)`, `std::setw(12)` for aligned columns |
| `std::string` | names, file names | text |
| `std::clamp(x, lo, hi)` | `timeline.h` | keep progress in 0..1 |
| `std::chrono::steady_clock` | `player.cpp` | measure real time |
| `std::filesystem::create_directories` | `camera.cpp` | make the output folder |
| `std::numeric_limits<float>::infinity()` | `render.cpp` | an "empty" depth value |
| `std::isfinite(x)` | `solver_test.cpp` | false for NaN and ±infinity: catches broken math |
| `std::fmod(a, b)` | `timeline.h` | the remainder of a / b for floats: looping (20) |
| `std::atan2(y, x)`, `std::asin(x)` | `fly_camera.cpp` | angles back from a direction; atan2 gets the quarter right from both signs (20) |
| `std::stoul(text, nullptr, 16)` | `scene_parser.cpp` | read `ff8800` as a hex number for `#rrggbb` colors |
| `std::isdigit`, `std::isalpha`, `std::isxdigit` | `scene_parser.cpp` | what kind of character is this (the lexer, 21) |
| `std::min({a, b, c})` | `scene_parser.cpp` | the smallest of a list (an `initializer_list`) |
| `std::printf("%-14s %4d", ...)` | `solver_test.cpp` | formatted columns: `-` = left-aligned, the number = width |
| exit code (`return 1` from `main`) | `solver_test.cpp` | non-zero tells `make` (and any script) that the tests failed |

### `<chrono>`, step by step
```cpp
using clock = std::chrono::steady_clock;
const clock::time_point started = clock::now();
float t = std::chrono::duration<float>(clock::now() - started).count();
```
`now()` gives a moment; subtracting two moments gives a duration;
`duration<float>` converts it to seconds; `.count()` takes the plain number
out.

### `if` with an initializer
```cpp
if(auto size = size_words.find(t.value); size != size_words.end()){ o.size = size->second; return; }
```
`if(declaration; condition)`: the variable only exists inside the `if`
(C++17). Good for "look it up, use it if found" (`scene_parser.cpp`).

### Exceptions for control flow: `[[noreturn]]`
```cpp
struct line_error{};
[[noreturn]] void fail(const token& where,const std::string& message);   // records, then throws line_error
...
try{ line(); }catch(const line_error&){ /* skip to the end of the line */ }
```
The parser's functions call each other several levels deep. When one finds
a mistake, `fail` records it and throws, which unwinds straight back to the
loop over lines (error recovery, 21). `[[noreturn]]` tells the compiler
`fail` never returns, so it doesn't warn about "missing return value" after
it. An empty struct is enough as the thing thrown: it's only a signal.

### A function template over a map
```cpp
template<typename T>
static std::vector<std::string> keys_of(const std::map<std::string, T>& m);
```
One function for the size, color, relation and view word lists, whatever
their value type.

### The ternary operator
```cpp
std::string which = argc > 1 ? argv[1] : "1";
```
`condition ? if_true : if_false`
