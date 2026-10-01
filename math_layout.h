#pragma once
#include <memory>
#include <string>
#include <vector>

class font;

// ============================================================================
//  A small TeX (docs/30): formulas like  F = G\frac{m_1 m_2}{r^2}.
//
//  1. parse_math: the text becomes a tree (a fraction has a top and a
//     bottom, a power has a base and an exponent, ...). No fonts needed, so
//     a .dan file can be checked for mistakes straight away.
//  2. layout_math: the tree becomes BOXES, the way TeX does it. Every box
//     has a width, a height (above the baseline) and a depth (below it).
//     Boxes are put side by side, raised, lowered, shrunk and stacked.
//
//  What it understands:
//    letters (italic), digits, + - = < > ( ) , . / ' and the space
//    x^2  x_1  x^{10}  x_{i,j}  x_1^2       powers and indices
//    \frac{a}{b}   \sqrt{x}                 fractions and roots
//    \alpha .. \omega, \Gamma .. \Omega     Greek
//    \times \cdot \pm \to \infty \le \ge \ne \approx \sum \int \partial
// ============================================================================

// one piece of the tree
struct math_node{
	enum class kind{ symbol, row, scripts, fraction, root };
	enum class role{ ordinary, binary, relation, open, close, punct };   // for spacing (docs/30)
	kind what = kind::row;
	role spacing = role::ordinary;
	int codepoint = 0;          // symbol: which character
	bool italic = false;        // symbol: a letter (italic) or not (upright)
	std::vector<std::unique_ptr<math_node>> parts;   // row: the pieces; fraction: top, bottom; root: inside
	std::unique_ptr<math_node> base, up, down;       // scripts: x, ^, _
};

struct math_parse_result{
	std::unique_ptr<math_node> tree;
	std::string error;          // empty = fine
	int column = 0;             // where the mistake is (1 = the first character)
};

math_parse_result parse_math(const std::string& tex);

// What a laid-out formula is made of, positioned from its left edge on its
// baseline: x right, y DOWN (so a raised exponent has a negative y).
struct math_glyph{
	int codepoint;
	const font* face;
	float size;
	float x, y;                 // the pen position (on the glyph's own baseline)
};
struct math_rule{
	float x, y, width, height;  // a filled bar (fraction lines, root tops): y = its top
};
struct math_box{
	float width = 0.0f, height = 0.0f, depth = 0.0f;
	std::vector<math_glyph> glyphs;
	std::vector<math_rule> rules;
};

// Lay a formula out at `size` pixels. The fonts must be there (docs/29).
math_box layout_math(const math_node& tree,float size);
