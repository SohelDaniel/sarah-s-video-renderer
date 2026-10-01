#include "solver.h"

#include <algorithm>
#include <cctype>
#include <sstream>


static std::vector<float> still_radii(const split_scene& parts,const std::vector<float>& mesh_radii){
	std::vector<float> radii;
	for(int i : parts.still_index) radii.push_back(mesh_radii[i]);
	return radii;
}

scene_solver::scene_solver(const scene_spec& spec,const std::vector<float>& mesh_radii,
                           layout::method still_how,motion_plan::method moving_how)
	: split(split_motion(spec)),
	  still_layout(split.still, still_radii(split, mesh_radii)),
	  planner(solve_still(still_how), make_paths(spec, mesh_radii)){
	planner.solve(moving_how);
	// step E4: the camera also has to fit the paths
	if(moving_how == motion_plan::method::framed && !planner.paths().empty()){
		// A second pass (docs/32). The paths change where the camera ends up,
		// and the refinement un-hides things for the camera it knows about.
		// So: solve the still objects again with this first guess at the
		// paths already in the framing, plan the motion again around the new
		// positions, and frame one last time.
		std::vector<obstacle> first_guess = planner.bounds();
		still_layout.clear_frame_extras();
		for(const obstacle& b : first_guess) still_layout.include_in_frame(b.position, b.radius);
		still_layout.solve(still_how);
		planner = motion_plan(obstacles(), make_paths(spec, mesh_radii));
		planner.solve(moving_how);
		still_layout.clear_frame_extras();
		for(const obstacle& b : planner.bounds()) still_layout.include_in_frame(b.position, b.radius);
		still_layout.reframe();
	}
}

std::vector<obstacle> scene_solver::obstacles()const{
	std::vector<obstacle> list;
	for(const placement& p : still_layout.result()) list.push_back({p.name, p.position, p.radius});
	return list;
}

int scene_solver::moving_off_screen()const{
	int count = 0;
	float dt = planner.sample_step();
	float last = planner.duration();
	for(size_t i = 0;i<planner.paths().size();i++){
		const path& p = planner.paths()[i];
		for(int k = 0;;k++){
			float t = std::min(k * dt, last);
			if(!still_layout.in_picture(planner.position(i, t), p.radius)){
				count++;
				break;
			}
			if(t >= last) break;
		}
	}
	return count;
}

// Roughly how wide a label is, without needing the font (docs/31): about
// 0.6 of its size per letter, plus a little. For a formula, only count what
// gets drawn (not the \commands, braces, ^ and _).
static float estimate_label_width(const object_spec& o){
	if(o.label.empty()) return 0.0f;
	const float size = 17.0f;
	if(!o.label_math) return 0.6f * size * float(o.label.size()) + 6.0f;
	int shown = 0;
	for(size_t i = 0;i<o.label.size();i++){
		char c = o.label[i];
		if(c == '\\'){ shown++; while(i + 1 < o.label.size() && std::isalpha((unsigned char)o.label[i + 1])) i++; continue; }
		if(c == '{' || c == '}' || c == '^' || c == '_' || c == ' ') continue;
		shown++;
	}
	return 0.6f * 20.0f * float(shown) + 6.0f;
}

// How tall the band at the top must be (docs/31): a title takes ~40 pixels
// and a formula ~60, plus a 12-pixel margin. But only the ones showing at
// the same moment are stacked (world.cpp), so it's the tallest stack at any
// moment. The stack only grows when something starts, so checking at every
// start time is enough.
static float words_band(const scene_spec& s){
	auto showing = [](const title_spec& w,float t){ return t >= w.start && (w.end < 0.0f || t < w.end); };
	float tallest = 0.0f;
	std::vector<float> starts;
	for(const title_spec& w : s.titles) starts.push_back(w.start);
	for(const title_spec& w : s.maths) starts.push_back(w.start);
	for(float t : starts){
		float stack = 0.0f;
		for(const title_spec& w : s.titles) if(showing(w, t)) stack += 40.0f;
		for(const title_spec& w : s.maths) if(showing(w, t)) stack += 60.0f;
		tallest = std::max(tallest, stack);
	}
	return tallest > 0.0f ? 12.0f + tallest : 0.0f;
}

// Solve the still objects, and hand them to the motion planner as obstacles.
std::vector<obstacle> scene_solver::solve_still(layout::method how){
	// room for words (docs/31): a band at the top for the titles and
	// formulas, and every label's width
	const scene_spec& s = split.still;
	float band = words_band(s);
	std::vector<float> widths;
	for(const object_spec& o : s.objects) widths.push_back(estimate_label_width(o));
	still_layout.set_words(band, widths, 480);
	still_layout.solve(how);
	return obstacles();
}

std::vector<path> scene_solver::make_paths(const scene_spec& spec,const std::vector<float>& mesh_radii)const{
	std::vector<path> paths;
	for(int i : split.moving){
		const object_spec& o = spec.objects[i];
		const motion& m = o.motions[0];
		path p;
		p.object = i;
		p.name   = o.name;
		p.kind   = m.kind;
		p.radius = mesh_radii[i] * size_value(o.size);
		p.turns  = m.turns;
		p.start  = m.start;
		p.end    = m.end;
		p.how    = m.how;
		// split_motion made sure the other object exists: it either stands
		// still, or (for an orbit) moves itself
		for(size_t k = 0;k<split.still.objects.size();k++){
			if(split.still.objects[k].name == m.other){
				p.around = int(k);
				break;
			}
		}
		paths.push_back(p);
	}
	// orbits around something that moves: point at its path
	for(path& p : paths){
		if(p.around >= 0) continue;
		const std::string& other = spec.objects[p.object].motions[0].other;
		for(size_t k = 0;k<paths.size();k++){
			if(paths[k].name == other){
				p.around_path = int(k);
				break;
			}
		}
	}
	return paths;
}

const split_scene& scene_solver::parts()const{ return split; }
const layout& scene_solver::still()const{ return still_layout; }
const motion_plan& scene_solver::plan()const{ return planner; }

int scene_solver::path_of(int i)const{
	const std::vector<path>& paths = planner.paths();
	for(size_t k = 0;k<paths.size();k++){
		if(paths[k].object == i) return int(k);
	}
	return -1;
}

const placement* scene_solver::placed(int i)const{
	for(size_t k = 0;k<split.still_index.size();k++){
		if(split.still_index[k] == i) return &still_layout.result()[k];
	}
	return nullptr;
}

std::string scene_solver::report()const{
	std::ostringstream out;
	out << still_layout.report();
	if(!planner.paths().empty() || !split.errors.empty()){
		out << planner.report();
		out << "  moving objects that leave the picture at some moment: " << moving_off_screen() << "\n";
	}
	if(!split.errors.empty()){
		out << "  problems with motions in the description:\n";
		for(const std::string& e : split.errors) out << "    " << e << "\n";
	}
	return out.str();
}
