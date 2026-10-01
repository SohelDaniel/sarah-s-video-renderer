#include "world.h"

#include <algorithm>
#include <cmath>
#include "expression.h"
#include "font.h"
#include "math_layout.h"


// Which flat shape an object uses: "circle", "square filled", or a graph's
// own one (each graph is different).
static std::string flat_key(const object_spec& o){
	if(o.flat == "graph") return "graph " + o.name;
	return o.flat + (o.filled ? " filled" : "");
}

std::vector<float> world::load_meshes(const scene_spec& spec){
	std::vector<float> radii;
	for(const object_spec& o : spec.objects){
		if(!o.flat.empty()){
			// a flat shape (docs/40): built once per kind, and it fits in radius 1
			std::string key = flat_key(o);
			if(o.flat == "graph"){
				// a graph (docs/42): its function was checked by the parser
				expression_result f = parse_expression(o.graph);
				flat_shapes.try_emplace(key, f.tree ? graph_shape(*f.tree, o.graph_from, o.graph_to, fonts::sans()) : flat_shape{});
			}else{
				flat_shapes.try_emplace(key, make_flat_shape(o.flat, o.filled));
				for(const object_spec::flat_change& c : o.changes){
					flat_shapes.try_emplace(c.shape + (o.filled ? " filled" : ""), make_flat_shape(c.shape, o.filled));
				}
			}
			radii.push_back(o.flat == "graph" ? graph_scale : 1.0f);
			continue;
		}
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
		object thing = o.flat.empty() ? object(meshes.at(o.mesh_file), o.color)
		                              : object(flat_shapes.at(flat_key(o)), o.color);
		thing.scale(size_value(o.size) * (o.flat == "graph" ? graph_scale : 1.0f));
		if(o.flat.empty()){
			// a three-quarter view, so the faces of flat-sided shapes are easy to
			// tell apart. The bounding sphere doesn't care how it's turned.
			thing.rotate(0.6f, 0.3f);
		}else{
			// a flat shape faces back along the camera's view (docs/40): its plane
			// is parallel to the picture, so from there it looks exactly 2D.
			// The model turns its own +z to (cos rx·sin ry, −sin rx, cos rx·cos ry),
			// and that has to be d, the direction back towards the camera:
			//      rx = −asin(d.y),    ry = atan2(d.x, d.z)
			vec3 d = normalize(solved.still().camera_eye() - solved.still().camera_target());
			thing.rotate(std::atan2(d[0], d[2]), -std::asin(d[1]));
			if(o.create_end >= 0.0f) thing.create(o.create_start, o.create_end);   // drawn in (docs/41)
			for(const object_spec::flat_change& c : o.changes){
				thing.morph_to(flat_shapes.at(c.shape + (o.filled ? " filled" : "")), c.start, c.end);   // docs/43
			}
		}
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
		if(!fonts::serif() || !fonts::italic()) break;
		world_math w{m, {}, {}};
		std::vector<std::string> texts = {m.text};
		for(const becomes_step& b : m.becomes) texts.push_back(b.text);
		for(const std::string& text : texts){
			math_parse_result parsed = parse_math(text);
			if(!parsed.tree) break;
			w.stages.push_back(math_paths(layout_math(*parsed.tree, 30.0f * ui)));
			if(w.stages.size() > 1) w.matches.push_back(match_pieces(w.stages[w.stages.size() - 2], w.stages.back()));
		}
		if(!w.stages.empty()) maths.push_back(std::move(w));
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

// A title or formula's pieces, with its top-left corner at (x, y). While
// it's being written (docs/38), each piece follows border_then_fill at its
// own lagged progress; otherwise every piece is simply there.
static void draw_words(render& renderer,const vgroup& group,float progress,float x,float y,
                       px::Pixel color,float shadow){
	int n = int(group.pieces.size());
	for(int i = 0;i<n;i++){
		const vpiece& p = group.pieces[size_t(i)];
		piece_look look = progress < 1.0f ? border_then_fill(piece_progress(i, n, progress)) : piece_look{};
		renderer.draw_vpiece(p, x + p.at.x, y + p.at.y, color, look, shadow);
	}
}

// How far a title or formula's writing is at t (1 = done, or not written in).
static float written(const title_spec& when,float t){
	return when.write > 0.0f ? (t - when.start) / when.write : 1.0f;
}

// One formula turning into the next (docs/39), f = 0..1 of the way.
//   a piece with a partner glides from its place to the partner's, and grows
//   or shrinks to its size (a bar stretches to its length)
//   a piece without one fades out; a new piece fades in
static void draw_change(render& renderer,const vgroup& a,const vgroup& b,const std::vector<int>& to,
                        float xa,float xb,float y,float f,px::Pixel color,float shadow){
	auto lerp = [f](float u,float v){ return u + (v - u) * f; };
	std::vector<bool> arrived(b.pieces.size(), false);
	for(size_t i = 0;i<a.pieces.size();i++){
		const vpiece& p = a.pieces[i];
		if(to[i] >= 0){
			const vpiece& q = b.pieces[size_t(to[i])];
			arrived[size_t(to[i])] = true;
			piece_look look;
			look.moving = true;
			if(p.key >= 0){
				look.scale_x = look.scale_y = lerp(p.size, q.size) / p.size;
			}else{
				look.scale_x = lerp(p.width, q.width) / p.width;
				look.scale_y = lerp(p.height, q.height) / p.height;
			}
			renderer.draw_vpiece(p, lerp(xa + p.at.x, xb + q.at.x), lerp(y + p.at.y, y + q.at.y), color, look, shadow);
		}else{
			px::Pixel fading = color;
			fading.a = uint8_t(std::lround(color.a * (1.0f - f)));
			renderer.draw_vpiece(p, xa + p.at.x, y + p.at.y, fading, piece_look{}, shadow);
		}
	}
	for(size_t j = 0;j<b.pieces.size();j++){
		if(arrived[j]) continue;
		const vpiece& q = b.pieces[j];
		px::Pixel coming = color;
		coming.a = uint8_t(std::lround(color.a * f));
		renderer.draw_vpiece(q, xb + q.at.x, y + q.at.y, coming, piece_look{}, shadow);
	}
}

// How visible a title or formula is at t: appear() (24, 28), except that a
// written one has no fade-in, since writing is how it comes in.
static float seen_at(const title_spec& when,float t){
	float seen = appear(when.start, when.end, t);
	if(when.write > 0.0f && t >= when.start && (when.end < 0.0f || t <= when.end)){
		seen = when.end >= 0.0f ? std::clamp((when.end - t) / 0.3f, 0.0f, 1.0f) : 1.0f;
	}
	return seen;
}

// Each arrow goes from the surface of one object to the surface of the
// other: from center to center, shortened at each end by that object's
// bounding radius (plus a little gap), so it touches neither.
void world::draw_overlays(render& renderer,float t){
	// titles, across the top, centered (docs/27, 29); several stack downwards
	float top = 12.0f * ui;
	for(const world_title& s : titles){
		float seen = seen_at(s.when, t);
		if(seen <= 0.0f) continue;
		float x = (cam.width - render::text_width(s.when.text, s.size)) / 2.0f;
		px::Pixel color(240, 240, 245, uint8_t(255 * seen));
		if(s.paths.pieces.empty()) renderer.draw_text(x, top, s.when.text, s.size, color);
		draw_words(renderer, s.paths, written(s.when, t), x, top, color, std::max(1.0f, s.size / 16.0f));
		top += render::text_height(s.size) + 6.0f * ui;
	}
	// formulas under the titles, centered, each during its own time range
	for(const world_math& m : maths){
		float seen = seen_at(m.when, t);
		if(seen <= 0.0f) continue;
		float y = top + 4.0f * ui;
		px::Pixel color(240, 240, 245, uint8_t(std::lround(255.0f * seen)));
		// which formula is showing, or which two it's between (docs/39)
		size_t k = 0;
		float f = -1.0f;
		for(size_t i = 0;i + 1<m.stages.size();i++){
			const becomes_step& step = m.when.becomes[i];
			if(t >= step.end) k = i + 1;
			else{
				if(t >= step.start){ k = i; f = smooth_curve((t - step.start) / (step.end - step.start)); }
				break;
			}
		}
		const vgroup& a = m.stages[k];
		float xa = (cam.width - a.width) / 2.0f;
		float tall = a.height + a.depth;
		if(f < 0.0f){
			// only the first one is written in (38); the others arrive by changing
			draw_words(renderer, a, k == 0 ? written(m.when, t) : 1.0f, xa, y, color, 1.5f * ui);
		}else{
			const vgroup& b = m.stages[k + 1];
			draw_change(renderer, a, b, m.matches[k], xa, (cam.width - b.width) / 2.0f, y, f, color, 1.5f * ui);
			tall += (b.height + b.depth - tall) * f;
		}
		top += tall + 12.0f * ui;
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
