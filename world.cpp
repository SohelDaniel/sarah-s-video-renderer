#include "world.h"

#include <algorithm>


std::vector<float> world::load_meshes(const scene_spec& spec){
	std::vector<float> radii;
	for(const object_spec& o : spec.objects){
		// try_emplace only loads the file if it isn't in the map yet
		auto it = meshes.try_emplace(o.mesh_file, o.mesh_file).first;
		radii.push_back(it->second.bounding_radius());
	}
	return radii;
}

world::world(const scene_spec& spec,layout::method still_how,motion_plan::method moving_how)
	: solved(spec, load_meshes(spec), still_how, moving_how){
	objects.reserve(spec.objects.size());
	for(size_t i = 0;i<spec.objects.size();i++){
		const object_spec& o = spec.objects[i];
		object thing(meshes.at(o.mesh_file), o.color);
		thing.scale(size_value(o.size));
		// a three-quarter view, so the faces of flat-sided shapes are easy to
		// tell apart. The bounding sphere doesn't care how it's turned.
		thing.rotate(0.6f, 0.3f);
		// the circle turns red in any frame where it overlaps something
		thing.show_bounds();

		int k = solved.path_of(int(i));
		if(k < 0){
			thing.move(solved.placed(int(i))->position);
		}else{
			// follow the path the solver picked, with the timed animations
			// from docs/09; path::at() describes exactly the same motion
			const path& p = solved.plan().paths()[k];
			thing.move(p.at(0.0f));
			if(p.kind == motion_kind::orbits){
				thing.rotate_around(p.center, 6.2831853f * p.turns, 0.0f, p.start, p.end);
			}else{
				thing.move(p.to, p.start, p.end);
			}
		}
		objects.push_back(thing);
		moves.push_back(k >= 0);
	}

	cam.move(solved.still().camera_eye());
	cam.point_at(solved.still().camera_target());
}

std::vector<object*> world::scene(){
	std::vector<object*> pointers;
	for(object& o : objects) pointers.push_back(&o);
	return pointers;
}

std::vector<object*> world::still_objects(){
	std::vector<object*> pointers;
	for(size_t i = 0;i<objects.size();i++){
		if(!moves[i]) pointers.push_back(&objects[i]);
	}
	return pointers;
}

float world::duration()const{
	return std::max(20.0f, solved.plan().duration() + 2.0f);
}

std::string world::report()const{
	return solved.report();
}
