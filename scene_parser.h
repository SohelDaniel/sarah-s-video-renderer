#pragma once
#include "scene_spec.h"

#include <string>
#include <vector>

// ============================================================================
//  The dan language (docs/21): what the AI writes, in .dan files.
//
//      scene "solar" view front_above
//      sun    = sphere big gold important
//      rock   = icosahedron grey near sun
//      planet = octahedron red orbits sun 1 turn 0s-20s
//      comet  = pyramid small white
//      comet hits planet at 12s from 8s
//
//  One statement per line, `#` starts a comment. The parser turns it into
//  the same scene_spec the C++ scenes build by hand, so nothing after it
//  changes.
//
//  It never stops at the first mistake: every error is collected, each with
//  its line and column, and a "did you mean" when a word is close to a
//  known one. That list is the feedback an AI gets to fix its scene.
// ============================================================================

struct parse_message{
	int line = 0;
	int column = 0;
	std::string text;

	// "line 4, col 10: unknown shape 'spher' (did you mean sphere?)"
	std::string to_string()const;
};

struct parse_result{
	scene_spec spec;
	std::vector<parse_message> errors;   // the scene is wrong here
	std::vector<parse_message> notes;    // probably wrong, but the solver will report it too
	bool ok()const{ return errors.empty(); }
};

// Read a scene from text...
parse_result parse_scene(const std::string& text);
// ... or from a file (an error if it can't be opened).
parse_result parse_scene_file(const std::string& filename);

// How many single-letter edits (insert, delete, change, or swap two
// neighbours) turn a into b (docs/21). Public so the tests can check it.
int edit_distance(const std::string& a,const std::string& b);
