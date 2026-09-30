#pragma once
#include "camera.h"
#include "layout.h"
#include "mesh.h"
#include "object.h"
#include "scene_spec.h"

#include <map>
#include <string>
#include <vector>

// A described scene turned into real things you can draw:
//   1. loads every mesh the description mentions (each file only once)
//   2. runs the layout solver to get positions
//   3. makes one object per placement, plus a camera
//
// It owns everything, so the meshes live exactly as long as the objects
// pointing at them.
class world{
public:
	world(const scene_spec& spec,layout::method how);

	camera cam;

	// the objects, in the form player::play wants
	std::vector<object*> scene();
	const layout& plan()const;

private:
	// loads the meshes and returns each object's mesh radius (for the layout)
	std::vector<float> load_meshes(const scene_spec& spec);

	// declared in this order on purpose: members are built top to bottom,
	// and the layout needs the meshes loaded first
	std::map<std::string, mesh> meshes;   // file name -> mesh
	layout solved;
	std::vector<object> objects;
};
