#include "live_scene.h"
#include "scene_parser.h"

#include <fstream>
#include <iostream>
#include <sstream>


live_scene::live_scene(const std::string& path,motion_plan::method how,const camera& look)
	: path(path), how(how), last_check(std::chrono::steady_clock::now()), empty_camera(look){
	check_now();
}

camera& live_scene::cam(){
	return current ? current->cam : empty_camera;
}

const std::vector<object*>& live_scene::objects(){
	return pointers;   // empty until there's a scene
}

float live_scene::seconds(){
	return current ? current->duration() : 20.0f;
}

void live_scene::draw_overlays(render& renderer,float t){
	if(current) current->draw_overlays(renderer, t);
}

bool live_scene::has_scene()const{
	return current != nullptr;
}

const std::string& live_scene::last_report()const{
	return report;
}

std::string live_scene::report_path()const{
	return path + ".report";
}

void live_scene::poll(){
	// looking at the file 120 times a second would be wasteful; twice is plenty
	auto now = std::chrono::steady_clock::now();
	if(now - last_check < std::chrono::milliseconds(500)) return;
	last_check = now;
	check_now();
}

bool live_scene::check_now(){
	std::error_code error;
	auto changed = std::filesystem::last_write_time(path, error);
	if(error){
		// it might be in the middle of being saved: try again next time
		if(!current && report.empty()){
			report = "could not open " + path + "\n";
			std::cout << report;
		}
		return false;
	}
	if(current && changed == stamp) return false;   // nothing new
	if(!current && changed == stamp && !report.empty()) return false;
	stamp = changed;
	return load();
}

bool live_scene::load(){
	std::ostringstream out;
	parse_result parsed = parse_scene_file(path);
	for(const parse_message& m : parsed.errors) out << "error: " << m.to_string() << "\n";
	for(const parse_message& m : parsed.notes)  out << "note: " << m.to_string() << "\n";

	bool swapped = false;
	if(parsed.ok()){
		try{
			auto next = std::make_unique<world>(parsed.spec, layout::method::framed, how,
			                                    empty_camera.width, empty_camera.height);
			next->cam.circles = empty_camera.circles;
			out << next->report();
			// still objects slowly spin in place, like the other solved scenes
			for(object* o : next->still_objects()){
				if(!o->is_flat()) o->rotate(0.6f + 6.283f, 0.3f, 0.0f, 20.0f);   // flat shapes keep facing the camera (docs/40)
			}
			current = std::move(next);
			pointers = current->scene();
			swapped = true;
		}catch(const std::exception& e){
			out << "error: " << e.what() << "\n";
		}
	}
	if(!swapped && current){
		out << "(the last good version is still on screen)\n";
	}

	report = out.str();
	std::ofstream(report_path()) << report;
	int errors = int(parsed.errors.size());
	std::cout << (swapped ? "loaded " : "could not load ") << path
	          << (errors ? " (" + std::to_string(errors) + " errors)" : std::string(""))
	          << " -> " << report_path() << std::endl;
	return swapped;
}
