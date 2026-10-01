#include "shapes2d.h"
#include "timeline.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

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

flat_look create_look(float p,bool filled){
	p = std::clamp(p, 0.0f, 1.0f);
	flat_look look;
	if(p >= 1.0f) return look;                   // done: all of it
	if(!filled){
		look.drawn = smooth_curve(p);
		look.fill = 0.0f;
	}else{
		look.drawn = smooth_curve(std::min(1.0f, 2.0f * p));
		look.fill = smooth_curve(std::max(0.0f, 2.0f * p - 1.0f));
	}
	return look;
}
