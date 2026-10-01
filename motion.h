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
// ============================================================================


// The description, split into what stands still and what moves.
struct split_scene{
	scene_spec still;                 // only the objects that stand still
	std::vector<int> still_index;     // still.objects[k] is object still_index[k] of the full scene
	std::vector<int> moving;          // indices (in the full scene) of the objects that move
	std::vector<std::string> errors;  // problems with motions in the description
};

// Separates the moving objects from the still ones. Motions that can't work
// (unknown name, around another moving object, no time to do it in) are
// reported, and that object stands still instead.
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
	int around = -1;          // which obstacle it moves around / past
	float start = 0.0f, end = 0.0f;

	// orbits: a circle in the horizontal plane through `center`
	vec3 center{0.0f, 0.0f, 0.0f};
	float orbit_radius = 0.0f;
	float start_angle  = 0.0f;   // radians, 0 = the +x side
	float turns = 1.0f;

	// flies_past: a straight line, `from` at `start` to `to` at `end`
	vec3 from{0.0f, 0.0f, 0.0f};
	vec3 to{0.0f, 0.0f, 0.0f};

	// Where it is at time t (before `start` it waits at the beginning,
	// after `end` it stays at the end)
	vec3 at(float t)const;
	// How fast it goes while moving (world units per second)
	float speed()const;
};

class motion_plan{
public:
	enum class method{ naive, orbits, flights };

	motion_plan(const std::vector<obstacle>& still,std::vector<path> moving);

	void solve(method how);

	const std::vector<path>& paths()const;
	// the time from 0 to when the last motion ends
	float duration()const;

	struct metrics{
		int moving     = 0;
		int collisions = 0;   // pairs that hit each other at some moment
	};
	metrics measure()const;
	std::string report()const;

private:
	void place_naive();
	void plan_orbits();
	void plan_flights();
	bool clear_of_still(const path& p)const;
	bool clear_of_moving(size_t k)const;
	std::vector<std::string> warnings;
	float sample_step()const;
	float sample_step_with(const path& extra)const;

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
