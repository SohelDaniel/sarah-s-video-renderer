#include "solver.h"

#include <algorithm>
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
	if(moving_how == motion_plan::method::framed){
		for(const obstacle& b : planner.bounds()) still_layout.include_in_frame(b.position, b.radius);
		still_layout.reframe();
	}
}

int scene_solver::moving_off_screen()const{
	int count = 0;
	float dt = planner.sample_step();
	float last = planner.duration();
	for(const path& p : planner.paths()){
		for(int k = 0;;k++){
			float t = std::min(k * dt, last);
			if(!still_layout.in_picture(p.at(t), p.radius)){
				count++;
				break;
			}
			if(t >= last) break;
		}
	}
	return count;
}

// Solve the still objects, and hand them to the motion planner as obstacles.
std::vector<obstacle> scene_solver::solve_still(layout::method how){
	still_layout.solve(how);
	std::vector<obstacle> obstacles;
	for(const placement& p : still_layout.result()){
		obstacles.push_back({p.name, p.position, p.radius});
	}
	return obstacles;
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
		// split_motion made sure the other object exists and stands still
		for(size_t k = 0;k<split.still.objects.size();k++){
			if(split.still.objects[k].name == m.other){
				p.around = int(k);
				break;
			}
		}
		paths.push_back(p);
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
