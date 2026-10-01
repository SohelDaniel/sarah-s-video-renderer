#pragma once
#include "point2.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

// ============================================================================
//  Outline fonts (docs/29): smooth letters at any size.
//
//  A TrueType font stores each letter as OUTLINES: closed loops made of
//  straight lines and quadratic Bezier curves. stb_truetype (a public-domain
//  library, stb_truetype.h) is used only to read those outlines and the
//  spacing out of the .ttf file. Turning outlines into pixels is ours:
//    1. flatten the curves into short straight lines (de Casteljau)
//    2. for each pixel, measure how much of it is inside the shape, with the
//       nonzero winding rule, at 4 x 4 points per pixel
// ============================================================================

// UTF-8 text -> Unicode code points ("aπ" -> 97, 960). Bad bytes become '?'.
std::vector<int> utf8_decode(const std::string& text);

// A point on a quadratic Bezier curve from p0 to p2, pulled towards p1, at
// t in 0..1, by de Casteljau's construction (docs/29).
point2 bezier_point(point2 p0,point2 p1,point2 p2,float t);

// Split a quadratic Bezier into straight pieces until each is within
// `tolerance` pixels of the curve. Adds the points after p0 to `out`.
void flatten_quadratic(point2 p0,point2 p1,point2 p2,float tolerance,std::vector<point2>& out);

// How much of each pixel a set of closed loops covers (0..1), nonzero
// winding rule. The result is `width` x `height`, pixel (x, y) at
// coverage[y * width + x]; the loops' coordinates are in that same grid.
std::vector<float> fill_loops(const std::vector<std::vector<point2>>& loops,int width,int height,int samples = 4);

class font{
public:
	// Reads a .ttf file. Throws if it can't.
	explicit font(const std::string& filename);
	~font();
	font(const font&) = delete;
	font& operator=(const font&) = delete;

	// At a size of `size` pixels (the letters' em height):
	float ascent(float size)const;                    // how far the tallest letters reach above the baseline
	float descent(float size)const;                   // how far letters like 'g' reach below it (positive)
	float advance(int codepoint,float size)const;     // how far to move right after this letter
	float kerning(int a,int b,float size)const;       // extra (usually negative) space between a and b
	float width(const std::string& text,float size)const;   // advances plus kerning, all added up

	// One letter, ready to blend: its coverage, and where its top-left
	// corner goes relative to the pen (x right from the pen, y down from the
	// baseline). Cached, so each letter at each size is only filled once.
	struct glyph{
		int width = 0, height = 0;
		int left = 0, top = 0;
		std::vector<float> coverage;
	};
	const glyph& get(int codepoint,float size)const;

	// The letter's outline, flattened, in pixels: y down, the pen at (0, 0)
	// on the baseline. (get() fills this.)
	std::vector<std::vector<point2>> outline(int codepoint,float size)const;

private:
	struct data;                                      // the stb_truetype bits, kept out of this header
	std::unique_ptr<data> d;
	mutable std::map<std::pair<int, int>, glyph> cache;   // (codepoint, size in 1/4 pixels) -> glyph
};

// The engine's three fonts, loaded once from fonts/ (docs/29). nullptr if a
// file is missing; text then falls back to the 8x8 bitmap font (docs/27).
namespace fonts{
	const font* sans();          // DejaVu Sans: titles and labels
	const font* serif();         // DejaVu Serif: digits and symbols in math (docs/30)
	const font* italic();        // DejaVu Serif Italic: letters in math
}
