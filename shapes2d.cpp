#include "shapes2d.h"
#include "timeline.h"
#include "expression.h"
#include "font.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <cstdio>

static const float pi = 3.14159265f;

const std::vector<std::string>& flat_shape_words(){
	static const std::vector<std::string> words = {"circle", "square", "triangle", "hexagon", "star"};
	return words;
}

// A point on the circle of radius r, at `angle` from straight up, going
// clockwise as seen from the front: up, right, down, left.
static point2 around(float r,float angle){
	return {r * std::sin(angle), r * std::cos(angle)};
}

flat_shape circle_shape(int points){
	flat_path p;
	for(int k = 0;k<points;k++) p.points.push_back(around(1.0f, 2.0f * pi * float(k) / float(points)));
	return {{p}};
}

// The corners sit on the circle of radius 1, so a square's sides are
// √2 = 1.414 long and it fits the same circle as everything else.
flat_shape polygon_shape(int corners){
	flat_path p;
	float turn = corners == 4 ? pi / 4.0f : 0.0f;   // a square stands on a side, not a corner
	for(int k = 0;k<corners;k++) p.points.push_back(around(1.0f, turn + 2.0f * pi * float(k) / float(corners)));
	return {{p}};
}

// Tips on the circle of radius 1, and the dents between them on a smaller
// one (radius `inner`), half way round between two tips.
flat_shape star_shape(int tips,float inner){
	flat_path p;
	for(int k = 0;k<2 * tips;k++){
		float r = k % 2 == 0 ? 1.0f : inner;
		p.points.push_back(around(r, pi * float(k) / float(tips)));
	}
	return {{p}};
}

flat_shape make_flat_shape(const std::string& word,bool filled){
	flat_shape s;
	if(word == "circle")        s = circle_shape();
	else if(word == "square")   s = polygon_shape(4);
	else if(word == "triangle") s = polygon_shape(3);
	else if(word == "hexagon")  s = polygon_shape(6);
	else if(word == "star")     s = star_shape();
	else throw std::invalid_argument("there is no flat shape called \"" + word + "\"");
	for(flat_path& p : s.paths) p.filled = filled && p.closed;
	return s;
}

static float distance(point2 a,point2 b){
	return std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
}

// Walk along the path's sides until the wanted length is used up, and cut
// the side where it runs out (the same walk as loop_prefix, docs/37).
std::vector<point2> path_prefix(const flat_path& path,float fraction){
	std::vector<point2> points = path.points;
	if(path.closed && !points.empty()) points.push_back(points.front());   // the side back to the start
	std::vector<point2> part;
	if(points.empty() || fraction <= 0.0f) return part;
	float length = 0.0f;
	for(size_t i = 0;i + 1<points.size();i++) length += distance(points[i], points[i + 1]);
	float left = length * std::min(fraction, 1.0f);
	part.push_back(points[0]);
	for(size_t i = 0;i + 1<points.size();i++){
		float side = distance(points[i], points[i + 1]);
		if(side >= left && fraction < 1.0f){
			float s = side > 0.0f ? left / side : 0.0f;
			part.push_back({points[i].x + (points[i + 1].x - points[i].x) * s, points[i].y + (points[i + 1].y - points[i].y) * s});
			return part;
		}
		part.push_back(points[i + 1]);
		left -= side;
	}
	return part;
}

flat_look create_look(float p,bool filled,bool graph){
	p = std::clamp(p, 0.0f, 1.0f);
	flat_look look;
	if(p >= 1.0f) return look;                   // done: all of it
	if(graph){
		// a graph (docs/42): the axes, ticks and numbers over the first 40%,
		// then the curve, left to right, over the rest
		look.drawn = smooth_curve(std::min(1.0f, p / 0.4f));
		look.fill = look.drawn;
		look.curve = smooth_curve(std::clamp((p - 0.4f) / 0.6f, 0.0f, 1.0f));
		return look;
	}
	if(!filled){
		look.drawn = smooth_curve(p);
		look.fill = 0.0f;
	}else{
		look.drawn = smooth_curve(std::min(1.0f, 2.0f * p));
		look.fill = smooth_curve(std::max(0.0f, 2.0f * p - 1.0f));
	}
	look.curve = look.drawn;
	return look;
}

// ---------------------------------------------------------------------------
//  Graphs (docs/42).
// ---------------------------------------------------------------------------

// 1, 2 or 5 times a power of ten: the smallest that gives at most 10 ticks.
//   a range of 6.28: 0.5 would give 12.6 ticks, too many; 1 gives 6.28: so 1
float tick_step(float range){
	float power = std::pow(10.0f, std::floor(std::log10(range / 10.0f)));
	for(float m : {1.0f, 2.0f, 5.0f, 10.0f}){
		if(range / (m * power) <= 10.0f) return m * power;
	}
	return 10.0f * power;
}

// "3", "−2", "0.5": whole numbers when the ticks are, one decimal when not.
// A real minus sign (U+2212), like the formulas (30).
static std::string tick_text(float v,float step){
	char buffer[32];
	if(step >= 1.0f) std::snprintf(buffer, sizeof buffer, "%d", int(std::lround(std::fabs(v))));
	else             std::snprintf(buffer, sizeof buffer, "%.1f", std::fabs(v));
	return (v < -1e-6f ? "\xE2\x88\x92" : "") + std::string(buffer);
}

// Words on the plane: each letter's outline (29), as filled loops, `em`
// units tall. (x, baseline) is where the text starts; y is UP here, so the
// font's y (down) is flipped.
static void add_text(flat_shape& shape,const font& face,const std::string& text,float x,float baseline,float em){
	const float size = 100.0f;                 // outlines at 100 px, so the curves are fine, then scaled down
	float scale = em / size;
	std::vector<int> codes = utf8_decode(text);
	float pen = 0.0f;
	for(size_t i = 0;i<codes.size();i++){
		// one path per letter: its first loop, and the rest (the hole of an
		// "0", the two of an "8") filled together with it, so the winding
		// rule can leave the holes empty (29)
		flat_path p;
		p.role = flat_path::text;
		p.filled = true;
		p.stroked = false;
		for(const auto& loop : face.outline(codes[i], size)){
			std::vector<point2> placed;
			for(const point2& q : loop) placed.push_back({x + (pen + q.x) * scale, baseline - q.y * scale});
			if(p.points.empty()) p.points = placed;
			else p.inner.push_back(placed);
		}
		if(!p.points.empty()) shape.paths.push_back(p);
		pen += face.advance(codes[i], size);
		if(i + 1 < codes.size()) pen += face.kerning(codes[i], codes[i + 1], size);
	}
}

static float text_width(const font& face,const std::string& text,float em){
	return face.width(text, 100.0f) * em / 100.0f;
}

flat_shape graph_shape(const expr_node& f,float x0,float x1,const font* digits){
	flat_shape shape;
	// 1. the curve's values, at 200 points
	const int n = 200;
	std::vector<float> xs(n), ys(n), seen;
	for(int i = 0;i<n;i++){
		xs[size_t(i)] = x0 + (x1 - x0) * float(i) / float(n - 1);
		ys[size_t(i)] = evaluate(f, xs[size_t(i)]);
		if(std::isfinite(ys[size_t(i)])) seen.push_back(ys[size_t(i)]);
	}
	// 2. the y range: leave out the most extreme 2% at each end (tan and 1/x
	// shoot off to infinity near their asymptotes), take 0 in if it's close,
	// and add 10% at each end
	float lo = -1.0f, hi = 1.0f;
	if(!seen.empty()){
		std::sort(seen.begin(), seen.end());
		lo = seen[size_t(0.02f * float(seen.size() - 1))];
		hi = seen[size_t(0.98f * float(seen.size() - 1))];
	}
	if(lo > 0.0f && lo < 0.25f * (hi - lo)) lo = 0.0f;
	if(hi < 0.0f && -hi < 0.25f * (hi - lo)) hi = 0.0f;
	if(hi - lo < 1e-6f){ lo -= 1.0f; hi += 1.0f; }
	float pad = 0.1f * (hi - lo);
	lo -= pad;
	hi += pad;

	// 3. the box: 1.6 times as wide as tall, its corners on the circle of
	// radius 0.9 (room for the numbers inside radius 1)
	const float aspect = 1.6f;
	float w = 2.0f * 0.9f * aspect / std::sqrt(1.0f + aspect * aspect), h = w / aspect;
	auto X = [&](float x){ return -w / 2.0f + (x - x0) / (x1 - x0) * w; };
	auto Y = [&](float y){ return -h / 2.0f + (y - lo) / (hi - lo) * h; };

	// 4. the axes, through 0 if 0 is in range, otherwise along the edge,
	// each with an arrow tip at its far end
	float ax_y = lo <= 0.0f && 0.0f <= hi ? Y(0.0f) : -h / 2.0f;
	float ax_x = x0 <= 0.0f && 0.0f <= x1 ? X(0.0f) : -w / 2.0f;
	auto line = [&](std::vector<point2> pts,flat_path::kind_of role){
		flat_path p;
		p.points = std::move(pts);
		p.closed = false;
		p.role = role;
		shape.paths.push_back(p);
	};
	const float tip = 0.05f;
	line({{-w / 2.0f, ax_y}, {w / 2.0f, ax_y}}, flat_path::axis);
	line({{w / 2.0f - tip, ax_y + tip * 0.5f}, {w / 2.0f, ax_y}, {w / 2.0f - tip, ax_y - tip * 0.5f}}, flat_path::axis);
	line({{ax_x, -h / 2.0f}, {ax_x, h / 2.0f}}, flat_path::axis);
	line({{ax_x - tip * 0.5f, h / 2.0f - tip}, {ax_x, h / 2.0f}, {ax_x + tip * 0.5f, h / 2.0f - tip}}, flat_path::axis);

	// 5. ticks and their numbers (0 is left out where the axes cross)
	const float em = 0.09f, tick = 0.02f;
	float sx = tick_step(x1 - x0), sy = tick_step(hi - lo);
	for(float k = std::ceil(x0 / sx);k * sx <= x1 + 1e-4f;k += 1.0f){
		float v = k * sx, px = X(v);
		if(std::fabs(v) < 1e-6f && ax_x == X(0.0f)) continue;
		if(px > w / 2.0f - tip) continue;          // under the arrow tip
		line({{px, ax_y - tick}, {px, ax_y + tick}}, flat_path::axis);
		if(digits){
			std::string t = tick_text(v, sx);
			add_text(shape, *digits, t, px - text_width(*digits, t, em) / 2.0f, ax_y - tick - 0.015f - em * 0.75f, em);
		}
	}
	for(float k = std::ceil(lo / sy);k * sy <= hi + 1e-4f;k += 1.0f){
		float v = k * sy, py = Y(v);
		if(std::fabs(v) < 1e-6f && ax_y == Y(0.0f)) continue;
		if(py > h / 2.0f - tip) continue;
		line({{ax_x - tick, py}, {ax_x + tick, py}}, flat_path::axis);
		if(digits){
			std::string t = tick_text(v, sy);
			add_text(shape, *digits, t, ax_x - tick - 0.015f - text_width(*digits, t, em), py - em * 0.35f, em);
		}
	}

	// 6. the curve: one open path for every stretch where it's defined and
	// in range; a gap wherever it isn't
	flat_path piece;
	piece.closed = false;
	piece.role = flat_path::curve;
	for(int i = 0;i<n;i++){
		float y = ys[size_t(i)];
		bool inside = std::isfinite(y) && y >= lo && y <= hi;
		if(inside) piece.points.push_back({X(xs[size_t(i)]), Y(y)});
		if((!inside || i == n - 1) && !piece.points.empty()){
			if(piece.points.size() >= 2) shape.paths.push_back(piece);
			piece.points.clear();
		}
	}
	return shape;
}

// ---------------------------------------------------------------------------
//  Morph (docs/43).
// ---------------------------------------------------------------------------

std::vector<point2> resample(const std::vector<point2>& loop,int n){
	std::vector<point2> out;
	if(loop.empty() || n <= 0) return out;
	size_t m = loop.size();
	auto side_length = [&](size_t i){ return distance(loop[i], loop[(i + 1) % m]); };
	float length = 0.0f;
	for(size_t i = 0;i<m;i++) length += side_length(i);
	// walk round the loop once, dropping a point every length / n
	size_t side = 0;
	float before = 0.0f;   // the length of all the sides before `side`
	for(int k = 0;k<n;k++){
		float want = length * float(k) / float(n);
		while(side + 1 < m && before + side_length(side) < want){
			before += side_length(side);
			side++;
		}
		point2 a = loop[side], b = loop[(side + 1) % m];
		float len = side_length(side);
		float s = len > 0.0f ? (want - before) / len : 0.0f;
		out.push_back({a.x + (b.x - a.x) * s, a.y + (b.y - a.y) * s});
	}
	return out;
}

// The shoelace formula: half the sum of x_i·y_{i+1} − x_{i+1}·y_i
float signed_area(const std::vector<point2>& loop){
	float twice = 0.0f;
	for(size_t i = 0;i<loop.size();i++){
		const point2& p = loop[i];
		const point2& q = loop[(i + 1) % loop.size()];
		twice += p.x * q.y - q.x * p.y;
	}
	return twice / 2.0f;
}

std::vector<point2> align_loop(const std::vector<point2>& a,std::vector<point2> b){
	size_t n = b.size();
	if(n == 0 || a.size() != n) return b;
	// turned the other way round: reverse it (keeping its first point first)
	if((signed_area(a) < 0.0f) != (signed_area(b) < 0.0f)) std::reverse(b.begin() + 1, b.end());
	size_t best = 0;
	float least = 1e30f;
	for(size_t k = 0;k<n;k++){
		float sum = 0.0f;
		for(size_t i = 0;i<n;i++){
			const point2& p = a[i];
			const point2& q = b[(i + k) % n];
			sum += (p.x - q.x) * (p.x - q.x) + (p.y - q.y) * (p.y - q.y);
		}
		if(sum < least){ least = sum; best = k; }
	}
	std::vector<point2> out(n);
	for(size_t i = 0;i<n;i++) out[i] = b[(i + best) % n];
	return out;
}
