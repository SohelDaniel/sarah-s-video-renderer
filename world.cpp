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
			// from docs/09; motion_plan::position describes exactly the same
			// motion
			const path& p = solved.plan().paths()[k];
			if(p.kind == motion_kind::orbits && p.around_path >= 0){
				// round something that moves: work relative to it (attached
				// below), so the circle is around (0,0,0)
				vec3 zero(0.0f, 0.0f, 0.0f);
				thing.move(p.at(0.0f, zero));
				thing.rotate_around(zero, 6.2831853f * p.turns, 0.0f, p.start, p.end);
			}else if(p.kind == motion_kind::orbits){
				thing.move(p.at(0.0f, p.center));
				thing.rotate_around(p.center, 6.2831853f * p.turns, 0.0f, p.start, p.end);
			}else{
				// fly-by, or a hit's approach (stuck to its target below)
				thing.move(p.from);
				thing.move(p.to, p.start, p.end);
			}
		}
		objects.push_back(thing);
		moves.push_back(k >= 0);
	}

	// now that every object exists (and won't move in memory), link the
	// moons to what they go round (docs/17)
	for(size_t i = 0;i<objects.size();i++){
		int k = solved.path_of(int(i));
		if(k < 0) continue;
		const path& p = solved.plan().paths()[k];
		if(p.kind == motion_kind::orbits && p.around_path >= 0){
			int parent = solved.plan().paths()[p.around_path].object;
			objects[i].attach_to(&objects[parent]);
		}
		if(p.kind == motion_kind::hits){
			// stick to what it hit, from the moment of impact (docs/18)
			int target = p.around_path >= 0 ? solved.plan().paths()[p.around_path].object
			                                : solved.parts().still_index[p.around];
			objects[i].stick_to(&objects[target], p.end);
		}
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
