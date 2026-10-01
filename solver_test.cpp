// ============================================================================
//  Stress tests for the layout solver (docs/15).
//
//  Every test scene is solved with every step (naive, greedy, refined,
//  framed) of the still layout, and its moving objects get paths (docs/16).
//  A table shows the numbers; then a list of hard checks says PASS or
//  FAIL. Exits with 1 if anything failed, so `make test` stops.
//
//  No window and no drawing: only the solver. Meshes are loaded just to get
//  their bounding radii.
// ============================================================================
#include "mesh.h"
#include "solver.h"
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

// Where every object is at a few moments: still ones never move, moving
// ones follow their paths. Used to compare two solves and to look for NaN.
static std::vector<vec3> snapshot(const scene_solver& s){
	std::vector<vec3> where;
	for(const placement& p : s.still().result()) where.push_back(p.position);
	for(const path& p : s.plan().paths()){
		for(float t : {0.0f, 0.5f * s.plan().duration(), s.plan().duration()}) where.push_back(p.at(t));
	}
	return where;
}

static bool all_finite(const scene_solver& s){
	for(const vec3& v : snapshot(s)){
		for(int k = 0;k<3;k++){
			if(!std::isfinite(v[k])) return false;
		}
	}
	return true;
}

static bool same_positions(const scene_solver& a,const scene_solver& b){
	std::vector<vec3> x = snapshot(a), y = snapshot(b);
	if(x.size() != y.size()) return false;
	for(size_t i = 0;i<x.size();i++){
		for(int k = 0;k<3;k++){
			if(x[i][k] != y[i][k]) return false;
		}
	}
	return true;
}

// mistakes found in the description: by the layout, and in the motions
static int errors_of(const scene_solver& s){
	return s.still().measure().errors + int(s.parts().errors.size());
}

int main(){
	const layout::method methods[] = {
		layout::method::naive, layout::method::greedy, layout::method::refined, layout::method::framed,
	};
	const char* method_names[] = {"naive", "greedy", "refined", "framed"};

	std::vector<test_scene> scenes = all_test_scenes();

	const motion_plan::method moving = motion_plan::method::framed;

	std::printf("%-14s %-8s %4s %8s %7s %5s %10s %6s %8s %6s %6s %8s\n", "scene", "step", "objs", "overlaps",
	            "hidden", "off", "relations", "errors", "warnings", "halved", "moving", "collide");
	for(const test_scene& t : scenes){
		std::vector<float> radii = radii_of(t.spec);
		for(int m = 0;m<4;m++){
			scene_solver solved(t.spec, radii, methods[m], moving);
			layout::metrics s = solved.still().measure();
			motion_plan::metrics mm = solved.plan().measure();
			std::printf("%-14s %-8s %4d %8d %7d %5d %5d of %-2d %6d %8d %6d %6d %8d\n",
			            m == 0 ? t.name.c_str() : "", method_names[m], s.objects + mm.moving, s.overlaps, s.hidden,
			            s.off_screen, s.relations_ok, s.relations_total, errors_of(solved), s.warnings,
			            s.steps_halved, mm.moving, mm.collisions);
		}
	}

	std::printf("\nchecks:\n");
	for(const test_scene& t : scenes){
		std::vector<float> radii = radii_of(t.spec);

		scene_solver once(t.spec, radii, layout::method::framed, moving);
		scene_solver twice(t.spec, radii, layout::method::framed, moving);
		layout::metrics s = once.still().measure();
		int errors = errors_of(once);

		check(all_finite(once), t.name, "every position is a real number (no NaN / infinity)");
		check(same_positions(once, twice), t.name, "solving twice gives exactly the same layout");
		check(s.overlaps == 0, t.name, "no overlaps after framing (" + std::to_string(s.overlaps) + ")");
		check(s.off_screen == 0, t.name, "everything inside the picture (" + std::to_string(s.off_screen) + " outside)");
		int collisions = once.plan().measure().collisions;
		check(collisions == 0, t.name, "moving objects never collide (" + std::to_string(collisions) + " pairs)");
		int leaving = once.moving_off_screen();
		check(leaving == 0, t.name, "moving objects stay in the picture (" + std::to_string(leaving) + " leave it)");
		check(errors >= t.expected_errors, t.name,
		      "reports its mistakes (" + std::to_string(errors) + " of at least " + std::to_string(t.expected_errors) + ")");
	}

	std::printf("\n%s: %d check%s failed\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
	return failures == 0 ? 0 : 1;
}
