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
	float scale = 1.0f;          // its size, compared with its own (Transform)
	float fill = 1.0f;           // how filled in, 0..1
	float stroke = 0.0f;         // how much of its outline is drawn, 0..1 of the length
	float stroke_alpha = 0.0f;   // how strongly that outline shows, 0..1
	bool still()const{ return scale == 1.0f && fill == 1.0f && stroke_alpha == 0.0f; }
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
