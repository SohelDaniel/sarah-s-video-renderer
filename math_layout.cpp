#include "math_layout.h"
#include "font.h"
#include "scene_parser.h"   // edit_distance, for "did you mean"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>


// ---------------------------------------------------------------------------
//  The commands it knows
// ---------------------------------------------------------------------------
struct command{
	int codepoint;
	math_node::role spacing;
	bool italic;
};

static const std::map<std::string, command>& commands(){
	using r = math_node::role;
	static const std::map<std::string, command> table = {
		{"alpha", {0x3B1, r::ordinary, true}}, {"beta", {0x3B2, r::ordinary, true}}, {"gamma", {0x3B3, r::ordinary, true}},
		{"delta", {0x3B4, r::ordinary, true}}, {"epsilon", {0x3B5, r::ordinary, true}}, {"zeta", {0x3B6, r::ordinary, true}},
		{"eta", {0x3B7, r::ordinary, true}}, {"theta", {0x3B8, r::ordinary, true}}, {"iota", {0x3B9, r::ordinary, true}},
		{"kappa", {0x3BA, r::ordinary, true}}, {"lambda", {0x3BB, r::ordinary, true}}, {"mu", {0x3BC, r::ordinary, true}},
		{"nu", {0x3BD, r::ordinary, true}}, {"xi", {0x3BE, r::ordinary, true}}, {"pi", {0x3C0, r::ordinary, true}},
		{"rho", {0x3C1, r::ordinary, true}}, {"sigma", {0x3C3, r::ordinary, true}}, {"tau", {0x3C4, r::ordinary, true}},
		{"upsilon", {0x3C5, r::ordinary, true}}, {"phi", {0x3C6, r::ordinary, true}}, {"chi", {0x3C7, r::ordinary, true}},
		{"psi", {0x3C8, r::ordinary, true}}, {"omega", {0x3C9, r::ordinary, true}},
		{"Gamma", {0x393, r::ordinary, false}}, {"Delta", {0x394, r::ordinary, false}}, {"Theta", {0x398, r::ordinary, false}},
		{"Lambda", {0x39B, r::ordinary, false}}, {"Xi", {0x39E, r::ordinary, false}}, {"Pi", {0x3A0, r::ordinary, false}},
		{"Sigma", {0x3A3, r::ordinary, false}}, {"Phi", {0x3A6, r::ordinary, false}}, {"Psi", {0x3A8, r::ordinary, false}},
		{"Omega", {0x3A9, r::ordinary, false}},
		{"times", {0xD7, r::binary, false}}, {"cdot", {0x22C5, r::binary, false}}, {"pm", {0xB1, r::binary, false}},
		{"to", {0x2192, r::relation, false}}, {"le", {0x2264, r::relation, false}}, {"ge", {0x2265, r::relation, false}},
		{"ne", {0x2260, r::relation, false}}, {"approx", {0x2248, r::relation, false}},
		{"infty", {0x221E, r::ordinary, false}}, {"partial", {0x2202, r::ordinary, true}},
		{"sum", {0x2211, r::ordinary, false}}, {"int", {0x222B, r::ordinary, false}},
	};
	return table;
}

// ---------------------------------------------------------------------------
//  1. Parsing, by recursive descent (like docs/21, one function per rule):
//    row    = { item }
//    item   = atom [ "^" group ] [ "_" group ]       (in either order)
//    atom   = symbol | "{" row "}" | "\frac" group group | "\sqrt" group | "\" name
//    group  = "{" row "}" | a single atom
// ---------------------------------------------------------------------------
namespace{
struct math_parser{
	explicit math_parser(const std::string& text) : text(text){}
	const std::string& text;
	size_t pos = 0;
	std::string error;
	int column = 0;

	void fail(const std::string& message,size_t at){
		if(error.empty()){ error = message; column = int(at) + 1; }
	}
	bool done()const{ return pos >= text.size(); }

	std::unique_ptr<math_node> row(bool inside_braces){
		auto r = std::make_unique<math_node>();
		r->what = math_node::kind::row;
		while(!done() && error.empty()){
			char c = text[pos];
			if(c == '}'){
				if(!inside_braces) fail("a '}' with no '{' before it", pos);
				break;
			}
			if(c == ' '){ pos++; continue; }       // spaces don't matter in math (TeX ignores them too)
			auto it = item();
			if(it) r->parts.push_back(std::move(it));
		}
		return r;
	}

	std::unique_ptr<math_node> item(){
		auto base = atom();
		if(!base) return nullptr;
		std::unique_ptr<math_node> up, down;
		while(!done() && (text[pos] == '^' || text[pos] == '_') && error.empty()){
			bool is_up = text[pos] == '^';
			size_t at = pos++;
			if((is_up && up) || (!is_up && down)) fail(std::string("two ") + (is_up ? "'^'" : "'_'") + " in a row: use braces, like x^{ab}", at);
			auto g = group(is_up ? "after '^'" : "after '_'", at);
			if(is_up) up = std::move(g); else down = std::move(g);
		}
		if(!up && !down) return base;
		auto s = std::make_unique<math_node>();
		s->what = math_node::kind::scripts;
		s->base = std::move(base);
		s->up = std::move(up);
		s->down = std::move(down);
		return s;
	}

	std::unique_ptr<math_node> group(const std::string& where,size_t at){
		while(!done() && text[pos] == ' ') pos++;
		if(done()){ fail("something is missing " + where, at); return nullptr; }
		return atom();
	}

	std::unique_ptr<math_node> symbol(int codepoint,math_node::role role,bool italic){
		auto s = std::make_unique<math_node>();
		s->what = math_node::kind::symbol;
		s->codepoint = codepoint;
		s->spacing = role;
		s->italic = italic;
		return s;
	}

	std::unique_ptr<math_node> atom(){
		using r = math_node::role;
		size_t at = pos;
		char c = text[pos];
		if(c == '{'){
			pos++;
			auto inside = row(true);
			if(done() || text[pos] != '}'){ fail("this '{' is never closed with '}'", at); return inside; }
			pos++;
			return inside;
		}
		if(c == '\\'){
			pos++;
			std::string name;
			while(!done() && std::isalpha((unsigned char)text[pos])) name += text[pos++];
			if(name.empty()){ fail("'\\' should be followed by a name, like \\alpha", at); return nullptr; }
			if(name == "frac"){
				auto f = std::make_unique<math_node>();
				f->what = math_node::kind::fraction;
				f->parts.push_back(group("after \\frac: the top, like \\frac{1}{2}", at));
				f->parts.push_back(group("after \\frac{...}: the bottom, like \\frac{1}{2}", at));
				return f;
			}
			if(name == "sqrt"){
				auto s = std::make_unique<math_node>();
				s->what = math_node::kind::root;
				s->parts.push_back(group("after \\sqrt, like \\sqrt{2}", at));
				return s;
			}
			auto it = commands().find(name);
			if(it == commands().end()){
				// did you mean (docs/21): the closest known command
				std::string best;
				int best_distance = 1000;
				for(const std::string& known : {std::string("frac"), std::string("sqrt")}){
					int d = edit_distance(name, known);
					if(d < best_distance){ best_distance = d; best = known; }
				}
				for(const auto& entry : commands()){
					int d = edit_distance(name, entry.first);
					if(d < best_distance){ best_distance = d; best = entry.first; }
				}
				std::string hint = best_distance <= std::max(1, int(name.size()) / 3) ? " (did you mean \\" + best + "?)" : "";
				fail("unknown command \\" + name + hint, at);
				return nullptr;
			}
			return symbol(it->second.codepoint, it->second.spacing, it->second.italic);
		}
		pos++;
		if(std::isalpha((unsigned char)c)) return symbol(c, r::ordinary, true);
		if(std::isdigit((unsigned char)c) || c == '.') return symbol(c, r::ordinary, false);
		switch(c){
			case '+': return symbol('+', r::binary, false);
			case '-': return symbol(0x2212, r::binary, false);      // a real minus sign, not a hyphen
			case '*': return symbol(0x2217, r::binary, false);
			case '/': return symbol('/', r::ordinary, false);
			case '=': case '<': case '>': return symbol(c, r::relation, false);
			case '(': case '[': return symbol(c, r::open, false);
			case ')': case ']': return symbol(c, r::close, false);
			case ',': case ';': return symbol(c, r::punct, false);
			case '\'': return symbol(0x2032, r::ordinary, false);   // a prime: f'
			case '^': case '_': fail(std::string("'") + c + "' needs something before it, like x" + c + "2", at); return nullptr;
		}
		fail(std::string("unexpected '") + c + "'", at);
		return nullptr;
	}
};
}

math_parse_result parse_math(const std::string& tex){
	math_parser p(tex);
	math_parse_result result;
	result.tree = p.row(false);
	if(p.error.empty() && !p.done()) p.fail("a '}' with no '{' before it", p.pos);
	result.error = p.error;
	result.column = p.column;
	return result;
}

// ---------------------------------------------------------------------------
//  2. Layout (docs/30). Every piece becomes a box: width, height above the
//  baseline, depth below it. Then boxes are put side by side, raised and
//  stacked. The numbers are fractions of the size (em), like TeX's.
// ---------------------------------------------------------------------------
namespace{

// Moving a whole box by (dx, dy): all its glyphs and rules.
void place(math_box& into,const math_box& b,float dx,float dy){
	for(math_glyph g : b.glyphs){ g.x += dx; g.y += dy; into.glyphs.push_back(g); }
	for(math_rule r : b.rules){ r.x += dx; r.y += dy; into.rules.push_back(r); }
}

math_box lay(const math_node& n,float size);

// One character: as wide as its advance; as tall and deep as its ink
// (from the filled glyph, docs/29).
math_box lay_symbol(const math_node& n,float size){
	const font* face = n.italic ? fonts::italic() : fonts::serif();
	math_box b;
	if(!face) return b;
	const font::glyph& g = face->get(n.codepoint, size);
	b.width = face->advance(n.codepoint, size);
	b.height = std::max(0.0f, float(-g.top - 1));                 // the glyph's box has 1 pixel of padding
	b.depth = std::max(0.0f, float(g.top + g.height - 1));
	if(g.coverage.empty()){ b.height = 0.0f; b.depth = 0.0f; }     // a space
	b.glyphs.push_back({n.codepoint, face, size, 0.0f, 0.0f});
	return b;
}

// TeX's spacing between neighbours, in "mu" (1/18 of the size):
//   a binary operator (+ − ×) gets 4 mu on each side, a relation (= ≤ →) 5 mu,
//   and after a comma 3 mu. Nothing between ordinary things like m and c.
float space_between(math_node::role left,math_node::role right,float size){
	using r = math_node::role;
	float mu = size / 18.0f;
	if(left == r::relation || right == r::relation) return 5.0f * mu;
	if(left == r::binary || right == r::binary) return 4.0f * mu;
	if(left == r::punct) return 3.0f * mu;
	return 0.0f;
}

math_box lay_row(const math_node& n,float size){
	math_box b;
	math_node::role previous = math_node::role::ordinary;
	bool first = true;
	for(const auto& part : n.parts){
		math_node::role role = part->what == math_node::kind::symbol ? part->spacing : math_node::role::ordinary;
		// a + or - at the very start, or right after another operator, is a sign, not an operation
		if(role == math_node::role::binary && (first || previous == math_node::role::binary
		   || previous == math_node::role::relation || previous == math_node::role::open)) role = math_node::role::ordinary;
		if(!first) b.width += space_between(previous, role, size);
		math_box p = lay(*part, size);
		place(b, p, b.width, 0.0f);
		b.width += p.width;
		b.height = std::max(b.height, p.height);
		b.depth = std::max(b.depth, p.depth);
		previous = role;
		first = false;
	}
	return b;
}

// x^2: the exponent 70% of the size, its baseline raised 0.45 em;
// x_1: the index 70% too, lowered 0.2 em (0.25 em if there's an exponent too)
math_box lay_scripts(const math_node& n,float size){
	math_box b = lay(*n.base, size);
	float small = size * 0.7f;
	float after = b.width + size * 0.04f;   // a hair of space after the base
	float widest = 0.0f;
	math_box result = b;
	if(n.up){
		math_box u = lay(*n.up, small);
		float rise = size * 0.45f;
		place(result, u, after, -rise);
		result.height = std::max(result.height, rise + u.height);
		widest = std::max(widest, u.width);
	}
	if(n.down){
		math_box d = lay(*n.down, small);
		float drop = size * (n.up ? 0.25f : 0.2f);
		place(result, d, after, drop);
		result.depth = std::max(result.depth, drop + d.depth);
		widest = std::max(widest, d.width);
	}
	result.width = after + widest;
	return result;
}

// \frac{a}{b}: top and bottom at 85% size, centered, with a bar between.
// The bar sits on the "math axis", the height of a minus sign (0.27 em),
// so a fraction lines up with the + and = beside it.
math_box lay_fraction(const math_node& n,float size){
	float small = size * 0.85f;
	math_box top = n.parts[0] ? lay(*n.parts[0], small) : math_box{};
	math_box bottom = n.parts[1] ? lay(*n.parts[1], small) : math_box{};
	float thickness = std::max(1.0f, size * 0.05f);
	float axis = size * 0.27f;
	float gap = size * 0.12f;
	float pad = size * 0.1f;
	math_box b;
	b.width = std::max(top.width, bottom.width) + 2.0f * pad;
	float bar_top = -axis - thickness / 2.0f;
	float top_baseline = bar_top - gap - top.depth;                    // its bottom just above the bar
	float bottom_baseline = bar_top + thickness + gap + bottom.height; // its top just below the bar
	place(b, top, (b.width - top.width) / 2.0f, top_baseline);
	place(b, bottom, (b.width - bottom.width) / 2.0f, bottom_baseline);
	b.rules.push_back({0.0f, bar_top, b.width, thickness});
	b.height = -top_baseline + top.height;
	b.depth = bottom_baseline + bottom.depth;
	return b;
}

// \sqrt{x}: the radical sign, made big enough for what's inside, then a bar
// across the top of the inside.
math_box lay_root(const math_node& n,float size){
	math_box inside = n.parts[0] ? lay(*n.parts[0], size) : math_box{};
	const font* face = fonts::serif();
	math_box b;
	if(!face) return inside;
	float gap = size * 0.12f;
	float thickness = std::max(1.0f, size * 0.05f);
	float needed = inside.height + inside.depth + 2.0f * gap;
	const font::glyph& probe = face->get(0x221A, size);
	float sign_height = float(probe.height - 2);
	float sign_size = size * std::max(1.0f, needed / std::max(1.0f, sign_height));
	const font::glyph& sign = face->get(0x221A, sign_size);
	float sign_width = face->advance(0x221A, sign_size);
	// put the sign's top level with the bar, just above the inside
	float bar_top = -(inside.height + gap) - thickness;
	float shift = bar_top - float(sign.top + 1);
	b.glyphs.push_back({0x221A, face, sign_size, 0.0f, shift});
	place(b, inside, sign_width, 0.0f);
	b.rules.push_back({sign_width - size * 0.02f, bar_top, inside.width + size * 0.08f, thickness});
	b.width = sign_width + inside.width + size * 0.08f;
	b.height = -bar_top;
	b.depth = std::max(inside.depth, shift + float(sign.top + sign.height - 1));
	return b;
}

math_box lay(const math_node& n,float size){
	switch(n.what){
		case math_node::kind::symbol:   return lay_symbol(n, size);
		case math_node::kind::row:      return lay_row(n, size);
		case math_node::kind::scripts:  return lay_scripts(n, size);
		case math_node::kind::fraction: return lay_fraction(n, size);
		case math_node::kind::root:     return lay_root(n, size);
	}
	return {};
}
}

math_box layout_math(const math_node& tree,float size){
	return lay(tree, size);
}
