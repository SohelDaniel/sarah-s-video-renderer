#include "vpath.h"
#include "font.h"
#include "timeline.h"

#include <algorithm>
#include <cmath>


vgroup text_paths(const std::string& text,const font& face,float size){
	vgroup group;
	group.height = face.ascent(size);
	group.depth = face.descent(size);
	group.width = face.width(text, size);
	std::vector<int> codes = utf8_decode(text);
	float pen = 0.0f;
	for(size_t i = 0;i<codes.size();i++){
		std::vector<std::vector<point2>> loops = face.outline(codes[i], size);
		if(!loops.empty()){
			vpiece p;
			p.key = codes[i];
			p.face = &face;
			p.size = size;
			p.at = {pen, group.height};   // on the baseline
			p.loops = std::move(loops);
			group.pieces.push_back(std::move(p));
		}
		// the same steps as font::width (29), so the letters land where the glyph cache puts them
		pen += face.advance(codes[i], size);
		if(i + 1 < codes.size()) pen += face.kerning(codes[i], codes[i + 1], size);
	}
	return group;
}

vgroup math_paths(const math_box& formula){
	vgroup group;
	group.width = formula.width;
	group.height = formula.height;
	group.depth = formula.depth;
	for(const math_glyph& g : formula.glyphs){
		vpiece p;
		p.key = g.codepoint;
		p.face = g.face;
		p.size = g.size;
		p.at = {g.x, formula.height + g.y};   // g.y is measured from the formula's baseline
		p.loops = g.face->outline(g.codepoint, g.size);
		if(!p.loops.empty()) group.pieces.push_back(std::move(p));
	}
	for(const math_rule& r : formula.rules){
		vpiece p;
		p.at = {r.x, formula.height + r.y};
		p.width = r.width;
		p.height = r.height;
		// a bar is a rectangle: one loop of 4 corners, going round
		p.loops = {{{0.0f, 0.0f}, {r.width, 0.0f}, {r.width, r.height}, {0.0f, r.height}}};
		// In reading order (Write draws pieces in this order, docs/38): just
		// before the first letter that starts at or after the bar's left end,
		// so a fraction's line comes before its top and bottom.
		size_t where = group.pieces.size();
		for(size_t k = 0;k<group.pieces.size();k++){
			if(group.pieces[k].key >= 0 && group.pieces[k].at.x >= r.x - 0.01f){ where = k; break; }
		}
		group.pieces.insert(group.pieces.begin() + long(where), std::move(p));
	}
	return group;
}

static float distance(point2 a,point2 b){
	return std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
}

float loop_length(const std::vector<point2>& loop){
	float length = 0.0f;
	for(size_t i = 0;i<loop.size();i++) length += distance(loop[i], loop[(i + 1) % loop.size()]);
	return length;
}

// Walk along the loop, side by side, until the wanted length is used up.
// The side where it runs out is cut at the right point:
//      end = a + (b − a) · (left over / side length)
std::vector<point2> loop_prefix(const std::vector<point2>& loop,float fraction){
	std::vector<point2> part;
	if(loop.empty() || fraction <= 0.0f) return part;
	float left = loop_length(loop) * std::min(fraction, 1.0f);
	part.push_back(loop[0]);
	for(size_t i = 0;i<loop.size();i++){
		point2 a = loop[i], b = loop[(i + 1) % loop.size()];
		float side = distance(a, b);
		if(side >= left){
			float s = side > 0.0f ? left / side : 0.0f;
			part.push_back({a.x + (b.x - a.x) * s, a.y + (b.y - a.y) * s});
			return part;
		}
		part.push_back(b);
		left -= side;
	}
	return part;
}

float piece_progress(int i,int n,float progress){
	if(progress >= 1.0f) return 1.0f;              // exactly done (rounding could leave 0.9999999)
	if(n <= 1) return std::clamp(progress, 0.0f, 1.0f);
	float w = 1.0f / (1.0f + write_lag * float(n - 1));
	float start = float(i) * write_lag * w;
	return std::clamp((progress - start) / w, 0.0f, 1.0f);
}

piece_look border_then_fill(float p){
	piece_look look;
	if(p >= 1.0f) return look;                 // done: just there, like any still piece
	if(p < 0.5f){
		look.fill = 0.0f;
		look.stroke = smooth_curve(p / 0.5f);
		look.stroke_alpha = 1.0f;
	}else{
		float f = smooth_curve((p - 0.5f) / 0.5f);
		look.fill = f;
		look.stroke = 1.0f;
		look.stroke_alpha = 1.0f - f;
	}
	return look;
}
