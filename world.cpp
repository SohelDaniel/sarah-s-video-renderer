#include "world.h"


std::vector<float> world::load_meshes(const scene_spec& spec){
	std::vector<float> radii;
	for(const object_spec& o : spec.objects){
		// try_emplace only loads the file if it isn't in the map yet
		auto it = meshes.try_emplace(o.mesh_file, o.mesh_file).first;
		radii.push_back(it->second.bounding_radius());
	}
	return radii;
}

world::world(const scene_spec& spec,layout::method how)
	: solved(spec, load_meshes(spec)){
	solved.solve(how);

	const std::vector<placement>& placed = solved.result();
	objects.reserve(placed.size());
	for(size_t i = 0;i<placed.size();i++){
		const object_spec& o = spec.objects[i];
		object thing(meshes.at(o.mesh_file), o.color);
		thing.move(placed[i].position);
		thing.scale(placed[i].size);
		// a three-quarter view, so the faces of flat-sided shapes are easy to
		// tell apart. The bounding sphere doesn't care how it's turned.
		thing.rotate(0.6f, 0.3f);
		// overlapping objects get a red circle, the rest a grey one
		thing.show_bounds(solved.overlapping(int(i)) ? px::Pixel(255, 70, 70) : px::Pixel(150, 150, 160));
		objects.push_back(thing);
	}

	cam.move(vec3(0.0f, 6.0f, 16.0f));
	cam.point_at(vec3(0.0f, 0.0f, 0.0f));
}

std::vector<object*> world::scene(){
	std::vector<object*> pointers;
	for(object& o : objects) pointers.push_back(&o);
	return pointers;
}

const layout& world::plan()const{
	return solved;
}
