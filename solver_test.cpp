// ============================================================================
//  Stress tests for the layout solver (docs/15).
//
//  Every test scene is solved with every step (naive, greedy, refined,
//  framed). A table shows the numbers; then a list of hard checks says
//  PASS or FAIL. Exits with 1 if anything failed, so `make test` stops.
//
//  No window and no drawing: only the solver. Meshes are loaded just to get
//  their bounding radii.
// ============================================================================
#include "layout.h"
#include "mesh.h"
#include "test_scenes.h"

#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>


static std::map<std::string, float> radius_cache;

static std::vector<float> radii_of(const scene_spec& spec){
	std::vector<float> radii;
	for(const object_spec& o : spec.objects){
		auto it = radius_cache.find(o.mesh_file);
		if(it == radius_cache.end()){
			it = radius_cache.emplace(o.mesh_file, mesh(o.mesh_file).bounding_radius()).first;
		}
		radii.push_back(it->second);
	}
	return radii;
}

static int failures = 0;

static void check(bool ok,const std::string& scene,const std::string& what){
	std::printf("  %s  %-14s %s\n", ok ? "PASS" : "FAIL", scene.c_str(), what.c_str());
	if(!ok) failures++;
}

static bool all_finite(const layout& l){
	for(const placement& p : l.result()){
		for(int k = 0;k<3;k++){
			if(!std::isfinite(p.position[k])) return false;
		}
	}
	return true;
}

static bool same_positions(const layout& a,const layout& b){
	if(a.result().size() != b.result().size()) return false;
	for(size_t i = 0;i<a.result().size();i++){
		for(int k = 0;k<3;k++){
			if(a.result()[i].position[k] != b.result()[i].position[k]) return false;
		}
	}
	return true;
}

int main(){
	const layout::method methods[] = {
		layout::method::naive, layout::method::greedy, layout::method::refined, layout::method::framed,
	};
	const char* method_names[] = {"naive", "greedy", "refined", "framed"};

	std::vector<test_scene> scenes = all_test_scenes();

	std::printf("%-14s %-8s %4s %8s %7s %5s %10s %6s %8s %6s\n",
	            "scene", "step", "objs", "overlaps", "hidden", "off", "relations", "errors", "warnings", "halved");
	for(const test_scene& t : scenes){
		std::vector<float> radii = radii_of(t.spec);
		for(int m = 0;m<4;m++){
			layout l(t.spec, radii);
			l.solve(methods[m]);
			layout::metrics s = l.measure();
			std::printf("%-14s %-8s %4d %8d %7d %5d %5d of %-2d %6d %8d %6d\n",
			            m == 0 ? t.name.c_str() : "", method_names[m], s.objects, s.overlaps, s.hidden,
			            s.off_screen, s.relations_ok, s.relations_total, s.errors, s.warnings, s.steps_halved);
		}
	}

	std::printf("\nchecks:\n");
	for(const test_scene& t : scenes){
		std::vector<float> radii = radii_of(t.spec);

		layout once(t.spec, radii);
		once.solve(layout::method::framed);
		layout twice(t.spec, radii);
		twice.solve(layout::method::framed);
		layout::metrics s = once.measure();

		check(all_finite(once), t.name, "every position is a real number (no NaN / infinity)");
		check(same_positions(once, twice), t.name, "solving twice gives exactly the same layout");
		check(s.overlaps == 0, t.name, "no overlaps after framing (" + std::to_string(s.overlaps) + ")");
		check(s.off_screen == 0, t.name, "everything inside the picture (" + std::to_string(s.off_screen) + " outside)");
		check(s.errors >= t.expected_errors, t.name,
		      "reports its mistakes (" + std::to_string(s.errors) + " of at least " + std::to_string(t.expected_errors) + ")");
	}

	std::printf("\n%s: %d check%s failed\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
	return failures == 0 ? 0 : 1;
}
