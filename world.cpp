#include "world.h"

#include <algorithm>
#include <cmath>
#include "font.h"
#include "math_layout.h"


std::vector<float> world::load_meshes(const scene_spec& spec){
	std::vector<float> radii;
	for(const object_spec& o : spec.objects){
		// try_emplace only loads the file if it isn't in the map yet
		auto it = meshes.try_emplace(o.mesh_file, o.mesh_file).first;
		radii.push_back(it->second.bounding_radius());
	}
	return radii;
}

// A camera making pictures of this size.
static camera sized(int width,int height){
	camera c;
	c.width = width;
	c.height = height;
	return c;
}

world::world(const scene_spec& spec,layout::method still_how,motion_plan::method moving_how,int width,int height)
	: cam(sized(width, height)),
	  solved(spec, load_meshes(spec), still_how, moving_how, float(width) / float(height)),
	  placer(width, height), ui(float(height) / 480.0f){
	placer.scale = ui;
	for(const object_spec& o : spec.objects){
		world_label l{o.label, o.label_math, o.label_always, o.label_start, o.label_end, {}};
		if(l.math && !l.text.empty()){
			math_parse_result parsed = parse_math(l.text);
			if(parsed.tree && fonts::serif() && fonts::italic()) l.formula = layout_math(*parsed.tree, 20.0f * ui);
			else l.math = false;
		}
		labels.push_back(l);
	}
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
		if((p.kind == motion_kind::orbits || p.kind == motion_kind::flies_past) && p.around_path >= 0){
			// round or past something that moves: measured from it, so attached (docs/17, 32)
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

	for(const title_spec& s : spec.titles){
		// as big as fits: 30 pixels, or smaller if it would run off the sides
		float size = 30.0f * ui;
		float room = float(cam.width) - 32.0f * ui;
		float w = render::text_width(s.text, size);
		if(w > room) size *= room / w;               // text width grows in step with size
		titles.push_back({s, size, fonts::sans() ? text_paths(s.text, *fonts::sans(), size) : vgroup{}});
	}
	// formulas are laid out once (docs/30); the parser already checked them
	for(const title_spec& m : spec.maths){
		math_parse_result parsed = parse_math(m.text);
		if(parsed.tree && fonts::serif() && fonts::italic()){
			math_box box = layout_math(*parsed.tree, 30.0f * ui);
			maths.push_back({m, box, math_paths(box)});
		}
	}

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
void world::draw_overlays(render& renderer,float t){
	// titles, across the top, centered (docs/27, 29); several stack downwards
	float top = 12.0f * ui;
	for(const world_title& s : titles){
		float seen = appear(s.when.start, s.when.end, t);
		if(seen <= 0.0f) continue;
		float x = (cam.width - render::text_width(s.when.text, s.size)) / 2.0f;
		px::Pixel color(240, 240, 245, uint8_t(255 * seen));
		if(s.paths.pieces.empty()) renderer.draw_text(x, top, s.when.text, s.size, color);
		for(const vpiece& p : s.paths.pieces){
			renderer.draw_vpiece(p, x + p.at.x, top + p.at.y, color, piece_look{}, std::max(1.0f, s.size / 16.0f));
		}
		top += render::text_height(s.size) + 6.0f * ui;
	}
	// formulas under the titles, centered, each during its own time range
	for(const world_math& m : maths){
		float seen = appear(m.when.start, m.when.end, t);
		if(seen <= 0.0f) continue;
		float x = (cam.width - m.formula.width) / 2.0f;
		float y = top + 4.0f * ui;
		px::Pixel color(240, 240, 245, uint8_t(std::lround(255.0f * seen)));
		for(const vpiece& p : m.paths.pieces){
			renderer.draw_vpiece(p, x + p.at.x, y + p.at.y, color, piece_look{}, 1.5f * ui);
		}
		top += m.formula.height + m.formula.depth + 12.0f * ui;
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

	// labels (docs/28): where each labelled object is on screen, then the
	// label layout decides where its words go
	const float label_size = 17.0f * ui;
	std::vector<label_request> requests;
	std::vector<size_t> owner;
	std::vector<float> seen_amount;
	for(size_t i = 0;i<objects.size();i++){
		const world_label& l = labels[i];
		if(l.text.empty()) continue;
		float w = l.math ? l.formula.width : render::text_width(l.text, label_size);
		float h = l.math ? l.formula.height + l.formula.depth : render::text_height(label_size);
		label_request r{w, h, {0, 0}, 0, false, l.always};
		float seen = appear(l.start, l.end, t);
		float x, y, radius;
		if(seen > 0.0f && objects[i].opacity() > 0.05f
		   && renderer.where_on_screen(objects[i].get_position(), objects[i].bounding_radius(), x, y, radius)){
			r.anchor = {x, y};
			r.radius = radius;
			r.visible = x >= 0 && y >= 0 && x <= cam.width && y <= cam.height;
		}
		requests.push_back(r);
		owner.push_back(i);
		seen_amount.push_back(seen);
	}
	if(requests.empty()) return;
	// no label may sit on the titles and formulas (docs/31)
	placer.keep_out.clear();
	if(top > 12.0f * ui) placer.keep_out.push_back({0.0f, float(cam.width), 0.0f, top});
	const std::vector<placed_label>& placed = placer.place(requests);
	for(size_t k = 0;k<placed.size();k++){
		if(!placed[k].shown) continue;
		const box2& b = placed[k].where;
		if(placed[k].leader){
			// from the circle's edge towards the label's middle
			point2 c = b.center();
			float dx = c.x - requests[k].anchor.x, dy = c.y - requests[k].anchor.y;
			float len = std::sqrt(dx * dx + dy * dy);
			if(len > 1.0f){
				float sx = requests[k].anchor.x + dx / len * requests[k].radius;
				float sy = requests[k].anchor.y + dy / len * requests[k].radius;
				renderer.draw_screen_line(sx, sy, c.x, c.y, ui, px::Pixel(200, 200, 210, uint8_t(200 * seen_amount[k])));
			}
		}
		const world_label& l = labels[owner[k]];
		if(l.math) renderer.draw_math(b.x0, b.y0, l.formula, px::Pixel(240, 240, 245), seen_amount[k]);
		else       renderer.draw_text(b.x0, b.y0, l.text, label_size, px::Pixel(240, 240, 245, uint8_t(255 * seen_amount[k])));
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
