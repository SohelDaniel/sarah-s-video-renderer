#include "world.h"

#include <algorithm>
#include <cmath>


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

		// fades (docs/24): if the first one fades IN, it starts invisible
		std::vector<fade_step> fades = o.fades;
		std::stable_sort(fades.begin(), fades.end(), [](const fade_step& a,const fade_step& b){ return a.start < b.start; });
		if(!fades.empty() && fades[0].to > 0.5f) thing.set_opacity(0.0f);
		for(const fade_step& f : fades) thing.fade(f.to, f.start, f.end, f.how);

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
				thing.move(p.to, p.start, p.end, p.how);
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

	titles.assign(spec.titles.begin(), spec.titles.end());

	// arrows (docs/26): look the names up once
	auto find = [&](const std::string& name){
		for(size_t i = 0;i<spec.objects.size();i++) if(spec.objects[i].name == name) return int(i);
		return -1;
	};
	for(const arrow_spec& a : spec.arrows){
		int from = find(a.from), to = find(a.to);
		if(from < 0 || to < 0 || from == to){
			arrow_errors.push_back("arrow " + a.from + " -> " + a.to + ": "
			                       + (from == to && from >= 0 ? std::string("it points at itself") : "there is no object called \""
			                          + (from < 0 ? a.from : a.to) + "\"") + " (arrow left out)");
			continue;
		}
		arrows.push_back({from, to, a.color, a.start, a.end});
	}
}

// Each arrow goes from the surface of one object to the surface of the
// other: from center to center, shortened at each end by that object's
// bounding radius (plus a little gap), so it touches neither.
void world::draw_overlays(render& renderer,float t)const{
	// titles, across the top, centered (docs/27); several stack downwards
	int row = 0;
	for(const title_spec& s : titles){
		if(t < s.start || (s.end >= 0.0f && t > s.end)) continue;
		// as big as fits: scale 3, or smaller if it would run off the sides
		int scale = 3;
		while(scale > 1 && render::text_width(s.text, scale) > cam.width - 32) scale--;
		int x = (cam.width - render::text_width(s.text, scale)) / 2;
		renderer.draw_text(x, 16 + row * (render::text_height(scale) + 8), s.text, scale, px::Pixel(240, 240, 245));
		row++;
	}

	for(const world_arrow& a : arrows){
		if(t < a.start || (a.end >= 0.0f && t > a.end)) continue;
		const object& from = objects[a.from];
		const object& to = objects[a.to];
		vec3 p = from.get_position(), q = to.get_position();
		vec3 d = q - p;
		float length = std::sqrt(dot(d, d));
		float gap = 0.15f;
		float cut_from = from.bounding_radius() + gap, cut_to = to.bounding_radius() + gap;
		if(length <= cut_from + cut_to) continue;          // touching: no room for an arrow
		vec3 dir = d * (1.0f / length);
		renderer.draw_arrow(p + dir * cut_from, q - dir * cut_to, a.color);
	}
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
	std::string text = solved.report();
	if(!arrow_errors.empty()){
		text += "  problems with arrows:\n";
		for(const std::string& e : arrow_errors) text += "    " + e + "\n";
	}
	return text;
}
