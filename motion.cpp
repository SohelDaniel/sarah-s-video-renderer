#include "motion.h"
#include "layout.h"
#include "timeline.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>


static const float two_pi = 6.2831853f;

static float distance(const vec3& p,const vec3& q){
	vec3 d = p - q;
	return std::sqrt(dot(d, d));
}

static int find_name(const scene_spec& spec,const std::string& name){
	for(size_t j = 0;j<spec.objects.size();j++){
		if(spec.objects[j].name == name) return int(j);
	}
	return -1;
}

split_scene split_motion(const scene_spec& spec){
	split_scene parts;
	parts.still.view = spec.view;
	int n = int(spec.objects.size());

	// first decide who moves: they need a usable motion
	std::vector<bool> moves(n, false);
	for(int i = 0;i<n;i++){
		const object_spec& o = spec.objects[i];
		if(o.motions.empty()) continue;
		const motion& m = o.motions[0];
		std::string what = o.name + " " + motion_name(m.kind) + " " + m.other;
		if(o.motions.size() > 1){
			parts.errors.push_back(o.name + " has " + std::to_string(o.motions.size())
			                       + " motions; only one per object for now (using the first)");
		}
		int other = find_name(spec, m.other);
		if(other < 0){
			parts.errors.push_back(what + ": there is no object called \"" + m.other + "\" (it stands still instead)");
		}else if(other == i){
			parts.errors.push_back(what + ": it can't move around itself (it stands still instead)");
		}else if(!(m.end > m.start)){
			parts.errors.push_back(what + ": it ends before it starts (it stands still instead)");
		}else if(!spec.objects[other].motions.empty()){
			parts.errors.push_back(what + ": " + m.other + " moves too, and moving around something that"
			                       " moves isn't supported yet (it stands still instead)");
		}else{
			moves[i] = true;
		}
	}

	// then build the still scene, without any relation that points at a mover
	for(int i = 0;i<n;i++){
		if(moves[i]){
			parts.moving.push_back(i);
			if(!spec.objects[i].relations.empty()){
				parts.errors.push_back(spec.objects[i].name + " moves, so its other relations are ignored");
			}
			continue;
		}
		object_spec o = spec.objects[i];
		o.motions.clear();
		std::vector<relation> kept;
		for(const relation& r : o.relations){
			int other = find_name(spec, r.other);
			if(other >= 0 && moves[other]){
				parts.errors.push_back(o.name + " " + relation_name(r.kind) + " " + r.other + ": " + r.other
				                       + " moves, so it can't be used to place things (relation ignored)");
			}else{
				kept.push_back(r);
			}
		}
		o.relations = kept;
		parts.still.objects.push_back(o);
		parts.still_index.push_back(i);
	}
	return parts;
}

// ---------------------------------------------------------------------------
//  Paths
// ---------------------------------------------------------------------------

vec3 path::at(float t)const{
	float f = std::clamp((t - start) / (end - start), 0.0f, 1.0f);
	if(kind == motion_kind::orbits){
		// the same turn as rotate_y (docs/03): x' = x·cos + z·sin, z' = -x·sin + z·cos,
		// applied to the point (orbit_radius, 0, 0); so object::rotate_around
		// in world.cpp moves along exactly this circle
		float angle = start_angle + two_pi * turns * f;
		return center + vec3(orbit_radius * std::cos(angle), 0.0f, -orbit_radius * std::sin(angle));
	}
	return lerp(from, to, f);
}

float path::speed()const{
	float length = kind == motion_kind::orbits ? two_pi * orbit_radius * std::fabs(turns)
	                                           : distance(from, to);
	return length / (end - start);
}

motion_plan::motion_plan(const std::vector<obstacle>& still,std::vector<path> moving)
	: still(still), moving(std::move(moving)){
	for(path& p : this->moving){
		if(p.around >= 0) p.center = still[p.around].position;
	}
}

void motion_plan::solve(method how){
	switch(how){
		case method::naive:  place_naive(); method_name = "naive"; break;
		case method::orbits: place_naive(); plan_orbits(); method_name = "orbits"; break;
		case method::flights:
			place_naive(); plan_orbits(); plan_flights(); method_name = "flights"; break;
		case method::framed:
			place_naive(); plan_orbits(); plan_flights(); method_name = "framed"; break;
	}
}

// Step E1: the simplest paths, checking nothing.
//   orbit   : as small as it can be without touching what it goes round
//   fly-by  : a straight line along x, right through the middle of it
void motion_plan::place_naive(){
	for(path& p : moving){
		const obstacle& other = still[p.around];
		if(p.kind == motion_kind::orbits){
			p.orbit_radius = p.radius + other.radius + layout::gap;
			p.start_angle  = 0.0f;
		}else{
			float half = 2.0f * (p.radius + other.radius) + 3.0f;
			p.from = other.position - vec3(half, 0.0f, 0.0f);
			p.to   = other.position + vec3(half, 0.0f, 0.0f);
		}
	}
}

// ---------------------------------------------------------------------------
//  Step E2: orbit radii (docs/16).
//
//  An orbit is a circle of radius R around c, in the horizontal plane. How
//  close does it come to a still object at o? Measure o from c:
//      h = horizontal distance (in x and z),   v = height difference (y)
//  The nearest point of the circle is straight "out" towards o, so
//      closest distance = √((h − R)² + v²)
//  It must be at least  need = r_object + r_o + gap. Squaring and solving
//  for R: the circle is too close exactly when
//      |h − R| < w,   with  w = √(need² − v²)      (only if need > |v|)
//  so every still object rules out the radii between h − w and h + w.
//
//  Two orbits round the same center, R and R_q: every point of one circle
//  is at least |R − R_q| from the other, so they can never meet if
//  |R − R_q| ≥ r + r_q + gap. That rules out R_q − need .. R_q + need.
//
//  The orbit gets the smallest R (at least r + r_center + gap) that isn't
//  ruled out by anything.
// ---------------------------------------------------------------------------
void motion_plan::plan_orbits(){
	std::vector<int> done;   // orbits that already have their radius
	for(size_t k = 0;k<moving.size();k++){
		path& p = moving[k];
		if(p.kind != motion_kind::orbits) continue;
		const obstacle& center = still[p.around];

		std::vector<std::pair<float, float>> ruled_out;
		for(size_t j = 0;j<still.size();j++){
			if(int(j) == p.around) continue;
			vec3 d = still[j].position - center.position;
			float h = std::sqrt(d[0] * d[0] + d[2] * d[2]);
			float v = d[1];
			float need = p.radius + still[j].radius + layout::gap;
			if(need <= std::fabs(v)) continue;   // far enough above or below: never in the way
			float w = std::sqrt(need * need - v * v);
			ruled_out.push_back({h - w, h + w});
		}
		for(int q : done){
			if(moving[q].around != p.around) continue;
			float need = p.radius + moving[q].radius + layout::gap;
			ruled_out.push_back({moving[q].orbit_radius - need, moving[q].orbit_radius + need});
		}

		// the answer is either the smallest radius, or just past the end of
		// one of the ruled-out ranges: try those, smallest first
		float smallest = p.radius + center.radius + layout::gap;
		std::vector<float> tries = {smallest};
		for(const auto& range : ruled_out){
			if(range.second > smallest) tries.push_back(range.second);
		}
		std::sort(tries.begin(), tries.end());
		for(float R : tries){
			bool free = true;
			for(const auto& range : ruled_out){
				if(R > range.first + 1e-4f && R < range.second - 1e-4f) free = false;
			}
			if(free){
				p.orbit_radius = R;
				break;
			}
		}
		done.push_back(int(k));
	}
}

// ---------------------------------------------------------------------------
//  Step E3: fly-by paths (docs/16).
//
//  A fly-by is a straight segment from `from` to `to`. Candidates: lines
//  along x that pass the object at distance D = r + r_other + gap (times
//  1, 1.5, 2, 3, 4) in front of it, then above, below and behind it.
//
//  Against still objects the check is exact. The closest point of a
//  segment A→B to a point q is at
//      s = clamp( dot(q − A, B − A) / |B − A|² , 0, 1 )
//      closest = A + s·(B − A)
//  and it has to be at least r + r_q + gap away.
//
//  Against moving objects there's no simple formula (both move), so the
//  candidate is checked by time sampling, the same way as find_collisions.
//  The first candidate that's clear of both is used.
// ---------------------------------------------------------------------------
bool motion_plan::clear_of_still(const path& p)const{
	vec3 ab = p.to - p.from;
	float length2 = dot(ab, ab);
	for(const obstacle& o : still){
		float s = length2 > 0.0f ? std::clamp(dot(o.position - p.from, ab) / length2, 0.0f, 1.0f) : 0.0f;
		vec3 closest = p.from + ab * s;
		if(distance(closest, o.position) < p.radius + o.radius + layout::gap - 1e-4f) return false;
	}
	return true;
}

// the sample step if `extra` were added to the moving objects
float motion_plan::sample_step_with(const path& extra)const{
	float fastest = extra.speed();
	float last = extra.end;
	for(const path& p : moving){
		fastest = std::max(fastest, p.speed());
		last = std::max(last, p.end);
	}
	return std::max(layout::gap / (4.0f * fastest), last / 20000.0f);
}

// Does moving object k ever come too close to another moving object?
bool motion_plan::clear_of_moving(size_t k)const{
	const path& p = moving[k];
	float dt = sample_step_with(p);
	float last = duration();
	for(int step = 0;;step++){
		float t = std::min(step * dt, last);
		vec3 here = p.at(t);
		for(size_t j = 0;j<moving.size();j++){
			if(j == k) continue;
			float surface_gap = distance(here, moving[j].at(t)) - p.radius - moving[j].radius;
			if(surface_gap < layout::gap / 4.0f) return false;
		}
		if(t >= last) break;
	}
	return true;
}

void motion_plan::plan_flights(){
	const vec3 sides[4] = {
		vec3(0.0f, 0.0f, 1.0f),    // in front (towards the usual camera)
		vec3(0.0f, 1.0f, 0.0f),    // above
		vec3(0.0f, -1.0f, 0.0f),   // below
		vec3(0.0f, 0.0f, -1.0f),   // behind
	};
	for(size_t k = 0;k<moving.size();k++){
		path& p = moving[k];
		if(p.kind != motion_kind::flies_past) continue;
		const obstacle& other = still[p.around];
		float D = p.radius + other.radius + layout::gap;
		float half = 2.0f * (p.radius + other.radius) + 3.0f;
		vec3 along(half, 0.0f, 0.0f);

		bool found = false;
		for(float scale : {1.0f, 1.5f, 2.0f, 3.0f, 4.0f}){
			for(const vec3& side : sides){
				vec3 middle = other.position + side * (D * scale);
				p.from = middle - along;
				p.to   = middle + along;
				if(clear_of_still(p) && clear_of_moving(k)){
					found = true;
					break;
				}
			}
			if(found) break;
		}
		if(!found){
			warnings.push_back("no clear line for " + p.name + " flying past " + other.name
			                   + " (using the last one tried)");
		}
	}
}

std::vector<obstacle> motion_plan::bounds()const{
	std::vector<obstacle> spheres;
	for(const path& p : moving){
		if(p.kind == motion_kind::orbits){
			// 32 spheres around the circle. A point of the circle is never
			// more than half a step's chord, 2·R·sin(π/64), from the nearest
			// one, so growing each by that covers the whole ring. One big
			// sphere would also work, but it's as tall as it is wide, and an
			// orbit is flat (docs/16).
			const int n = 32;
			float cover = p.radius + 2.0f * p.orbit_radius * std::sin(3.14159265f / (2.0f * n));
			for(int k = 0;k<n;k++){
				float angle = two_pi * float(k) / float(n);
				vec3 point = p.center + vec3(p.orbit_radius * std::cos(angle), 0.0f, -p.orbit_radius * std::sin(angle));
				spheres.push_back({p.name, point, cover});
			}
		}else{
			spheres.push_back({p.name, p.from, p.radius});
			spheres.push_back({p.name, p.to, p.radius});
		}
	}
	return spheres;
}

const std::vector<path>& motion_plan::paths()const{
	return moving;
}

float motion_plan::duration()const{
	float last = 0.0f;
	for(const path& p : moving) last = std::max(last, p.end);
	return last;
}

// ---------------------------------------------------------------------------
//  Checking paths over time (docs/16).
//
//  Collisions are found by looking at the scene at many moments. How many?
//  Between two samples dt apart, nothing moves more than v_max·dt, so two
//  objects' distance changes by at most 2·v_max·dt. Any moment lies at most
//  dt/2 from a sample, so there the distance is off by at most v_max·dt.
//  Choose
//        dt = gap / (4 · v_max)        →   off by at most gap/4
//  and flag a pair whenever a sample shows them closer than (sum of radii
//  + gap/4). Then a real collision, at ANY moment, can't be missed.
// ---------------------------------------------------------------------------
float motion_plan::sample_step()const{
	float fastest = 0.0f;
	for(const path& p : moving) fastest = std::max(fastest, p.speed());
	if(fastest <= 0.0f) return 1.0f;
	float dt = layout::gap / (4.0f * fastest);
	// keep the number of samples sane for very fast objects
	return std::max(dt, duration() / 20000.0f);
}

std::vector<motion_plan::collision> motion_plan::find_collisions()const{
	std::vector<collision> found;
	if(moving.empty()) return found;
	float dt = sample_step();
	float tolerance = layout::gap / 4.0f;
	float last = duration();

	auto record = [&](const std::string& a,const std::string& b,float t,float surface_gap){
		for(collision& c : found){
			if(c.a == a && c.b == b){
				c.closest = std::min(c.closest, surface_gap);
				return;
			}
		}
		found.push_back({a, b, t, surface_gap});
	};

	for(int k = 0;;k++){
		float t = std::min(k * dt, last);
		for(size_t i = 0;i<moving.size();i++){
			vec3 p = moving[i].at(t);
			// against everything standing still
			for(const obstacle& o : still){
				float surface_gap = distance(p, o.position) - moving[i].radius - o.radius;
				if(surface_gap < tolerance) record(moving[i].name, o.name, t, surface_gap);
			}
			// against the other moving objects
			for(size_t j = i + 1;j<moving.size();j++){
				float surface_gap = distance(p, moving[j].at(t)) - moving[i].radius - moving[j].radius;
				if(surface_gap < tolerance) record(moving[i].name, moving[j].name, t, surface_gap);
			}
		}
		if(t >= last) break;
	}
	return found;
}

motion_plan::metrics motion_plan::measure()const{
	metrics m;
	m.moving = int(moving.size());
	m.collisions = int(find_collisions().size());
	return m;
}

std::string motion_plan::report()const{
	std::ostringstream out;
	out << std::fixed << std::setprecision(2);
	std::vector<collision> hits = find_collisions();

	float dt = sample_step();
	float fastest = 0.0f;
	for(const path& p : moving) fastest = std::max(fastest, p.speed());
	out << "motion (" << method_name << "): " << moving.size() << " moving objects, "
	    << hits.size() << " colliding pairs\n";
	if(moving.empty()) return out.str();
	out << "  checked every " << std::setprecision(4) << dt << " s (fastest moves " << fastest
	    << " per second)\n" << std::setprecision(2);

	out << "  paths:\n";
	for(const path& p : moving){
		out << "    " << std::left << std::setw(10) << p.name << std::right << " "
		    << motion_name(p.kind) << " " << still[p.around].name << ", "
		    << p.start << "-" << p.end << " s: ";
		if(p.kind == motion_kind::orbits){
			out << "radius " << p.orbit_radius << ", " << p.turns << " turns";
		}else{
			out << "from (" << p.from[0] << ", " << p.from[1] << ", " << p.from[2] << ") to ("
			    << p.to[0] << ", " << p.to[1] << ", " << p.to[2] << ")";
		}
		out << "\n";
	}
	if(!warnings.empty()){
		out << "  problems while planning:\n";
		for(const std::string& w : warnings) out << "    " << w << "\n";
	}
	if(!hits.empty()){
		out << "  collisions:\n";
		for(const collision& c : hits){
			out << "    " << c.a << " hits " << c.b << ", first at " << c.first_time << " s (closest: "
			    << (c.closest < 0.0f ? "inside each other by " : "") << std::fabs(c.closest) << ")\n";
		}
	}
	return out.str();
}
