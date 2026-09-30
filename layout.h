#pragma once
#include "scene_spec.h"
#include "vec3.h"

#include <string>
#include <vector>

// ============================================================================
//  The layout solver: turns a scene_spec (names + relations) into positions.
//
//  Every object is treated as a sphere (its bounding sphere, see mesh.h), so
//  "do these two overlap?" is one test:  distance < r1 + r2.
//
//  The steps (docs/11 .. 14):
//    naive  : everything goes where its relation points, no checking (the mess)
// ============================================================================

// Where one object ended up.
struct placement{
	std::string name;
	vec3 position{0.0f, 0.0f, 0.0f};
	float size   = 1.0f;   // scale to draw the mesh at
	float radius = 0.0f;   // bounding sphere radius at that size
};

class layout{
public:
	enum class method{ naive };

	// mesh_radii[i] = bounding radius of object i's mesh at size 1
	// (same order as spec.objects). The layout never loads files itself.
	layout(const scene_spec& spec,const std::vector<float>& mesh_radii);

	void solve(method how);

	const std::vector<placement>& result()const;
	// true if object i overlaps any other object
	bool overlapping(int i)const;
	// Everything an AI (or a person) needs to know about how it went.
	std::string report()const;

	// wanted empty space between two objects' spheres (world units)
	static constexpr float gap = 0.4f;
	// "near" is satisfied if the two surfaces are at most this far apart
	static constexpr float near_limit = 2.0f;

private:
	// A relation with the other object's name looked up to its index.
	struct link{
		relation_kind kind;
		int other;
	};

	void resolve_names();
	void sort_by_dependencies();
	void place_naive();

	enum class verdict{ ok, weak, failed };
	verdict check(int i,const link& l)const;
	int count_overlaps()const;

	scene_spec spec;                        // its own copy, so it can't dangle
	std::vector<placement> placed;          // one per object, same order as spec.objects
	std::vector<std::vector<link>> links;   // links[i] = object i's relations
	std::vector<int> order;                 // the order to place objects in
	std::vector<std::string> errors;        // problems found in the description
	std::string method_name = "none";
};
