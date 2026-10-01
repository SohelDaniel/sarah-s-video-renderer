#pragma once
#include "scene_spec.h"
#include "vec3.h"

#include <string>
#include <vector>

// ============================================================================
//  Placement for things that MOVE (docs/16).
//
//  The layout solver (layout.h) places everything that stands still. Then
//  the motion planner gives every moving object a path: a circle for
//  "orbits", a straight line for "flies_past". It checks the paths over
//  time for collisions by sampling them often enough that no collision can
//  slip between two samples.
//
//  The steps:
//    naive  : the simplest path (smallest orbit, a line straight through)
//    orbits : every orbit gets the smallest radius whose whole circle stays
//             clear of the still objects and of the other orbits around the
//             same center (fly-bys still naive)
//    flights: orbits as above, and every fly-by tries lines in front of,
//             above, below and behind what it passes, and takes the first
//             one that never comes too close to anything
//    framed : flights, and the automatic camera also fits every path, so
//             nothing leaves the picture while it moves (done in solver.cpp)
// ============================================================================


// The description, split into what stands still and what moves.
struct split_scene{
	scene_spec still;                 // only the objects that stand still
	std::vector<int> still_index;     // still.objects[k] is object still_index[k] of the full scene
	std::vector<int> moving;          // indices (in the full scene) of the objects that move
	std::vector<std::string> errors;  // problems with motions in the description
};

// Separates the moving objects from the still ones. Motions that can't work
// (unknown name, a circle of motions, no time to do it in, flying past
// something that moves) are reported, and that object stands still instead.
// Orbiting something that moves is fine (a moon around a planet, docs/17).
split_scene split_motion(const scene_spec& spec);

// Something standing still, as the planner sees it.
struct obstacle{
	std::string name;
	vec3 position;
	float radius;
};

// One moving object's path.
struct path{
	int object = -1;          // index in the full scene
	std::string name;
	motion_kind kind = motion_kind::orbits;
	float radius = 0.0f;      // the object's bounding radius
	int around = -1;          // which still obstacle it moves around / past (or -1)
	int around_path = -1;     // orbits and hits: which moving object it goes round / hits (or -1)
	float start = 0.0f, end = 0.0f;

	// orbits: a circle in the horizontal plane through `center`
	vec3 center{0.0f, 0.0f, 0.0f};
	float orbit_radius = 0.0f;
	float start_angle  = 0.0f;   // radians, 0 = the +x side
	float turns = 1.0f;

	// flies_past and hits: a straight line, `from` at `start` to `to` at `end`
	// (for hits, `to` is the point where it touches what it hits, and from
	// `end` on it sticks there and rides along, docs/18)
	vec3 from{0.0f, 0.0f, 0.0f};
	vec3 to{0.0f, 0.0f, 0.0f};

	// Where it is at time t, if the thing it orbits is at `center` then
	// (before `start` it waits at the beginning, after `end` it stays at the
	// end). Fly-bys don't use `center`. motion_plan::position fills it in.
	vec3 at(float t,const vec3& center)const;
	// How fast it goes along its own path (world units per second)
	float speed()const;
};

class motion_plan{
public:
	enum class method{ naive, orbits, flights, framed };

	motion_plan(const std::vector<obstacle>& still,std::vector<path> moving);

	void solve(method how);

	const std::vector<path>& paths()const;
	// Where moving object k is at time t (following its center if that moves)
	vec3 position(size_t k,float t)const;
	// How far object k reaches from its own center: its radius, or more if
	// it has moons going round it (docs/17)
	float reach(size_t k)const;
	// Spheres that hold each whole path: for an orbit, 32 spheres around its
	// circle; for a fly-by, its two end points (the segment lies between them).
	std::vector<obstacle> bounds()const;
	// the time between two checks (docs/16)
	float sample_step()const;
	// the time from 0 to when the last motion ends
	float duration()const;

	struct metrics{
		int moving     = 0;
		int collisions = 0;   // pairs that hit each other at some moment (and weren't meant to)
		int hits_planned = 0; // intended hits (docs/18)
		int hits_on_time = 0; // ... that really touch, exactly at their time
	};
	metrics measure()const;
	std::string report()const;

private:
	void place_naive();
	void plan_orbits();
	void repair_orbits();
	bool clear_of_still_sampled(size_t k)const;
	void plan_flights();
	void plan_hits();
	vec3 target_position(size_t k,float t)const;   // where the thing it moves around / hits is
	// Is this moment part of an intended hit between moving object k and the
	// other one (a moving object, or a still obstacle)? Then touching is fine.
	bool meant_to_touch(size_t k,int other_moving,int other_still,float t)const;
	float hit_gap(size_t k)const;   // the gap when it arrives (should be 0)
	bool clear_of_still(const path& p)const;
	// orbits_only: ignore fly-bys (they're planned after the orbits)
	bool clear_of_moving(size_t k,bool orbits_only = false)const;
	std::vector<std::string> warnings;
	float speed_of(size_t k)const;   // its own speed plus its center's
	int depth(size_t k)const;        // 0 = goes round something still, 1 = round a mover, ...

	// One pair that hit each other.
	struct collision{
		std::string a, b;
		float first_time;     // the first sample where they were too close
		float closest;        // the smallest surface gap seen (negative = inside each other)
	};
	std::vector<collision> find_collisions()const;

	std::vector<obstacle> still;
	std::vector<path> moving;
	std::string method_name = "none";
};
