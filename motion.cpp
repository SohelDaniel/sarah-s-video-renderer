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

	// 1. every motion that could work on its own
	std::vector<bool> moves(n, false);
	std::vector<int> target(n, -1);
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
		}else if(m.kind == motion_kind::hits && m.how == rate::there_and_back){
			parts.errors.push_back(what + ": there_and_back comes back, so it would never arrive"
			                       " (it stands still instead)");
		}else{
			moves[i] = true;
			target[i] = other;
		}
	}

	// 2. motions in a circle ("a orbits b, b orbits a") can't work: follow
	// each chain of centers, and if it comes back round, stop that one
	for(int i = 0;i<n;i++){
		if(!moves[i]) continue;
		int j = target[i];
		for(int steps = 0;steps<n && j >= 0 && moves[j];steps++){
			if(j == i){
				parts.errors.push_back(spec.objects[i].name + " " + motion_name(spec.objects[i].motions[0].kind) + " "
				                       + spec.objects[i].motions[0].other + ": the motions go round in a circle"
				                       " (it stands still instead)");
				moves[i] = false;
				break;
			}
			j = target[j];
		}
	}

	// 3. flying past something that moves isn't supported yet
	for(int i = 0;i<n;i++){
		if(moves[i] && spec.objects[i].motions[0].kind == motion_kind::flies_past && moves[target[i]]){
			parts.errors.push_back(spec.objects[i].name + " flies_past " + spec.objects[i].motions[0].other + ": "
			                       + spec.objects[i].motions[0].other + " moves too, and flying past something that"
			                       " moves isn't supported yet (it stands still instead)");
			moves[i] = false;
		}
	}

	// 4. the still scene, without any relation that points at a mover
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

vec3 path::at(float t,const vec3& center)const{
	float f = std::clamp((t - start) / (end - start), 0.0f, 1.0f);
	if(kind != motion_kind::orbits) f = shape(how, f);   // easing (docs/23)
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
	// an eased path covers the same distance in the same time, but at its
	// steepest moment it goes steepest(how) times faster than the average
	float fastest = kind == motion_kind::orbits ? 1.0f : steepest(how);
	return fastest * length / (end - start);
}

motion_plan::motion_plan(const std::vector<obstacle>& still,std::vector<path> moving)
	: still(still), moving(std::move(moving)){
	for(path& p : this->moving){
		if(p.around >= 0) p.center = still[p.around].position;
	}
}

vec3 motion_plan::target_position(size_t k,float t)const{
	const path& p = moving[k];
	return p.around_path >= 0 ? position(size_t(p.around_path), t) : p.center;
}

// Where moving object k is at time t. If it orbits something that moves,
// first ask where THAT is at t (docs/17), and so on up the chain. Something
// that hit its target sticks: it keeps the offset it had at the moment of
// impact and rides along (docs/18).
vec3 motion_plan::position(size_t k,float t)const{
	const path& p = moving[k];
	vec3 center = target_position(k, t);
	if(p.kind == motion_kind::hits && t >= p.end){
		return p.to + (center - target_position(k, p.end));
	}
	return p.at(t, center);
}

float motion_plan::speed_of(size_t k)const{
	const path& p = moving[k];
	return p.speed() + (p.around_path >= 0 ? speed_of(size_t(p.around_path)) : 0.0f);
}

int motion_plan::depth(size_t k)const{
	return moving[k].around_path >= 0 ? 1 + depth(size_t(moving[k].around_path)) : 0;
}

// A planet with a moon is, for planning its own orbit, a bigger object: the
// moon's circle reaches orbit_radius + the moon's own reach from the planet.
float motion_plan::reach(size_t k)const{
	float r = moving[k].radius;
	for(size_t j = 0;j<moving.size();j++){
		if(moving[j].around_path != int(k)) continue;
		if(moving[j].kind == motion_kind::orbits){
			r = std::max(r, moving[j].orbit_radius + reach(j));
		}else if(moving[j].kind == motion_kind::hits){
			// something stuck to it: touching it, and sticking out by its own size
			r = std::max(r, moving[k].radius + 2.0f * moving[j].radius);
		}
	}
	return r;
}

// An intended hit is allowed to touch its target, but only at the very end
// of its approach (the last 2·gap/speed seconds: the time it needs to cover
// the last bit of gap twice over) and while it's stuck afterwards. Hitting
// it EARLIER is still a collision, and so is touching anything else.
bool motion_plan::meant_to_touch(size_t k,int other_moving,int other_still,float t)const{
	auto pair = [&](size_t hitter,int target_moving,int target_still){
		const path& p = moving[hitter];
		if(p.kind != motion_kind::hits) return false;
		bool is_target = (target_moving >= 0 && p.around_path == target_moving)
		              || (target_still >= 0 && p.around_path < 0 && p.around == target_still);
		if(!is_target) return false;
		float window = 2.0f * layout::gap / std::max(p.speed(), 1e-4f);
		return t >= p.end - window;
	};
	if(pair(k, other_moving, other_still)) return true;
	if(other_moving >= 0 && pair(size_t(other_moving), int(k), -1)) return true;
	return false;
}

float motion_plan::hit_gap(size_t k)const{
	const path& p = moving[k];
	vec3 target = target_position(k, p.end);
	float target_radius = p.around_path >= 0 ? moving[p.around_path].radius : still[p.around].radius;
	return distance(position(k, p.end), target) - p.radius - target_radius;
}

void motion_plan::solve(method how){
	switch(how){
		case method::naive:  place_naive(); method_name = "naive"; break;
		case method::orbits: place_naive(); plan_orbits(); repair_orbits(); method_name = "orbits"; break;
		case method::flights:
			place_naive(); plan_orbits(); repair_orbits(); plan_hits(); plan_flights(); method_name = "flights"; break;
		case method::framed:
			place_naive(); plan_orbits(); repair_orbits(); plan_hits(); plan_flights(); method_name = "framed"; break;
	}
}

// Step E1: the simplest paths, checking nothing.
//   orbit   : as small as it can be without touching what it goes round
//   fly-by  : a straight line along x, right through the middle of it
void motion_plan::place_naive(){
	for(path& p : moving){
		if(p.kind == motion_kind::orbits){
			float center_radius = p.around_path >= 0 ? moving[p.around_path].radius : still[p.around].radius;
			p.orbit_radius = p.radius + center_radius + layout::gap;
			p.start_angle  = 0.0f;
		}else if(p.kind == motion_kind::hits){
			continue;   // below, once every orbit has its radius
		}else{
			const obstacle& other = still[p.around];
			float half = 2.0f * (p.radius + other.radius) + 3.0f;
			p.from = other.position - vec3(half, 0.0f, 0.0f);
			p.to   = other.position + vec3(half, 0.0f, 0.0f);
		}
	}
	// hits: straight in along x, to where the target will be at impact
	for(size_t k = 0;k<moving.size();k++){
		path& p = moving[k];
		if(p.kind != motion_kind::hits) continue;
		float target_radius = p.around_path >= 0 ? moving[p.around_path].radius : still[p.around].radius;
		float touch = p.radius + target_radius;
		vec3 x(1.0f, 0.0f, 0.0f);
		p.to   = target_position(k, p.end) + x * touch;
		p.from = p.to + x * (2.0f * touch + 3.0f);
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
//  ruled out by anything. Here r is the object's REACH: with a moon going
//  round it, a planet counts as big enough to hold the moon's circle
//  (docs/17), so if the planet's orbit is clear, so is its moon.
// ---------------------------------------------------------------------------
void motion_plan::plan_orbits(){
	// moons before planets: a planet's reach depends on its moons' orbits
	std::vector<size_t> order;
	for(size_t k = 0;k<moving.size();k++) order.push_back(k);
	std::stable_sort(order.begin(), order.end(), [&](size_t a,size_t b){ return depth(a) > depth(b); });

	std::vector<size_t> done;   // orbits that already have their radius
	for(size_t k : order){
		path& p = moving[k];
		if(p.kind != motion_kind::orbits) continue;
		bool center_moves = p.around_path >= 0;
		float center_radius = center_moves ? moving[p.around_path].radius : still[p.around].radius;
		float r = reach(k);   // the object, or the object with its moons

		std::vector<std::pair<float, float>> ruled_out;
		if(!center_moves){
			// still objects can only be in the way of orbits around still things;
			// a moon's circle moves along with its planet, and the planet's own
			// orbit (with the moon's reach) already keeps clear of them
			const obstacle& center = still[p.around];
			for(size_t j = 0;j<still.size();j++){
				if(int(j) == p.around) continue;
				vec3 d = still[j].position - center.position;
				float h = std::sqrt(d[0] * d[0] + d[2] * d[2]);
				float v = d[1];
				float need = r + still[j].radius + layout::gap;
				if(need <= std::fabs(v)) continue;   // far enough above or below: never in the way
				float w = std::sqrt(need * need - v * v);
				ruled_out.push_back({h - w, h + w});
			}
		}
		for(size_t q : done){
			if(moving[q].around != p.around || moving[q].around_path != p.around_path) continue;
			float need = r + reach(q) + layout::gap;
			ruled_out.push_back({moving[q].orbit_radius - need, moving[q].orbit_radius + need});
		}

		// the answer is either the smallest radius, or just past the end of
		// one of the ruled-out ranges: try those, smallest first
		float smallest = r + center_radius + layout::gap;
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
		done.push_back(k);
	}
}

// Does moving object k ever come too close to a still object? (Sampled,
// for paths where no exact formula applies.)
bool motion_plan::clear_of_still_sampled(size_t k)const{
	const path& p = moving[k];
	float dt = sample_step();
	float last = duration();
	for(int step = 0;;step++){
		float t = std::min(step * dt, last);
		vec3 here = position(k, t);
		for(size_t j = 0;j<still.size();j++){
			if(meant_to_touch(k, -1, int(j), t)) continue;
			if(distance(here, still[j].position) - p.radius - still[j].radius < layout::gap / 4.0f) return false;
		}
		if(t >= last) break;
	}
	return true;
}

// Orbits around DIFFERENT centers aren't compared by the exact rule above
// (their circles aren't around the same point), so two of them can cross.
// Crossing isn't the problem; being at the crossing at the same TIME is.
// So for an orbit that still collides with another orbit (fly-bys come
// later, and steer around the orbits):
//   1. try starting it at a different place on its circle: 45°, 90°, ...
//      (the circle stays the same, so nothing still can be in the way)
//   2. if no start works, make the circle 15% bigger and try again,
//      now checking the still objects too
void motion_plan::repair_orbits(){
	for(size_t k = 0;k<moving.size();k++){
		path& p = moving[k];
		if(p.kind != motion_kind::orbits || clear_of_moving(k, true)) continue;
		float first_radius = p.orbit_radius;
		bool fixed = false;
		for(int grow = 0;grow<6 && !fixed;grow++){
			if(grow > 0) p.orbit_radius *= 1.15f;
			for(int step = 0;step<8 && !fixed;step++){
				p.start_angle = step * two_pi / 8.0f;
				fixed = clear_of_moving(k, true) && (grow == 0 || clear_of_still_sampled(k));
			}
		}
		std::ostringstream note;
		note << std::fixed << std::setprecision(2);
		if(fixed){
			note << p.name << "'s orbit crossed another moving object's path; it now starts at "
			     << p.start_angle * 360.0f / two_pi << " degrees";
			if(p.orbit_radius != first_radius) note << ", radius " << first_radius << " -> " << p.orbit_radius;
		}else{
			p.orbit_radius = first_radius;
			p.start_angle = 0.0f;
			note << "no clear orbit found for " << p.name << " (it may collide)";
		}
		warnings.push_back(note.str());
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

// Does moving object k ever come too close to another moving object?
bool motion_plan::clear_of_moving(size_t k,bool orbits_only)const{
	const path& p = moving[k];
	float dt = sample_step();
	float last = duration();
	for(int step = 0;;step++){
		float t = std::min(step * dt, last);
		vec3 here = position(k, t);
		for(size_t j = 0;j<moving.size();j++){
			if(j == k || (orbits_only && moving[j].kind != motion_kind::orbits)) continue;
			if(meant_to_touch(k, int(j), -1, t)) continue;
			float surface_gap = distance(here, position(j, t)) - p.radius - moving[j].radius;
			if(surface_gap < layout::gap / 4.0f) return false;
		}
		if(t >= last) break;
	}
	return true;
}

// ---------------------------------------------------------------------------
//  Intended hits (docs/18).
//
//  "comet hits planet1 at 12 s": the comet must TOUCH planet1 at exactly
//  12 s. All paths are worked out in advance, so where planet1 will be at
//  12 s is known: target(12). Arriving from direction n (a unit vector),
//  the comet touches it when its center is at
//      contact = target(T) + n · (r_comet + r_target)
//  so the path is a straight line from  contact + n·L  to contact, over the
//  approach time. That intercepts a moving target exactly. The direction n
//  and the length L are picked like a fly-by's: the first one that doesn't
//  hit anything it isn't meant to.
// ---------------------------------------------------------------------------
void motion_plan::plan_hits(){
	const vec3 sides[6] = {
		vec3(0.0f, 0.0f, 1.0f), vec3(0.0f, 1.0f, 0.0f), vec3(1.0f, 0.0f, 0.0f),
		vec3(-1.0f, 0.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f), vec3(0.0f, 0.0f, -1.0f),
	};
	for(size_t k = 0;k<moving.size();k++){
		path& p = moving[k];
		if(p.kind != motion_kind::hits) continue;
		float target_radius = p.around_path >= 0 ? moving[p.around_path].radius : still[p.around].radius;
		float touch = p.radius + target_radius;
		vec3 target = target_position(k, p.end);

		bool found = false;
		for(float scale : {1.0f, 1.5f, 2.0f}){
			for(const vec3& n : sides){
				p.to   = target + n * touch;
				p.from = p.to + n * ((2.0f * touch + 3.0f) * scale);
				if(clear_of_still_sampled(k) && clear_of_moving(k)){
					found = true;
					break;
				}
			}
			if(found) break;
		}
		if(!found){
			warnings.push_back("no clear way in for " + p.name + " to hit its target (using the last one tried)");
		}
	}
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
	for(size_t k = 0;k<moving.size();k++){
		const path& p = moving[k];
		if(p.kind == motion_kind::orbits){
			// a moon's circle travels with its planet, and the planet's
			// spheres already include the moon's reach
			if(p.around_path >= 0) continue;
			// 32 spheres around the circle. A point of the circle is never
			// more than half a step's chord, 2·R·sin(π/64), from the nearest
			// one, so growing each by that covers the whole ring. One big
			// sphere would also work, but it's as tall as it is wide, and an
			// orbit is flat (docs/16).
			const int n = 32;
			float cover = reach(k) + 2.0f * p.orbit_radius * std::sin(3.14159265f / (2.0f * n));
			for(int k = 0;k<n;k++){
				float angle = two_pi * float(k) / float(n);
				vec3 point = p.center + vec3(p.orbit_radius * std::cos(angle), 0.0f, -p.orbit_radius * std::sin(angle));
				spheres.push_back({p.name, point, cover});
			}
		}else{
			// fly-by, or the approach of a hit (once stuck, it rides along
			// inside its target's reach)
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
	for(size_t k = 0;k<moving.size();k++) fastest = std::max(fastest, speed_of(k));
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
			vec3 p = position(i, t);
			// against everything standing still
			for(size_t j = 0;j<still.size();j++){
				if(meant_to_touch(i, -1, int(j), t)) continue;
				const obstacle& o = still[j];
				float surface_gap = distance(p, o.position) - moving[i].radius - o.radius;
				if(surface_gap < tolerance) record(moving[i].name, o.name, t, surface_gap);
			}
			// against the other moving objects
			for(size_t j = i + 1;j<moving.size();j++){
				if(meant_to_touch(i, int(j), -1, t)) continue;
				float surface_gap = distance(p, position(j, t)) - moving[i].radius - moving[j].radius;
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
	for(size_t k = 0;k<moving.size();k++){
		if(moving[k].kind != motion_kind::hits) continue;
		m.hits_planned++;
		if(std::fabs(hit_gap(k)) <= 0.01f) m.hits_on_time++;
	}
	return m;
}

std::string motion_plan::report()const{
	std::ostringstream out;
	out << std::fixed << std::setprecision(2);
	std::vector<collision> hits = find_collisions();

	float dt = sample_step();
	float fastest = 0.0f;
	for(size_t k = 0;k<moving.size();k++) fastest = std::max(fastest, speed_of(k));
	out << "motion (" << method_name << "): " << moving.size() << " moving objects, "
	    << hits.size() << " colliding pairs\n";
	if(moving.empty()) return out.str();
	out << "  checked every " << std::setprecision(4) << dt << " s (fastest moves " << fastest
	    << " per second)\n" << std::setprecision(2);

	out << "  paths:\n";
	for(const path& p : moving){
		std::string center = p.around_path >= 0 ? moving[p.around_path].name + " (which moves)" : still[p.around].name;
		out << "    " << std::left << std::setw(10) << p.name << std::right << " "
		    << motion_name(p.kind) << " " << center << ", "
		    << p.start << "-" << p.end << " s" << (p.how != rate::linear ? std::string(" ") + rate_name(p.how) : "") << ": ";
		if(p.kind == motion_kind::orbits){
			out << "radius " << p.orbit_radius << ", " << p.turns << " turns";
		}else if(p.kind == motion_kind::hits){
			out << "from (" << p.from[0] << ", " << p.from[1] << ", " << p.from[2] << "), touching at ("
			    << p.to[0] << ", " << p.to[1] << ", " << p.to[2] << ") at " << p.end << " s, then sticks";
		}else{
			out << "from (" << p.from[0] << ", " << p.from[1] << ", " << p.from[2] << ") to ("
			    << p.to[0] << ", " << p.to[1] << ", " << p.to[2] << ")";
		}
		out << "\n";
	}
	for(size_t k = 0;k<moving.size();k++){
		if(moving[k].kind != motion_kind::hits) continue;
		float g = hit_gap(k);
		out << "  planned hit: " << moving[k].name << " touches "
		    << (moving[k].around_path >= 0 ? moving[moving[k].around_path].name : still[moving[k].around].name)
		    << " at " << moving[k].end << " s (gap " << std::fabs(g) << ")"
		    << (std::fabs(g) <= 0.01f ? " as planned" : " NOT ON TIME") << "\n";
	}
	if(!warnings.empty()){
		out << "  notes from planning:\n";
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
