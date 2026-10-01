#pragma once
#include "math_layout.h"
#include "point2.h"

#include <string>
#include <vector>

class font;

// ============================================================================
//  Vector paths (docs/37): text and formulas as OUTLINES, not pictures.
//
//  A glyph from the font cache (29) is a finished picture of a letter: good
//  for drawing it still, useless for animating it. A vector path keeps the
//  letter as loops of points, so it can be drawn partly (Write, docs/38),
//  moved and resized smoothly (Transform, docs/39), filled, or just outlined.
//
//  The loops are polylines: the font's curves already flattened to within
//  0.25 px (29). Manim keeps them as Bezier curves instead; for drawing and
//  measuring, short straight pieces are just as good, and simpler.
// ============================================================================

// One piece of a text or formula: a letter, or a bar (a fraction line, the
// top of a root). Pieces are what Write draws one after the other, and what
// Transform matches between two formulas.
struct vpiece{
	int key = -1;                // the letter's codepoint, or -1 for a bar
	const font* face = nullptr;  // a letter: which font, and how big
	float size = 0.0f;
	point2 at{0.0f, 0.0f};       // a letter: its pen position, on its baseline; a bar: its top-left corner
	float width = 0.0f, height = 0.0f;   // a bar's size
	std::vector<std::vector<point2>> loops;   // the outline, measured from `at`, y down
};

// A whole text or formula. Its own coordinates: (0, 0) is the top-left of
// its box, and the baseline is `height` below that (like a math_box, 30).
struct vgroup{
	std::vector<vpiece> pieces;
	float width = 0.0f, height = 0.0f, depth = 0.0f;
};

// How one piece is drawn right now. The defaults: just there, filled.
struct piece_look{
	float scale_x = 1.0f;        // its size, compared with its own (Transform, docs/39):
	float scale_y = 1.0f;        // a letter grows evenly, a bar can get longer without getting thicker
	float fill = 1.0f;           // how filled in, 0..1
	float stroke = 0.0f;         // how much of its outline is drawn, 0..1 of the length
	float stroke_alpha = 0.0f;   // how strongly that outline shows, 0..1
	bool moving = false;         // gliding (docs/39): drawn from its loops even at its own size,
	                             // so it can sit between whole pixels instead of hopping
	bool still()const{ return !moving && scale_x == 1.0f && scale_y == 1.0f && fill == 1.0f && stroke_alpha == 0.0f; }
};

// A line of text in one font: one piece per letter that has ink (a space
// has none, so it gives no piece, but it still moves the pen).
vgroup text_paths(const std::string& text,const font& face,float size);
// A laid-out formula: one piece per glyph, and one per bar.
vgroup math_paths(const math_box& formula);

// How long a closed loop is: all its sides, including the one back to the start.
float loop_length(const std::vector<point2>& loop);
// The first `fraction` of a closed loop, measured along it (0..1): the
// points up to there, ending at a point in between if it stops mid-side.
std::vector<point2> loop_prefix(const std::vector<point2>& loop,float fraction);

// ---- Write (docs/38) ----
// Writing n pieces takes `progress` from 0 to 1. Each piece gets its own
// stretch of it, overlapping the next (Manim's lag_ratio):
//     w = 1 / (1 + lag·(n − 1))      how long each piece takes
//     piece i starts at i · lag · w  so the last one ends exactly at 1
// Returns how far piece i is, 0..1.
constexpr float write_lag = 0.2f;
float piece_progress(int i,int n,float progress);
// One piece being written, p = 0..1 (Manim's DrawBorderThenFill):
//   first half:  its outline grows along the loops (stroke 0 -> 1)
//   second half: the fill comes up (0 -> 1) as the outline fades (1 -> 0)
// Both halves eased with smooth (23). At p = 1 it's plainly there.
piece_look border_then_fill(float p);

// ---- Transform (docs/39) ----
// Which piece of `b` each piece of `a` turns into (-1: none, it fades out).
// The longest common subsequence of their keys (the same letter, or bar
// with bar), like a diff: as many matches as possible, all in reading order
// in both, so nothing crosses over. Ties go to the earliest place in b.
std::vector<int> match_pieces(const vgroup& a,const vgroup& b);
