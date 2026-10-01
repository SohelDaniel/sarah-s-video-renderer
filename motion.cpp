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
		case method::naive: place_naive(); method_name = "naive"; break;
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
	if(!hits.empty()){
		out << "  collisions:\n";
		for(const collision& c : hits){
			out << "    " << c.a << " hits " << c.b << ", first at " << c.first_time << " s (closest: "
			    << (c.closest < 0.0f ? "inside each other by " : "") << std::fabs(c.closest) << ")\n";
		}
	}
	return out.str();
}
