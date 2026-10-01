// ============================================================================
//  Tests for the engine itself (docs/19-21): the parts that aren't the
//  layout solver. No window and no SDL, so they run anywhere.
//
//  Exits with 1 if anything failed, so `make test` stops.
// ============================================================================
#include "camera.h"
#include "fly_camera.h"
#include "render.h"
#include "timeline.h"

#include <cmath>

#include <cstdio>
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

// ---- looping (docs/20) ----
static void test_looping(){
	std::printf("looping:\n");
	check(loop_time(7.0f, 20.0f) == 7.0f, "before the end, time is unchanged (7 s -> 7 s)");
	check(loop_time(25.0f, 20.0f) == 5.0f, "after the end, it starts again (25 s -> 5 s)");
	check(loop_time(40.0f, 20.0f) == 0.0f, "two whole loops later it's back at 0 (40 s -> 0 s)");
	check(loop_time(5.0f, 0.0f) == 0.0f, "a video with no length doesn't divide by zero");
}

int main(){
	test_clipping();
	test_fly_camera();
	test_looping();
	std::printf("\n%s: %d check%s failed\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
	return failures == 0 ? 0 : 1;
}
