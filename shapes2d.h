#pragma once
#include "point2.h"

#include <string>
#include <vector>

// ============================================================================
//  Flat shapes (docs/40): Manim-style 2D drawings that really stand in the
//  3D world.
//
//  A shape is a few paths in its own flat plane (x right, y UP, z = 0),
//  fitting inside a circle of radius 1 around (0, 0), like a mesh fits
//  inside its bounding sphere. The world turns that plane to face the
//  scripted camera, so from there it looks exactly 2D; walk round it and
//  it's lines and flat color in space.
// ============================================================================

struct flat_path{
	enum kind_of{ outline, axis, curve, text };
	std::vector<point2> points;
	bool closed = true;      // a loop (back to the start), or an open line
	bool filled = false;     // its inside is colored too
	bool stroked = true;     // its outline is drawn (a graph's numbers are only filled)
	std::vector<std::vector<point2>> inner;   // more loops filled together with this one: a letter's holes (docs/42)
	kind_of role = outline;  // picks its color: the object's, or grey for axes
};

struct flat_shape{
	std::vector<flat_path> paths;
};

// The shape words of the dan language (docs/40).
const std::vector<std::string>& flat_shape_words();
// The shape for one of those words; `filled` fills its inside.
flat_shape make_flat_shape(const std::string& word,bool filled);

// The building blocks:
flat_shape circle_shape(int points = 64);
flat_shape polygon_shape(int corners);           // regular, a corner straight up
flat_shape star_shape(int tips = 5,float inner = 0.4f);

// How much of a shape is drawn right now (Create, docs/41). The defaults:
// all of it.
struct flat_look{
	float drawn = 1.0f;   // how much of each path's length is drawn, 0..1
	float fill = 1.0f;    // how far its fill has come up, 0..1
	float curve = 1.0f;   // a graph's curve: drawn after its axes (docs/42)
};

// The first `fraction` of a path along its length (0..1), as an open line
// of points: for a closed path the side back to the start counts too.
std::vector<point2> path_prefix(const flat_path& path,float fraction);

// Create (docs/41), p = 0..1 of the way through, like Manim's Create:
//   outline only:  the outline is drawn along its length the whole time:   drawn = smooth(p)
//   filled:        the outline in the first half:   drawn = smooth(min(1, 2p))
//                  then the fill comes up:          fill  = smooth(max(0, 2p − 1))
flat_look create_look(float p,bool filled,bool graph = false);

// A graph (docs/42): axes, ticks, numbers and the curve of f from x0 to x1,
// fitted inside the circle of radius 1. `digits` draws the numbers (none if
// it's null).
struct expr_node;
class font;
flat_shape graph_shape(const expr_node& f,float x0,float x1,const font* digits);
// A graph is something to read, like a board: for the same size word it's
// 2.5 times as big as the other shapes.
constexpr float graph_scale = 2.5f;
// The gap between ticks for a range this long: 1, 2 or 5 times a power of
// ten, the smallest that gives at most 10 ticks.
float tick_step(float range);
