// ============================================================================
//  Tests for the engine itself (docs/19-22): the parts that aren't the
//  layout solver. No window and no SDL, so they run anywhere.
//
//  Exits with 1 if anything failed, so `make test` stops.
// ============================================================================
#include "camera.h"
#include "fly_camera.h"
#include "live_scene.h"
#include "render.h"
#include "scene_parser.h"
#include "test_scenes.h"
#include "timeline.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>


static int failures = 0;

static void check(bool ok,const std::string& what){
	std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
	if(!ok) failures++;
}

// How many pixels aren't the background color.
static int drawn_pixels(const render& r){
	const px::Image& img = r.picture();
	px::Pixel background = img.Get(0, 0);   // the corner is never drawn on in these tests
	int count = 0;
	for(int y = 0;y<img.Height();y++){
		for(int x = 0;x<img.Width();x++){
			px::Pixel p = img.Get(x, y);
			if(p.r != background.r || p.g != background.g || p.b != background.b) count++;
		}
	}
	return count;
}

// ---- near-plane clipping (docs/19) ----
static void test_clipping(){
	std::printf("near-plane clipping:\n");
	camera cam;
	cam.width = 160;
	cam.height = 120;
	cam.move(vec3(0.0f, 0.0f, 4.0f));
	cam.point_at(vec3(0.0f, 0.0f, 0.0f));

	// a floor-like triangle: two corners in front of the camera, one far
	// behind it (z = 10 is 6 units behind the camera at z = 4). Corners in
	// the order a, b, c are counter-clockwise seen from ABOVE, so it faces up,
	// towards the camera (otherwise back-face culling would skip it, docs/07).
	vec3 a(-2.0f, -1.0f, -2.0f), b(0.0f, -1.0f, 10.0f), c(2.0f, -1.0f, -2.0f);

	render with(cam.width, cam.height);
	with.begin(cam);
	with.draw(a, b, c, px::Pixel(200, 200, 200));
	int on = drawn_pixels(with);

	render without(cam.width, cam.height);
	without.clipping = false;
	without.begin(cam);
	without.draw(a, b, c, px::Pixel(200, 200, 200));
	int off = drawn_pixels(without);

	check(on > 500, "a triangle reaching behind the camera is still drawn (" + std::to_string(on) + " pixels)");
	check(off == 0, "without clipping, the same triangle disappears (" + std::to_string(off) + " pixels)");

	// a triangle completely behind the camera draws nothing
	render behind(cam.width, cam.height);
	behind.begin(cam);
	behind.draw(vec3(-1, 0, 6), vec3(1, 0, 6), vec3(0, 1, 6), px::Pixel(200, 200, 200));
	check(drawn_pixels(behind) == 0, "a triangle completely behind the camera draws nothing");
}

static bool close(const vec3& a,const vec3& b,float tolerance = 1e-4f){
	vec3 d = a - b;
	return std::sqrt(dot(d, d)) <= tolerance;
}

// ---- the free camera (docs/20) ----
static void test_fly_camera(){
	std::printf("free camera:\n");

	// looking somewhere, then asking which way we look, gives it back
	bool round_trip = true;
	for(const vec3& d : {vec3(0, 0, -1), vec3(1, 0, 0), vec3(0.3f, 0.5f, -0.8f), vec3(-0.6f, -0.2f, 0.7f)}){
		fly_camera f;
		f.look_from(vec3(0, 0, 0), d);
		if(!close(f.forward(), normalize(d), 1e-3f)) round_trip = false;
	}
	check(round_trip, "look_from, then forward(), gives back the same direction");

	// pitch stops just short of straight up
	fly_camera up;
	controls look_up;
	look_up.turn_y = -100000.0f;            // the mouse moved a long way up
	up.step(look_up, 0.016f);
	check(std::fabs(up.pitch - fly_camera::max_pitch) < 1e-6f, "looking up stops at 89 degrees (never straight up)");

	// W for 1 second, looking along -z, at speed 4
	fly_camera walk;
	controls w;
	w.forward = true;
	walk.step(w, 1.0f);
	check(close(walk.eye, vec3(0, 0, -4)), "W for 1 s moves 4 units forward");

	// W + D is not faster than W
	fly_camera diagonal;
	controls wd;
	wd.forward = true;
	wd.right = true;
	diagonal.step(wd, 1.0f);
	check(std::fabs(std::sqrt(dot(diagonal.eye, diagonal.eye)) - 4.0f) < 1e-4f, "W + D moves at the same speed as W alone");

	// looking down and pressing W stays level
	fly_camera level;
	level.pitch = -0.8f;
	level.step(w, 1.0f);
	check(std::fabs(level.eye[1]) < 1e-6f, "looking down and pressing W doesn't dig into the floor");

	// 60 frames of 1/60 s go as far as one frame of 1 s (frame-rate independent)
	fly_camera many, one;
	for(int k = 0;k<60;k++) many.step(w, 1.0f / 60.0f);
	one.step(w, 1.0f);
	check(close(many.eye, one.eye, 1e-3f), "60 small steps = 1 big step (the speed doesn't depend on frame rate)");

	// the same inputs give the same result
	fly_camera a, b;
	controls mixed;
	mixed.forward = true;
	mixed.turn_x = 37.0f;
	mixed.turn_y = -12.0f;
	for(int k = 0;k<10;k++){ a.step(mixed, 0.02f); b.step(mixed, 0.02f); }
	check(close(a.eye, b.eye, 0.0f) && a.yaw == b.yaw && a.pitch == b.pitch, "the same inputs always give the same camera");
}

// ---- easing (docs/23) ----
static void test_easing(){
	std::printf("easing:\n");
	bool ends = true;
	for(rate r : {rate::linear, rate::smooth, rate::sine, rate::rush_into, rate::rush_from}){
		if(std::fabs(shape(r, 0.0f)) > 1e-6f || std::fabs(shape(r, 1.0f) - 1.0f) > 1e-6f) ends = false;
	}
	check(ends, "every curve starts at 0 and ends at 1");
	check(std::fabs(shape(rate::there_and_back, 1.0f)) < 1e-6f && std::fabs(shape(rate::there_and_back, 0.5f) - 1.0f) < 1e-6f,
	      "there_and_back is at 1 halfway and back at 0 at the end");
	check(std::fabs(shape(rate::smooth, 0.5f) - 0.5f) < 1e-6f, "smooth is exactly halfway at t = 0.5");
	float predicted = 10.0f * 0.25f / (1.0f - 2.0f * sigmoid(-5.0f));
	check(std::fabs(steepest(rate::smooth) - predicted) < 0.01f,
	      "smooth's steepest slope matches the formula 10/4 / (1 - 2 sigma(-5)) = " + std::to_string(predicted));
	check(std::fabs(steepest(rate::sine) - 1.5708f) < 0.01f, "sine's steepest slope is pi/2");

	parse_result p = parse_scene("sun = sphere\ncomet = pyramid flies_past sun 4s-10s smooth\n");
	check(p.ok() && p.spec.objects[1].motions[0].how == rate::smooth, "'flies_past sun 4s-10s smooth' is read as smooth");
	parse_result o = parse_scene("sun = sphere\nplanet = cube orbits sun 1 turn 0s-20s smooth\n");
	check(!o.ok(), "an orbit with a rate is an error (orbits stay steady, so loops have no jump)");
}

// ---- looping (docs/20) ----
static void test_looping(){
	std::printf("looping:\n");
	check(loop_time(7.0f, 20.0f) == 7.0f, "before the end, time is unchanged (7 s -> 7 s)");
	check(loop_time(25.0f, 20.0f) == 5.0f, "after the end, it starts again (25 s -> 5 s)");
	check(loop_time(40.0f, 20.0f) == 0.0f, "two whole loops later it's back at 0 (40 s -> 0 s)");
	check(loop_time(5.0f, 0.0f) == 0.0f, "a video with no length doesn't divide by zero");
}

// ---- the scene language (docs/21) ----

// Are two descriptions exactly the same? If not, say where they differ.
static bool same_spec(const scene_spec& a,const scene_spec& b,std::string& why){
	if(a.objects.size() != b.objects.size()){ why = "different number of objects"; return false; }
	if(a.view != b.view){ why = "different view"; return false; }
	for(size_t i = 0;i<a.objects.size();i++){
		const object_spec& x = a.objects[i];
		const object_spec& y = b.objects[i];
		std::string who = "object " + std::to_string(i + 1) + " (" + x.name + ")";
		if(x.name != y.name || x.mesh_file != y.mesh_file){ why = who + ": name or shape"; return false; }
		if(x.color.r != y.color.r || x.color.g != y.color.g || x.color.b != y.color.b){ why = who + ": color"; return false; }
		if(x.size != y.size || x.importance != y.importance){ why = who + ": size or importance"; return false; }
		if(x.relations.size() != y.relations.size()){ why = who + ": number of relations"; return false; }
		for(size_t k = 0;k<x.relations.size();k++){
			if(x.relations[k].kind != y.relations[k].kind || x.relations[k].other != y.relations[k].other){
				why = who + ": relation " + std::to_string(k + 1); return false;
			}
		}
		if(x.motions.size() != y.motions.size()){ why = who + ": number of motions"; return false; }
		for(size_t k = 0;k<x.motions.size();k++){
			const motion& m = x.motions[k];
			const motion& n = y.motions[k];
			if(m.kind != n.kind || m.other != n.other || m.turns != n.turns || m.start != n.start || m.end != n.end){
				why = who + ": motion " + std::to_string(k + 1); return false;
			}
		}
	}
	return true;
}

static bool has_message(const std::vector<parse_message>& list,int line,const std::string& part){
	for(const parse_message& m : list){
		if(m.line == line && m.text.find(part) != std::string::npos) return true;
	}
	return false;
}

static void test_scene_language(){
	std::printf("scene language:\n");

	// the Levenshtein examples from docs/21
	check(edit_distance("spher", "sphere") == 1, "edit_distance(spher, sphere) = 1");
	check(edit_distance("cueb", "cube") == 1, "edit_distance(cueb, cube) = 1 (two neighbours swapped)");
	check(edit_distance("kitten", "sitting") == 3, "edit_distance(kitten, sitting) = 3");

	// every .dan file gives exactly the same scene as its C++ version
	struct pair{ const char* file; scene_spec spec; };
	for(const pair& p : {pair{"scenes/lazy.dan", lazy_ai_scene()}, pair{"scenes/motion.dan", lazy_motion_scene()},
	                     pair{"scenes/impact.dan", impact_scene()}}){
		parse_result r = parse_scene_file(p.file);
		std::string why;
		bool same = r.ok() && same_spec(r.spec, p.spec, why);
		check(same, std::string(p.file) + " parses to exactly the C++ scene" + (same ? "" : " (" + why + ")"));
	}

	// a broken scene: every mistake found, on the right line, with a suggestion
	const char* broken =
		"scene \"broken\" view front_abuve\n"          // 1: unknown view
		"cube = cueb big orange\n"                       // 2: unknown shape
		"sphere = sphere blu near cube\n"                // 3: unknown word, did you mean blue
		"comet = pyramid hits cube at 12\n"              // 4: a time without 's'
		"planet = octahedron orbits cube 1 turn 0s 20s\n" // 5: no '-' between the times
		"sphre near cube\n"                              // 6: a fact about an object that doesn't exist
		"ring = torus near planit\n";                    // 7: fine, but 'planit' is probably 'planet'
	parse_result r = parse_scene(broken);
	check(has_message(r.errors, 1, "unknown view") && has_message(r.errors, 1, "front_above"), "line 1: unknown view, did you mean front_above");
	check(has_message(r.errors, 2, "expected a shape") && has_message(r.errors, 2, "did you mean cube"), "line 2: unknown shape 'cueb', did you mean cube");
	check(has_message(r.errors, 3, "did you mean blue"), "line 3: unknown word 'blu', did you mean blue");
	check(has_message(r.errors, 4, "times need an 's'"), "line 4: a time written without 's'");
	check(has_message(r.errors, 5, "expected '-'"), "line 5: no '-' between the two times");
	check(has_message(r.errors, 6, "did you mean sphere"), "line 6: a fact about 'sphre', did you mean sphere");
	check(has_message(r.notes, 7, "did you mean planet"), "line 7: a note that 'planit' is probably 'planet'");
	check(r.errors.size() == 6, "every broken line is reported, not just the first (" + std::to_string(r.errors.size()) + " errors)");
}

// ---- live reload (docs/22) ----
static void write_file(const std::string& path,const std::string& text,int seconds_later){
	std::ofstream(path) << text;
	// file times can be coarse, so make sure each version looks newer
	auto when = std::filesystem::file_time_type::clock::now() + std::chrono::seconds(seconds_later);
	std::filesystem::last_write_time(path, when);
}

static bool contains(const std::string& text,const std::string& part){
	return text.find(part) != std::string::npos;
}

static void test_live_reload(){
	std::printf("live reload:\n");
	std::string path = "engine_test_live.dan";

	write_file(path, "sun = sphere big gold important\n", 1);
	live_scene live(path, motion_plan::method::framed);
	check(live.has_scene() && live.objects().size() == 1, "reads the file when it starts (1 object)");
	std::ifstream report_file(live.report_path());
	std::string report((std::istreambuf_iterator<char>(report_file)), std::istreambuf_iterator<char>());
	check(contains(report, "layout (framed): 1 objects"), "writes the solver's report next to it (" + live.report_path() + ")");
	check(!live.check_now(), "nothing changed: no reload");

	write_file(path, "sun = sphere big gold important\nrock = icosahedron grey near sun\n", 2);
	check(live.check_now() && live.objects().size() == 2, "the file was saved again: reloaded (2 objects)");

	write_file(path, "sun = spher big gold\n", 3);
	check(!live.check_now() && live.objects().size() == 2, "a broken version: the last good scene stays (still 2 objects)");
	check(contains(live.last_report(), "did you mean sphere") && contains(live.last_report(), "last good version"),
	      "... and the report says what's wrong, and that the old scene is still showing");

	std::filesystem::remove(path);
	std::filesystem::remove(live.report_path());
}

int main(){
	test_clipping();
	test_fly_camera();
	test_easing();
	test_looping();
	test_scene_language();
	test_live_reload();
	std::printf("\n%s: %d check%s failed\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
	return failures == 0 ? 0 : 1;
}
