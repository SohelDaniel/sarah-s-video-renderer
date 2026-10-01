#pragma once
#include "layout.h"
#include "motion.h"
#include "scene_spec.h"

#include <string>
#include <vector>

// The whole solver in one place (docs/11-16):
//   1. split the description into still and moving objects   (motion.h)
//   2. place the still ones                                   (layout.h)
//   3. give the moving ones paths around them                 (motion.h)
// Used by world (to build the scene) and by the stress tests (to check it).
class scene_solver{
public:
	// mesh_radii[i] = bounding radius of object i's mesh at size 1
	scene_solver(const scene_spec& spec,const std::vector<float>& mesh_radii,
	             layout::method still_how,motion_plan::method moving_how);

	const split_scene& parts()const;
	const layout& still()const;
	const motion_plan& plan()const;

	// which path object i follows (index into motion().paths()), or -1 if it stands still
	int path_of(int i)const;
	// where object i stands, if it stands still (nullptr if it moves)
	const placement* placed(int i)const;

	// how many moving objects leave the picture at some moment (docs/16)
	int moving_off_screen()const;

	std::string report()const;

private:
	// these run in the member initializers, in this order
	std::vector<obstacle> solve_still(layout::method how);
	std::vector<path> make_paths(const scene_spec& spec,const std::vector<float>& mesh_radii)const;

	split_scene split;      // declared first: everything below is built from it
	layout still_layout;
	motion_plan planner;
};
