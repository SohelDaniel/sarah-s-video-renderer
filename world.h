#pragma once
#include "camera.h"
#include "mesh.h"
#include "object.h"
#include "scene_spec.h"
#include "solver.h"

#include <map>
#include <string>
#include <vector>

// A described scene turned into real things you can draw:
//   1. loads every mesh the description mentions (each file only once)
//   2. runs the solver to get positions and paths
//   3. makes one object per described object (moving ones get timed
//      animations along their paths), plus a camera
//
// It owns everything, so the meshes live exactly as long as the objects
// pointing at them.
class world{
public:
	world(const scene_spec& spec,layout::method still_how,
	      motion_plan::method moving_how = motion_plan::method::naive);

	camera cam;

	// the objects, in the form player::play wants
	std::vector<object*> scene();
	// only the objects that stand still
	std::vector<object*> still_objects();
	// how long the video should play: until the last motion ends, at least 20 s
	float duration()const;
	std::string report()const;

private:
	// loads the meshes and returns each object's mesh radius (for the solver)
	std::vector<float> load_meshes(const scene_spec& spec);

	// declared in this order on purpose: members are built top to bottom,
	// and the solver needs the meshes loaded first
	std::map<std::string, mesh> meshes;   // file name -> mesh
	scene_solver solved;
	std::vector<object> objects;
	std::vector<bool> moves;              // moves[i]: does object i follow a path?
};
