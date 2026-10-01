# 21 · The dan language

Until now, scenes were written in C++ (`spec.add(...).near("cube")`). The AI
won't write C++. It writes a small text language made for exactly this,
**dan** (files end in `.dan`), one statement per line, and a **parser** turns
that into the very same `scene_spec`. Everything after it (solver, motion planner, renderer) doesn't
change at all.

```
# scenes/impact.dan
scene "impact"

sun    = sphere big gold important
rock   = icosahedron grey near sun
planet = octahedron red orbits sun 1 turn 0s-20s
comet  = pyramid small white
meteor = tetrahedron small yellow hits rock at 6s from 3s

comet hits planet at 12s from 8s sticks
```

```
./main scenes/impact.dan                 play it
./main scenes/impact.dan framed x.png 12 save the picture at 12 s
```

Code: `scene_parser.h/.cpp`, `scenes/*.dan`, the tests in `engine_test.cpp`.

---

## 1. The language

| Kind of line | Example | Means |
|---|---|---|
| header | `scene "solar" view front_above` | a name (for people) and where the camera looks from (14) |
| definition | `rock = icosahedron grey near sun` | a new object: its shape, then any properties |
| fact | `comet hits planet at 12s` | something more about an object defined anywhere in the file |
| comment | `# anything` | ignored, to the end of the line |

**Properties**, in any order after the shape:

| Property | Words |
|---|---|
| size (11) | `tiny small normal big huge` |
| color | `blue green purple yellow grey white red teal orange gold`, or `#rrggbb` |
| importance (11) | `important` |
| relation (11) | `near X`, `left_of X`, `right_of X`, `above X`, `below X`, `in_front_of X`, `behind X` |
| orbit (16) | `orbits X 2 turns 0s-20s` (the turns are optional: 1 if left out) |
| fly-by (16) | `flies_past X 4s-10s` |
| hit (18) | `hits X at 12s from 8s sticks` (`from` is optional: 4 s before; `sticks` too) |

Commas between properties are allowed and ignored: `box = cube near sun, above rock`.

### The grammar

Written in EBNF: `[ x ]` = optional, `{ x }` = any number of times,
`|` = or, quotes = the word itself.

```
scene      = { line }
line       = [ header | definition | fact ] end_of_line
header     = "scene" [ text ] [ "view" view_word ]
definition = name "=" shape { property }
fact       = name phrase { [","] phrase }
property   = size | color | "important" | phrase | ","
phrase     = relation_word name
           | "orbits" name [ number ("turn" | "turns") ] time "-" time
           | "flies_past" name time "-" time
           | "hits" name "at" time [ "from" time ] [ "sticks" ]
color      = color_word | "#rrggbb"
```

## 2. Step one: the lexer (text → tokens)

The lexer reads characters and groups them into **tokens**: the smallest
meaningful pieces. Each token remembers its line and column, so error
messages can point at the exact spot.

`planet = octahedron orbits sun 1 turn 0s-20s` becomes:

| column | token | kind |
|---|---|---|
| 1 | `planet` | word |
| 8 | `=` | equals |
| 10 | `octahedron` | word |
| 21 | `orbits` | word |
| 28 | `sun` | word |
| 32 | `1` | number |
| 34 | `turn` | word |
| 39 | `0s` | **time** |
| 41 | `-` | dash |
| 42 | `20s` | **time** |
| | | end of line |

The rules:

- **letters, digits, `_`**, starting with a letter: a word
- **digits**: a number, and if an `s` follows straight away, it's a **time**
  (`20s`; but `20 s` is a number and a word)
- **`"..."`**: text
- **`#`**: a color if exactly 6 hex digits follow (`#ff8800`), otherwise a
  comment to the end of the line. (`# ff8800` with a space is a comment.)
- spaces and tabs separate tokens; a newline is its own token, because
  lines matter in this language

## 3. Step two: the parser (tokens → scene_spec)

**Recursive descent:** every grammar rule is one function, and a rule calls
the functions of the rules inside it. So the program's call stack follows
the shape of the grammar.

Parsing the tokens above:

```
line()                                   first token 'planet', next is '=' → a definition
└─ definition()                          takes 'planet', '=', and the shape 'octahedron'
   │                                     → spec.add("planet", "shapes/octahedron.obj", ...)
   └─ property()  until the end of the line
      └─ phrase()                        'orbits' → an orbit
         ├─ name()     → "sun"
         ├─ number()   → 1, then expects 'turn' or 'turns' ✓
         ├─ time()     → 0
         ├─ expects '-' ✓
         └─ time()     → 20             → o.orbits("sun", 1, 0, 20)
```

The parser only ever looks at the **next** token to decide what to do
(`peek()`). That's enough for this grammar, and it keeps the parser simple.
The one place it looks two tokens ahead is at the start of a line: a name
followed by `=` is a definition, otherwise it's a fact.

### Two passes

`comet hits planet at 12s` may come **before** `planet = ...` in the file.
So facts are only remembered on the first pass, and applied on a second
pass, once every name is known. Definitions and headers are handled straight
away.

## 4. Mistakes: all of them, with suggestions

When the AI makes a mistake, the most useful thing is a precise message,
**for every mistake at once**, so it can fix them all in one go.

**Error recovery:** a mistake throws out of that line's functions, back to
the main loop. The loop records the error, skips to the end of the line,
and carries on with the next one. One bad line never hides the others.

`./main scenes/broken.dan`:

```
line 4, col 21: unknown view 'front_abuve' (did you mean front_above?); try front, front_above, left_above or right_above
line 5, col 8: expected a shape after '=', got 'cueb' (did you mean cube?)
line 6, col 17: unknown word 'blu' (did you mean blue?)
line 7, col 30: times need an 's': write 12s instead of 12
line 8, col 43: expected '-' between the start and end times, like 0s-20s
line 9, col 1: there is no object called 'sphre' to say this about (did you mean sphere?)
note: line 10, col 19: there is no object called 'planit' (did you mean planet?)
```

Six errors, one per broken line, each with its line and column. A scene with
errors doesn't play (exit code 1).

A **note** isn't an error: `near planit` is valid language, and the solver
would report the unknown name anyway (11). But here the parser can guess what
was meant.

One detail: on line 5, `cube = cueb ...` fails, but `cube` still counts as
**defined**. Otherwise every later line that mentions `cube` would also get
a "no object called cube" note, and one mistake would look like several.

### Did you mean: edit distance

How close is `cueb` to `cube`? The **edit distance**: the fewest single-letter
edits that turn one into the other. Allowed edits:

- insert a letter
- delete a letter
- change a letter
- **swap two neighbouring letters**

It's filled in as a table: `d[i][j]` = the distance between the first i
letters of one word and the first j of the other.

```
d[i][0] = i,  d[0][j] = j                        (delete or insert everything)
d[i][j] = the smallest of:
          d[i−1][j]   + 1                        delete
          d[i][j−1]   + 1                        insert
          d[i−1][j−1] + (same letter ? 0 : 1)    keep, or change
          d[i−2][j−2] + 1                        if the last two letters are swapped
```

**Worked example: `cueb` → `cube`**

|   | "" | c | u | b | e |
|---|---|---|---|---|---|
| **""** | 0 | 1 | 2 | 3 | 4 |
| **c** | 1 | 0 | 1 | 2 | 3 |
| **u** | 2 | 1 | 0 | 1 | 2 |
| **e** | 3 | 2 | 1 | 1 | 1 |
| **b** | 4 | 3 | 2 | 1 | **1** |

The bottom-right cell is the answer: **1**. It comes from the swap rule. The
last two letters `e b` are `b e` swapped, so it's d[2][2] + 1 = 0 + 1 = 1.
Without the swap rule (that's **Levenshtein** distance), that cell would be
min(1 + 1, 1 + 1, 1 + 1) = **2**: change e → b, then b → e.

The first version of the parser used plain Levenshtein, and the test
`did you mean cube` failed: 2 edits is too many for a 4-letter word to count
as a typo. Swapped letters are one of the most common typos, so the swap
rule went in. (This version is called **optimal string alignment**
distance.)

**When to suggest:** the closest known word, if it's within
max(1, length / 3) edits. So 1 edit for short words, more for long ones
(`icosahedrn` → `icosahedron` is 1, but even 3 would count for a 10-letter
word). Far-off words get no suggestion, rather than a silly one.

## 5. Tests

```
scene language:
  PASS  edit_distance(spher, sphere) = 1
  PASS  edit_distance(cueb, cube) = 1 (two neighbours swapped)
  PASS  edit_distance(kitten, sitting) = 3
  PASS  scenes/lazy.dan parses to exactly the C++ scene
  PASS  scenes/motion.dan parses to exactly the C++ scene
  PASS  scenes/impact.dan parses to exactly the C++ scene
  PASS  line 1: unknown view, did you mean front_above
  ...
  PASS  every broken line is reported, not just the first (6 errors)
```

**Exactly the C++ scene** compares every field of every object: name, shape,
color, size, importance, each relation, each motion. The named colors were
chosen to be the exact RGB values the C++ test scenes use, so scenes 3, 4
and 5 now exist in both forms, and the test proves they're identical.

---

## Try it on paper

1. Tokenize `comet hits planet at 12s from 8s`.
2. Fill in the edit-distance table for `blu` → `blue`.

Answers:
1. word(comet) word(hits) word(planet) word(at) time(12) word(from) time(8)
   end_of_line.
2. The last row is 3, 2, 1, 0, 1: the answer is **1** (insert the e).
