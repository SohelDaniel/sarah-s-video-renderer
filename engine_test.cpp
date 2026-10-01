// ============================================================================
//  Tests for the engine itself (docs/19-22): the parts that aren't the
//  layout solver. No window and no SDL, so they run anywhere.
//
//  Exits with 1 if anything failed, so `make test` stops.
// ============================================================================
#include "camera.h"
#include "fly_camera.h"
#include "expression.h"
#include "font.h"
#include "label_layout.h"
#include "live_scene.h"
#include "math_layout.h"
#include "object.h"
#include "mesh.h"
#include "render.h"
#include "scene_parser.h"
#include "shapes2d.h"
#include "srgb.h"
#include "test_scenes.h"
#include "timeline.h"
#include "vpath.h"

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

// ---- outline fonts (docs/29) ----
static void test_fonts(){
	std::printf("outline fonts:\n");
	std::vector<int> codes = utf8_decode("a\xCF\x80");   // "aπ": π is the two bytes CF 80
	check(codes.size() == 2 && codes[0] == 97 && codes[1] == 960, "UTF-8: \"a\\xCF\\x80\" decodes to 97 (a) and 960 (pi)");

	point2 m = bezier_point({0, 0}, {2, 4}, {4, 0}, 0.5f);
	check(std::fabs(m.x - 2) < 1e-6f && std::fabs(m.y - 2) < 1e-6f, "de Casteljau at t = 1/2 of (0,0) (2,4) (4,0) is (2, 2)");
	std::vector<point2> flat;
	flatten_quadratic({0, 0}, {2, 4}, {4, 0}, 0.25f, flat);
	check(flat.size() == 4, "flattened to within 0.25 px: 4 straight pieces (bulge 2 -> 0.5 -> 0.125)");

	// a 4 x 4 square from (2, 2) to (6, 6) on an 8 x 8 grid
	std::vector<float> square = fill_loops({{{2, 2}, {6, 2}, {6, 6}, {2, 6}}}, 8, 8);
	check(square[3 * 8 + 3] == 1.0f && square[0] == 0.0f, "a filled square: inside 1, outside 0");
	// a half-covered pixel: the edge at x = 2.5 cuts pixel 2 in half
	std::vector<float> half = fill_loops({{{2.5f, 0}, {6, 0}, {6, 8}, {2.5f, 8}}}, 8, 8);
	check(std::fabs(half[3 * 8 + 2] - 0.5f) < 1e-6f, "an edge through the middle of a pixel: coverage 0.5");
	// a square with a square hole, the inner loop going the other way: the
	// winding number inside the hole is +1 - 1 = 0, so it stays empty
	std::vector<float> ring = fill_loops({{{0, 0}, {8, 0}, {8, 8}, {0, 8}}, {{2, 2}, {2, 6}, {6, 6}, {6, 2}}}, 8, 8);
	check(ring[4 * 8 + 4] == 0.0f && ring[1 * 8 + 1] == 1.0f, "a loop inside a loop, going the other way, leaves a hole");

	const font* sans = fonts::sans();
	check(sans != nullptr, "fonts/DejaVuSans.ttf loads");
	if(sans){
		float by_hand = sans->advance('A', 20) + sans->kerning('A', 'V', 20) + sans->advance('V', 20);
		check(std::fabs(sans->width("AV", 20) - by_hand) < 1e-4f, "width(\"AV\") = advance(A) + kerning(A, V) + advance(V)");
		check(sans->kerning('A', 'V', 20) < 0.0f, "A and V tuck together: their kerning is negative (" + std::to_string(sans->kerning('A', 'V', 20)) + ")");
		const font::glyph& o = sans->get('o', 40);
		float middle = o.coverage[size_t(o.height / 2) * o.width + o.width / 2];
		check(middle == 0.0f, "the middle of an 'o' is empty: its inner loop makes the hole");
		check(!sans->get(960, 30).coverage.empty(), "pi has an outline (the font has Greek)");
	}
}

// ---- math (docs/30) ----
static void test_math(){
	std::printf("math:\n");
	math_parse_result x2 = parse_math("x^2");
	check(x2.error.empty(), "x^2 parses");
	if(fonts::serif() && fonts::italic() && x2.error.empty()){
		math_box b = layout_math(*x2.tree, 30.0f);
		bool shape = b.glyphs.size() == 2;
		float expected_x = fonts::italic()->advance('x', 30) + 30 * 0.04f;
		check(shape && b.glyphs[1].size == 21.0f && b.glyphs[1].y == -13.5f && std::fabs(b.glyphs[1].x - expected_x) < 1e-4f,
		      "the 2 in x^2 is 70% size (21 px), raised 0.45 em (13.5 px), right after the x");

		math_parse_result half = parse_math("\\frac{1}{2}");
		math_box f = layout_math(*half.tree, 30.0f);
		bool ok = f.glyphs.size() == 2 && f.rules.size() == 1;
		check(ok && f.glyphs[0].y < f.rules[0].y && f.glyphs[1].y > f.rules[0].y + f.rules[0].height,
		      "\\frac{1}{2}: the 1 above the bar, the 2 below it");
		check(ok && f.rules[0].width == f.width && f.glyphs[0].size == 25.5f, "the bar is as wide as the fraction; top and bottom are 85% size (25.5 px)");
	}
	math_parse_result pi = parse_math("\\pi");
	check(pi.error.empty() && pi.tree->parts.size() == 1 && pi.tree->parts[0]->codepoint == 960, "\\pi is the character 960");

	math_parse_result typo = parse_math("\\frax{1}{2}");
	check(typo.column == 1 && typo.error.find("did you mean \\frac") != std::string::npos, "\\frax: unknown, at column 1, did you mean \\frac");
	math_parse_result open = parse_math("x^{2");
	check(open.column == 3 && open.error.find("never closed") != std::string::npos, "x^{2: the { at column 3 is never closed");

	parse_result scene = parse_scene("math \"E = mc^\" 0s-5s\n");
	check(!scene.ok() && scene.errors[0].column == 13, "a broken formula in a .dan file points at the exact character: the ^ in column 13");
}

// ---- labels (docs/28): the original notes, replayed ----
static void test_labels(){
	std::printf("labels:\n");

	// the notes' greedy example: a 200 x 100 screen, labels 40 x 10, dots of
	// radius 2, gap 3. A at (100, 50), B at (106, 58).
	label_layout layout(200, 100);
	std::vector<label_request> ab = {{40, 10, {100, 50}, 2}, {40, 10, {106, 58}, 2}};
	const std::vector<placed_label>& got = layout.place(ab);
	box2 a = got[0].where, b = got[1].where;
	check(got[0].side == 0 && a.x0 == 105 && a.x1 == 145 && a.y0 == 45 && a.y1 == 55,
	      "A's label goes right: [105, 145] x [45, 55], as in the notes");
	check(got[1].side == 1 && b.x0 == 61 && b.x1 == 101 && b.y0 == 53 && b.y1 == 63,
	      "B's right spot overlaps A's label, so B goes left: [61, 101] x [53, 63]");

	// the notes' gradient example: home (81, 58), an obstacle at (84, 62), R = 10
	point2 p1 = label_layout::gradient_step({81, 58}, {81, 58}, {84, 62}, 10.0f);
	point2 p2 = label_layout::gradient_step(p1, {81, 58}, {84, 62}, 10.0f);
	check(std::fabs(p1.x - 80.4f) < 1e-4f && std::fabs(p1.y - 57.2f) < 1e-4f, "gradient step 1: (81, 58) -> (80.4, 57.2)");
	check(std::fabs(p2.x - 80.04f) < 1e-4f && std::fabs(p2.y - 56.72f) < 1e-4f, "gradient step 2: -> (80.04, 56.72)");

	// ... and where it settles: 7.5 from the obstacle, not the 10 asked for
	point2 p = {81, 58};
	for(int k = 0;k<200;k++) p = label_layout::gradient_step(p, {81, 58}, {84, 62}, 10.0f);
	float d = std::sqrt((p.x - 84) * (p.x - 84) + (p.y - 62) * (p.y - 62));
	check(std::fabs(d - 7.5f) < 1e-3f, "it settles 7.5 from the obstacle (spring and push balance), not 10 (" + std::to_string(d) + ")");

	parse_result lp = parse_scene("sun = sphere big gold label \"the sun\"\n");
	check(lp.ok() && lp.spec.objects[0].label == "the sun", "'label \"the sun\"' is read as the sun's label");

	// a crowd: 15 labels around points close together: no two shown labels overlap
	label_layout crowd(640, 480);
	std::vector<label_request> many;
	for(int k = 0;k<15;k++){
		float angle = k * 0.42f;
		many.push_back({56, 16, {320 + 70 * std::cos(angle) * (1 + k % 3), 240 + 50 * std::sin(angle)}, 14});
	}
	const std::vector<placed_label>& out = crowd.place(many);
	int shown = 0, overlaps = 0;
	for(size_t i = 0;i<out.size();i++){
		if(out[i].shown) shown++;
		for(size_t j = i + 1;j<out.size();j++) if(out[i].shown && out[j].shown && overlap(out[i].where, out[j].where)) overlaps++;
	}
	check(overlaps == 0, "15 crowded labels: " + std::to_string(shown) + " shown, 0 overlapping (" + std::to_string(overlaps) + ")");

	// timed words fade in and out over 0.3 s (docs/28)
	check(appear(2, 8, 1.9f) == 0.0f && appear(2, 8, 5) == 1.0f && appear(2, 8, 8.5f) == 0.0f,
	      "a label shown 2s-8s: hidden at 1.9 s, there at 5 s, gone at 8.5 s");
	check(std::fabs(appear(2, 8, 2.15f) - 0.5f) < 1e-5f && std::fabs(appear(2, 8, 7.85f) - 0.5f) < 1e-5f,
	      "halfway through its 0.3 s fade-in (2.15 s) and fade-out (7.85 s) it's half visible");
	check(appear(0, -1, 0) == 1.0f, "a label with no time range is simply always there, from the first frame");

	parse_result opts = parse_scene("sun = sphere label math \"m_1\" 2s-8s always\n");
	const object_spec& sun = opts.spec.objects[0];
	check(opts.ok() && sun.label_math && sun.label_always && sun.label_start == 2.0f && sun.label_end == 8.0f,
	      "'label math \"m_1\" 2s-8s always' is read as a formula, shown 2 s to 8 s, never hidden");

	// the crowd again: the one label that was hidden, marked always-on, is shown
	size_t hidden = 0;
	for(size_t i = 0;i<out.size();i++) if(!out[i].shown) hidden = i;
	label_layout crowd2(640, 480);
	std::vector<label_request> pinned = many;
	pinned[hidden].always = true;
	const std::vector<placed_label>& again = crowd2.place(pinned);
	check(again[hidden].shown, "the label that had no room is shown once it's marked always (label " + std::to_string(hidden) + ")");

	// flicker: B's object jitters back and forth (30 pixels) just right of
	// A's, for 120 frames; count how often the labels switch side
	auto flips = [](bool keep){
		label_layout l(640, 480);
		l.hysteresis = keep;
		for(int f = 0;f<120;f++){
			float x = 415 + 30 * std::sin(f * 0.9f);
			std::vector<label_request> two = {{60, 16, {320, 240}, 20}, {60, 16, {x, 240}, 20}};
			l.place(two);
		}
		return l.side_changes();
	};
	int with = flips(true), without = flips(false);
	check(with < without, "keeping last frame's side means fewer flips: " + std::to_string(with) + " with, "
	      + std::to_string(without) + " without");
}

// ---- text (docs/27) ----
static void test_text(){
	std::printf("text:\n");
	camera cam;
	cam.width = 24;
	cam.height = 12;
	cam.update(0.0f);
	render r(cam.width, cam.height);
	r.begin(cam);
	r.draw_bitmap_text(0, 0, "A", 1, px::Pixel(255, 255, 255));
	r.finish();
	const px::Image& img = r.picture();
	auto lit = [&](int x,int y){ px::Pixel p = img.Get(x, y); return p.r == 255 && p.g == 255 && p.b == 255; };

	// row 0 of 'A' is 0x0C = 0000 1100: lowest bit first, so pixels 2 and 3
	std::string row0;
	for(int x = 0;x<8;x++) row0 += lit(x, 0) ? 'X' : '.';
	check(row0 == "..XX....", "row 0 of 'A' (0x0C) lights pixels 2 and 3: " + row0);
	// row 4 is 0x3F = 0011 1111: pixels 0 to 5
	std::string row4;
	for(int x = 0;x<8;x++) row4 += lit(x, 4) ? 'X' : '.';
	check(row4 == "XXXXXX..", "row 4 of 'A' (0x3F) lights pixels 0 to 5: " + row4);
	check(render::bitmap_text_width("hello", 3) == 120, "'hello' at scale 3 is 5 x 8 x 3 = 120 pixels wide");

	parse_result p = parse_scene("title \"Orbits\" 0s-5s\nsun = sphere\n");
	check(p.ok() && p.spec.titles.size() == 1 && p.spec.titles[0].text == "Orbits" && p.spec.titles[0].end == 5.0f,
	      "'title \"Orbits\" 0s-5s' is read as a title shown for the first 5 s");
}

// ---- lines and arrows (docs/26) ----
static void test_lines(){
	std::printf("lines and arrows:\n");
	// a horizontal line along y = 10, 2 pixels wide
	check(std::fabs(render::line_coverage(5.0f, 10.0f, 0, 10, 20, 10, 2.0f) - 1.0f) < 1e-6f, "a pixel on the line is fully covered");
	check(std::fabs(render::line_coverage(5.0f, 11.0f, 0, 10, 20, 10, 2.0f) - 0.5f) < 1e-6f, "a pixel 1 away from a 2-wide line is half covered (2/2 + 1/2 - 1 = 0.5)");
	check(render::line_coverage(5.0f, 13.0f, 0, 10, 20, 10, 2.0f) == 0.0f, "a pixel 3 away isn't covered at all");
	check(std::fabs(render::line_coverage(23.0f, 14.0f, 0, 10, 20, 10, 2.0f)) < 1e-6f, "past the end, the distance is to the end point (5 away: 0)");

	parse_result p = parse_scene("sun = sphere\ncomet = pyramid\narrow comet sun red 2s-8s\n");
	check(p.ok() && p.spec.arrows.size() == 1 && p.spec.arrows[0].from == "comet" && p.spec.arrows[0].end == 8.0f,
	      "'arrow comet sun red 2s-8s' is read as an arrow from comet to sun, shown from 2 s to 8 s");
}

// ---- fades (docs/24) ----
static void test_fades(){
	std::printf("fades:\n");
	camera cam;
	cam.width = 160;
	cam.height = 120;
	cam.move(vec3(0.0f, 0.0f, 4.0f));
	cam.point_at(vec3(0.0f, 0.0f, 0.0f));
	// a triangle facing the camera (counter-clockwise seen from +z)
	vec3 a(-1, -1, 0), b(1, -1, 0), c(0, 1, 0);

	render solid(cam.width, cam.height);
	solid.begin(cam);
	solid.draw(a, b, c, px::Pixel(220, 220, 220));
	render half(cam.width, cam.height);
	half.begin(cam);
	half.draw(a, b, c, px::Pixel(220, 220, 220), 0.5f);

	px::Pixel s = solid.picture().Get(80, 62);   // inside the triangle
	px::Pixel h = half.picture().Get(80, 62);
	px::Pixel bg = solid.picture().Get(2, 2);     // the background
	// halfway in LIGHT, not in bytes (docs/35): 101, where the bytes' halfway is 79
	float alpha = 128.0f / 255.0f;
	int expected = srgb::to_byte(alpha * srgb::to_linear(s.r) + (1.0f - alpha) * srgb::to_linear(bg.r));
	check(std::abs(h.r - expected) <= 1, "half see-through = halfway in light between it and the background (" + std::to_string(h.r)
	      + ", expected " + std::to_string(expected) + ")");

	// see-through must not hide what's behind it, even if drawn first
	render order(cam.width, cam.height);
	order.begin(cam);
	order.draw(a, b, c, px::Pixel(220, 220, 220), 0.5f);   // in front, at z = 0
	order.draw(vec3(-1, -1, -1), vec3(1, -1, -1), vec3(0, 1, -1), px::Pixel(250, 50, 50));   // behind, solid red
	check(order.picture().Get(80, 62).r > order.picture().Get(80, 62).g + 40,
	      "something solid behind a see-through one still shows through");
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
	if(a.arrows.size() != b.arrows.size()){ why = "different number of arrows"; return false; }
	if(a.titles.size() != b.titles.size()){ why = "different number of titles"; return false; }
	for(size_t i = 0;i<a.objects.size();i++){
		const object_spec& x = a.objects[i];
		const object_spec& y = b.objects[i];
		if(x.label != y.label || x.label_math != y.label_math || x.label_always != y.label_always
		   || x.label_start != y.label_start || x.label_end != y.label_end){ why = "labels differ"; return false; }
	}
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
		if(x.fades.size() != y.fades.size()){ why = who + ": number of fades"; return false; }
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

// Any picture size (docs/34): sizes on the picture scale with its height,
// and the solver frames for the picture's shape.
static void test_hd(){
	std::printf("\nany picture size:\n");
	render small(640, 480), big(1920, 1080, 2);
	check(small.ui_scale() == 1.0f && big.ui_scale() == 2.25f,
	      "sizes scale with the height: 1 at 480, 2.25 at 1080 (a 30 px title becomes 67.5 px), samples don't count");

	scene_spec spec = parse_scene_file("scenes/showcase.dan").spec;
	world hd(spec, layout::method::framed, motion_plan::method::framed, 1920, 1080);
	world sd(spec, layout::method::framed, motion_plan::method::framed);
	check(hd.cam.width == 1920 && hd.cam.height == 1080 && sd.cam.width == 640 && sd.cam.height == 480,
	      "a world made for 1920x1080 has a camera of that size; the default is still 640x480");

	std::vector<float> radii;
	for(const object_spec& o : spec.objects){
		// a flat shape fits radius 1 (a graph 2.5, docs/40, 42); a mesh, its own
		if(!o.flat.empty()) radii.push_back(o.flat == "graph" ? graph_scale : 1.0f);
		else radii.push_back(mesh(o.mesh_file).bounding_radius());
	}
	scene_solver wide(spec, radii, layout::method::framed, motion_plan::method::framed, 16.0f / 9.0f);
	scene_solver narrow(spec, radii, layout::method::framed, motion_plan::method::framed);
	check(std::fabs(wide.still().picture_aspect() - 16.0f / 9.0f) < 1e-6f
	      && std::fabs(narrow.still().picture_aspect() - 4.0f / 3.0f) < 1e-6f,
	      "the solver frames for the picture's shape: 16:9 for HD, 4:3 by default");
	layout::metrics m = wide.still().measure();
	check(m.off_screen == 0 && m.hidden == 0 && m.labels_without_room == 0 && wide.moving_off_screen() == 0,
	      "the showcase framed for 16:9: everything in the picture, nothing hidden, every label has room");

	label_layout placer(1920, 1080);
	placer.scale = 2.25f;
	label_request r{40, 10, {500, 500}, 2, true, false};
	const std::vector<placed_label>& placed = placer.place({r});
	check(placed[0].shown && std::fabs(placed[0].where.x0 - (500 + 2 + 3 * 2.25f)) < 1e-4f,
	      "label gaps scale too: 3 px becomes 6.75 px at 1080");
}

// Mixing colors as light, not as bytes (docs/35).
static void test_linear_light(){
	std::printf("\nlinear light:\n");
	bool round_trip = true;
	for(int b = 0;b<256;b++) if(srgb::to_byte(srgb::to_linear(uint8_t(b))) != b) round_trip = false;
	check(round_trip, "every byte turned into light and back gives the same byte (all 256)");
	check(srgb::to_linear(0) == 0.0f && srgb::to_linear(255) == 1.0f, "the ends are exact: 0 is no light, 255 is all of it");
	check(std::fabs(srgb::to_linear(128) - 0.2158f) < 1e-4f, "byte 128 is only 21.6% of the light, not half");

	px::Image black(1, 1, px::Pixel(0, 0, 0));
	srgb::blend(black, 0, 0, px::Pixel(255, 255, 255, 128));
	int light = black.Get(0, 0).r;
	px::Image bytes(1, 1, px::Pixel(0, 0, 0));
	bytes.Draw(0, 0, px::Pixel(255, 255, 255, 128));
	check(light == 188 && bytes.Get(0, 0).r == 128,
	      "white covering half a black pixel: 188 mixed as light, 128 mixed as bytes (" + std::to_string(light) + ")");
}

// Smooth shading (docs/36): corner normals with a crease angle.
static void test_smooth_shading(){
	std::printf("\nsmooth shading:\n");
	mesh ball("shapes/sphere.obj");
	float worst = 0.0f;   // the biggest angle between a corner's normal and "straight out from the center"
	for(int i = 0;i<ball.get_faces_count();i++){
		for(int k = 0;k<3;k++){
			vec3 out = normalize(ball.vertex(ball.face(i)[k] - 1));
			float c = std::clamp(dot(ball.corner_normal(i, k), out), -1.0f, 1.0f);
			worst = std::max(worst, std::acos(c) * 180.0f / 3.14159265f);
		}
	}
	check(worst < 1.0f, "every corner of the sphere points straight out from its center (worst " + std::to_string(worst) + " degrees)");

	mesh box("shapes/cube.obj");
	bool sharp = true;
	for(int i = 0;i<box.get_faces_count();i++){
		triangle t = box.face(i);
		vec3 a = box.vertex(t[0] - 1), b = box.vertex(t[1] - 1), c = box.vertex(t[2] - 1);
		vec3 face = normalize(cross(b - a, c - a));
		for(int k = 0;k<3;k++) if(dot(box.corner_normal(i, k), face) < 0.9999f) sharp = false;
	}
	check(sharp, "a cube's corners keep their own face's normal (its faces meet at 90 degrees, past the 40 degree crease)");

	mesh ico("shapes/icosahedron.obj");
	triangle t = ico.face(0);
	vec3 a = ico.vertex(t[0] - 1), b = ico.vertex(t[1] - 1), c = ico.vertex(t[2] - 1);
	check(dot(ico.corner_normal(0, 0), normalize(cross(b - a, c - a))) > 0.9999f,
	      "an icosahedron stays faceted too: its faces meet at 41.8 degrees, just past the crease");

	// page 08's lit triangle, facing the camera: Lambert gives 220 · 0.6235 =
	// 137.2, and the highlight adds 0.25 · 0.8823^32 · 255 = 1.16
	camera cam;
	cam.width = 160;
	cam.height = 120;
	cam.move(vec3(0.0f, 0.0f, 4.0f));
	cam.point_at(vec3(0.0f, 0.0f, 0.0f));
	render lit(cam.width, cam.height);
	lit.begin(cam);
	lit.draw(vec3(-1, -1, 0), vec3(1, -1, 0), vec3(0, 1, 0), px::Pixel(220, 220, 220));
	int grey = lit.picture().Get(80, 62).r;
	check(grey == 138, "a grey (220) face towards the camera: 137.2 from the light + 1.16 highlight = 138 (" + std::to_string(grey) + ")");

	vec3 n = normalize(vec3(1, 0, 0) * 0.5f + vec3(0, 1, 0) * 0.5f);
	check(std::fabs(std::sqrt(dot(n, n)) - 1.0f) < 1e-6f && std::fabs(n[0] - 0.7071f) < 1e-4f,
	      "halfway between two normals, made 1 long again: (0.7071, 0.7071, 0)");
}

// Vector paths (docs/37): text and formulas as loops of points.
static void test_vector_paths(){
	std::printf("\nvector paths:\n");
	const font* sans = fonts::sans();
	if(!sans){ check(false, "the fonts are in fonts/"); return; }
	vgroup e = text_paths("E = m", *sans, 30.0f);
	check(e.pieces.size() == 3 && e.pieces[0].key == 69 && e.pieces[1].key == 61 && e.pieces[2].key == 109,
	      "\"E = m\" is 3 pieces: E (69), = (61), m (109); the spaces have no ink, so no piece");

	std::vector<point2> square = {{0, 0}, {10, 0}, {10, 10}, {0, 10}};
	std::vector<point2> quarter = loop_prefix(square, 0.25f), more = loop_prefix(square, 0.3f);
	check(std::fabs(loop_length(square) - 40.0f) < 1e-5f, "a 10 x 10 square's loop is 40 long (4 sides, back to the start)");
	check(std::fabs(quarter.back().x - 10) < 1e-5f && std::fabs(quarter.back().y) < 1e-5f
	      && std::fabs(more.back().x - 10) < 1e-5f && std::fabs(more.back().y - 2) < 1e-5f,
	      "its first 0.25 ends at the corner (10, 0); its first 0.3 (12 long) at (10, 2)");

	math_parse_result half = parse_math("\\frac{1}{2}");
	vgroup f = math_paths(layout_math(*half.tree, 30.0f));
	bool bar = false;
	for(const vpiece& p : f.pieces) if(p.key == -1 && p.loops.size() == 1 && p.loops[0].size() == 4) bar = true;
	check(f.pieces.size() == 3 && bar, "\\frac{1}{2} is 3 pieces: 1, 2, and the bar as a 4-corner loop");

	// the same 'o' drawn still (the glyph cache) and animated (filled from its loops)
	camera cam;
	cam.width = 60;
	cam.height = 60;
	cam.update(0.0f);
	vgroup o = text_paths("o", *sans, 40.0f);
	render still(cam.width, cam.height), moving(cam.width, cam.height);
	still.begin(cam);
	moving.begin(cam);
	piece_look nearly;
	nearly.fill = 0.99999f;   // not still(): takes the loops path
	still.draw_vpiece(o.pieces[0], 10.0f, 45.0f, px::Pixel(255, 255, 255), piece_look{}, 0.0f);
	moving.draw_vpiece(o.pieces[0], 10.0f, 45.0f, px::Pixel(255, 255, 255), nearly, 0.0f);
	still.finish();
	moving.finish();
	int worst = 0;
	for(int y = 0;y<cam.height;y++) for(int x = 0;x<cam.width;x++){
		worst = std::max(worst, std::abs(int(still.picture().Get(x, y).r) - int(moving.picture().Get(x, y).r)));
	}
	check(worst <= 1, "an 'o' filled from its loops looks the same as its cached glyph (at most " + std::to_string(worst) + " apart)");
}

// Write (docs/38): letters drawn in, outline first, then filled.
static void test_write(){
	std::printf("\nwrite:\n");
	// 5 pieces ("E = mc^2"): each takes w = 1/(1 + 0.2·4) = 0.5556 of the time
	float w = 1.0f / 1.8f;
	check(std::fabs(piece_progress(2, 5, 2 * 0.2f * w)) < 1e-5f && std::fabs(piece_progress(2, 5, 2 * 0.2f * w + w) - 1.0f) < 1e-5f,
	      "of 5 pieces, the 3rd starts at 0.2222 and takes 0.5556 of the writing");
	check(piece_progress(4, 5, 0.4444f) < 0.001f && piece_progress(4, 5, 1.0f) == 1.0f && piece_progress(0, 5, w) == 1.0f,
	      "the last starts at 0.4444 and ends exactly at 1; the first is done at 0.5556");

	piece_look start = border_then_fill(0.0f), quarter = border_then_fill(0.25f), three = border_then_fill(0.75f), done = border_then_fill(1.0f);
	check(start.stroke == 0.0f && start.fill == 0.0f, "at 0 nothing shows: no outline drawn yet, no fill");
	check(std::fabs(quarter.stroke - 0.5f) < 1e-5f && quarter.fill == 0.0f && quarter.stroke_alpha == 1.0f,
	      "at 0.25 half the outline is drawn (smooth(0.5) = 0.5), and nothing is filled");
	check(three.stroke == 1.0f && std::fabs(three.fill - 0.5f) < 1e-5f && std::fabs(three.stroke_alpha - 0.5f) < 1e-5f,
	      "at 0.75 the outline is whole, the fill half up, the outline half faded");
	check(done.still(), "at 1 it's simply there (drawn like any still letter)");

	math_parse_result half = parse_math("\\frac{1}{2}");
	vgroup f = math_paths(layout_math(*half.tree, 30.0f));
	check(f.pieces.size() == 3 && f.pieces[0].key == -1, "a fraction's bar is written first, before its top and bottom");

	parse_result two = parse_scene("math \"E = mc^2\" 0s-10s write 2s\ntitle \"Hi\" write\n");
	check(two.ok() && two.spec.maths[0].write == 2.0f && two.spec.titles[0].write == 1.0f,
	      "'write 2s' writes it in over 2 s; 'write' alone takes 1 s");
	parse_result zero = parse_scene("title \"Hi\" 0s-5s write 0s\n");
	parse_result longer = parse_scene("title \"Hi\" 2s-3s write 2s\n");
	parse_result minus = parse_scene("title \"Hi\" write -1s\n");
	check(!zero.ok() && !longer.ok() && !minus.ok() && minus.errors[0].column == 18,
	      "'write 0s', writing for longer than it's shown, and 'write -1s' are mistakes (the '-' at column 18)");
}

// Transform (docs/39): matching the pieces of two formulas.
static void test_transform(){
	std::printf("\ntransform:\n");
	auto paths = [](const char* text){ return math_paths(layout_math(*parse_math(text).tree, 30.0f)); };
	auto count = [](const std::vector<int>& to,size_t in_b,int& matched,int& gone,int& added){
		matched = 0;
		for(int j : to) if(j >= 0) matched++;
		gone = int(to.size()) - matched;
		added = int(in_b) - matched;
	};
	vgroup a = paths("E = mc^2"), b = paths("E^2 = (mc^2)^2 + (pc)^2"), c = paths("E = \\gamma mc^2");
	std::vector<int> ab = match_pieces(a, b), bc = match_pieces(b, c);
	int matched, gone, added;
	count(ab, b.pieces.size(), matched, gone, added);
	check(matched == 5 && gone == 0 && added == 10 && ab == std::vector<int>({0, 2, 4, 5, 6}),
	      "E = mc^2 -> E^2 = (mc^2)^2 + (pc)^2: E, =, m, c, 2 glide (to pieces 0, 2, 4, 5, 6), 10 new ones fade in");
	count(bc, c.pieces.size(), matched, gone, added);
	check(matched == 5 && gone == 10 && added == 1 && bc[1] == -1 && bc[6] == 5,
	      "-> E = \\gamma mc^2: 5 glide, 10 fade out (E's own 2 among them: it isn't the 2 of c^2), gamma fades in");

	std::vector<int> small = match_pieces(paths("E^2 = c^2"), paths("E = c^2"));
	check(small == std::vector<int>({0, -1, 1, 2, 3}),
	      "E^2 = c^2 -> E = c^2: E, =, c, 2 match; E's own 2 fades (the table in docs/39)");

	parse_result chain = parse_scene("math \"a\" 0s-12s write 1s becomes \"b\" at 2s-3s becomes \"c\" at 5s-6s\n");
	check(chain.ok() && chain.spec.maths[0].becomes.size() == 2 && chain.spec.maths[0].becomes[1].text == "c"
	      && chain.spec.maths[0].becomes[1].start == 5.0f,
	      "'becomes \"b\" at 2s-3s becomes \"c\" at 5s-6s' is read as two changes, in order");
	parse_result early = parse_scene("math \"a\" 0s-12s write 2s becomes \"b\" at 1s-3s\n");
	parse_result late = parse_scene("math \"a\" 0s-5s becomes \"b\" at 4s-6s\n");
	parse_result broken = parse_scene("math \"a\" becomes \"x^\" at 1s-2s\n");
	check(!early.ok() && !late.ok() && !broken.ok() && broken.errors[0].column == 20,
	      "changing before the writing is done, or after the formula is gone, is a mistake; a broken new formula points at its '^' (column 20)");
}

// Flat shapes (docs/40): 2D drawings standing in the 3D world.
static void test_flat_shapes(){
	std::printf("\nflat shapes:\n");
	camera cam;
	cam.width = 200;
	cam.height = 200;
	cam.move(vec3(0.0f, 0.0f, 10.0f));
	cam.point_at(vec3(0.0f, 0.0f, 0.0f));
	cam.update(0.0f);
	flat_shape ring = circle_shape();
	object o(ring, px::Pixel(255, 255, 255));
	vec3 d = normalize(vec3(0.0f, 0.0f, 10.0f));                     // back towards the camera
	o.rotate(std::atan2(d[0], d[2]), -std::asin(d[1]));
	o.update(0.0f);
	mat4<float> model = o.model_matrix();
	// how wide and tall the circle is on screen, from a camera
	auto extent = [&](const camera& c,float& wide,float& tall){
		render r(c.width, c.height);
		r.begin(c);
		float xs[4], ys[4], rr;
		point2 rim[4] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
		for(int k = 0;k<4;k++) r.where_on_screen(transform_point(model, vec3(rim[k].x, rim[k].y, 0.0f)), 0.0f, xs[k], ys[k], rr);
		wide = std::fabs(xs[0] - xs[1]);
		tall = std::fabs(ys[2] - ys[3]);
	};
	float wide, tall;
	extent(cam, wide, tall);
	check(std::fabs(wide - tall) < 0.5f, "from the camera it faces, a circle is round on screen (" + std::to_string(wide) + " across, " + std::to_string(tall) + " up)");
	camera side = cam;
	side.move(vec3(10.0f * std::sin(1.0472f), 0.0f, 10.0f * std::cos(1.0472f)));   // 60 degrees round to the side
	side.update(0.0f);
	float side_wide, side_tall;
	extent(side, side_wide, side_tall);
	float squash = side_wide / side_tall;
	check(std::fabs(squash - 0.5f) < 0.03f, "60 degrees round to the side it's an ellipse, about cos 60 = 0.5 as wide as tall (" + std::to_string(squash) + ")");

	// hidden behind a solid cube, seen in front of it
	mesh box("shapes/cube.obj");
	flat_shape square = make_flat_shape("square", true);
	auto middle = [&](float z){
		object sq(square, px::Pixel(80, 160, 230));
		sq.move(vec3(0.0f, 0.0f, z));
		sq.scale(2.0f);
		sq.update(0.0f);
		render r(cam.width, cam.height);
		r.begin(cam);
		r.draw_mesh(box, mat4<float>::identity(), px::Pixel(230, 130, 60));
		sq.draw(r);
		r.finish();
		return r.picture().Get(100, 100);
	};
	px::Pixel behind = middle(-3.0f), in_front = middle(3.0f);
	check(behind.b < 100 && in_front.b > behind.b + 40,
	      "a filled square behind a solid cube is hidden; in front of it, it shows (blue " + std::to_string(behind.b) + " vs " + std::to_string(in_front.b) + ")");

	parse_result ok = parse_scene("sun = sphere\nring = circle teal filled right_of sun\n");
	parse_result typo = parse_scene("ring = cirlce teal\n");
	parse_result solid = parse_scene("box = cube filled\n");
	check(ok.ok() && ok.spec.objects[1].flat == "circle" && ok.spec.objects[1].filled && ok.spec.objects[1].mesh_file.empty(),
	      "'circle teal filled right_of sun' is a filled flat circle, with no mesh file");
	check(!typo.ok() && typo.errors[0].text.find("did you mean circle") != std::string::npos && !solid.ok(),
	      "'cirlce' gets a did-you-mean; 'filled' on a cube is a mistake (only flat shapes can be filled)");
}

// Create (docs/41): flat shapes drawn in along their outline.
static void test_create(){
	std::printf("\ncreate:\n");
	flat_path open;
	open.points = {{0, 0}, {3, 0}, {3, 4}};
	open.closed = false;
	std::vector<point2> half = path_prefix(open, 0.5f);
	check(half.size() == 3 && std::fabs(half.back().x - 3) < 1e-5f && std::fabs(half.back().y - 0.5f) < 1e-5f,
	      "an open path 3 + 4 = 7 long: its first half (3.5) ends at (3, 0.5); no side back to the start");
	flat_shape square = polygon_shape(4);
	std::vector<point2> part = path_prefix(square.paths[0], 0.6f);
	check(std::fabs(part.back().x + 0.7071f) < 1e-3f && std::fabs(part.back().y + 0.1414f) < 1e-3f,
	      "a square's outline (4 x 1.414 = 5.657) at 0.6: 3.394 = 2 sides + 0.566, at (-0.7071, -0.1414)");

	flat_look line = create_look(0.5f, false), filled_early = create_look(0.25f, true), filled_late = create_look(0.75f, true);
	check(std::fabs(line.drawn - 0.5f) < 1e-5f && line.fill == 0.0f, "an outline at 0.5 is half drawn (smooth(0.5))");
	check(std::fabs(filled_early.drawn - 0.5f) < 1e-5f && filled_early.fill == 0.0f
	      && filled_late.drawn == 1.0f && std::fabs(filled_late.fill - 0.5f) < 1e-5f,
	      "a filled one: at 0.25 the outline is half drawn, at 0.75 it's whole and the fill half up");
	flat_look before = create_look(-0.2f, true), after = create_look(1.0f, true);
	check(before.drawn == 0.0f && before.fill == 0.0f && after.drawn == 1.0f && after.fill == 1.0f,
	      "before it starts nothing is drawn; at the end all of it");

	parse_result timed = parse_scene("ring = circle create 1s-2s\nbox = square create\n");
	parse_result solid = parse_scene("box = cube create 1s-2s\n");
	parse_result back = parse_scene("ring = circle create 2s-1s\n");
	check(timed.ok() && timed.spec.objects[0].create_start == 1.0f && timed.spec.objects[0].create_end == 2.0f
	      && timed.spec.objects[1].create_end == 1.0f && !solid.ok() && !back.ok(),
	      "'create 1s-2s' and 'create' (0 s to 1 s) are read; create on a cube, or ending before it starts, are mistakes");
}

// Graphs (docs/42): expressions, ticks and curves.
static void test_graphs(){
	std::printf("\ngraphs:\n");
	auto value = [](const char* text,float x){
		expression_result r = parse_expression(text);
		return r.tree ? evaluate(*r.tree, x) : NAN;
	};
	check(std::fabs(value("2*sin(x)+1", 3.14159265f / 6.0f) - 2.0f) < 1e-5f, "2*sin(x)+1 at x = pi/6 is 2 * 0.5 + 1 = 2");
	check(value("-x^2", 3.0f) == -9.0f && value("2^3^2", 0.0f) == 512.0f,
	      "-x^2 at 3 is -(3^2) = -9; 2^3^2 is 2^(3^2) = 512 (powers go right to left)");
	expression_result typo = parse_expression("sinn(x)"), juxt = parse_expression("2x");
	check(!typo.tree && typo.column == 1 && typo.error.find("did you mean sin") != std::string::npos
	      && juxt.error.find("write *") != std::string::npos,
	      "'sinn(x)' is unknown at column 1 (did you mean sin?); '2x' says to write 2*x");

	check(tick_step(6.28f) == 1.0f && tick_step(12.56f) == 2.0f && std::fabs(tick_step(2.2f) - 0.5f) < 1e-6f,
	      "tick steps: 1 for a range of 6.28, 2 for 12.56, 0.5 for 2.2 (at most 10 ticks)");

	expression_result log = parse_expression("log(x)");
	flat_shape g = graph_shape(*log.tree, -1.0f, 1.0f, nullptr);
	int curves = 0;
	bool right_half = true;
	for(const flat_path& p : g.paths){
		if(p.role != flat_path::curve) continue;
		curves++;
		for(const point2& q : p.points) if(q.x <= 0.0f) right_half = false;   // x = 0 is the middle of the box
	}
	check(curves == 1 && right_half, "log(x) from -1 to 1: one curve, only where x > 0 (no value to draw for x <= 0)");

	parse_result ok = parse_scene("g = graph \"sin(x)\" from -3.14 to 3.14 teal\n");
	parse_result bad = parse_scene("g = graph \"sin(x\"\n");
	parse_result backwards = parse_scene("g = graph \"x\" from 3 to -3\n");
	check(ok.ok() && ok.spec.objects[0].flat == "graph" && std::fabs(ok.spec.objects[0].graph_from + 3.14f) < 1e-5f
	      && !bad.ok() && bad.errors[0].column == 15 && !backwards.ok(),
	      "'graph \"sin(x)\" from -3.14 to 3.14' is read; an unclosed '(' points at column 15; from 3 to -3 is a mistake");
}

// Morph (docs/43): one flat shape melting into another.
static void test_morph(){
	std::printf("\nmorph:\n");
	flat_shape square = polygon_shape(4);
	std::vector<point2> eight = resample(square.paths[0].points, 8);
	const float c = 0.7071f;
	std::vector<point2> expect = {{c, c}, {c, 0}, {c, -c}, {0, -c}, {-c, -c}, {-c, 0}, {-c, c}, {0, c}};
	bool same = eight.size() == 8;
	for(size_t i = 0;same && i<8;i++) same = std::fabs(eight[i].x - expect[i].x) < 1e-3f && std::fabs(eight[i].y - expect[i].y) < 1e-3f;
	check(same, "a square resampled to 8 points: its 4 corners and the 4 middles of its sides");
	check(std::fabs(signed_area(square.paths[0].points) + 2.0f) < 1e-4f,
	      "its signed area is -2: area 2 (sides 1.414), negative because it goes clockwise");

	std::vector<point2> turned(8), backwards(8);
	for(size_t i = 0;i<8;i++) turned[i] = eight[(i + 3) % 8];
	backwards[0] = eight[0];
	for(size_t i = 1;i<8;i++) backwards[i] = eight[8 - i];
	auto equal = [](const std::vector<point2>& a,const std::vector<point2>& b){
		for(size_t i = 0;i<a.size();i++) if(std::fabs(a[i].x - b[i].x) > 1e-4f || std::fabs(a[i].y - b[i].y) > 1e-4f) return false;
		return true;
	};
	check(equal(align_loop(eight, turned), eight), "the same square started 3 points later is lined back up (offset 3 is best)");
	check(equal(align_loop(eight, backwards), eight), "the same square going the other way round is turned back first");

	flat_shape ring = circle_shape();
	object o(square, px::Pixel(255, 255, 255));
	o.morph_to(ring, 1.0f, 2.0f);
	o.update(0.5f);
	bool before = o.flat_now() == &square;
	o.update(1.0f);
	std::vector<point2> start = o.flat_now()->paths[0].points;
	std::vector<point2> square128 = resample(square.paths[0].points, 128);
	o.update(2.5f);
	check(before && start.size() == 128 && equal(start, square128) && o.flat_now() == &ring,
	      "before the morph it's the square; at its start the 128 points are exactly the square's; after it, it's the circle");

	parse_result ok = parse_scene("s = square becomes circle at 2s-3s becomes star at 4s-5s\n");
	parse_result solid = parse_scene("c = cube becomes circle at 2s-3s\n");
	parse_result overlap = parse_scene("s = square becomes circle at 2s-4s becomes star at 3s-5s\n");
	check(ok.ok() && ok.spec.objects[0].changes.size() == 2 && ok.spec.objects[0].changes[1].shape == "star"
	      && !solid.ok() && !overlap.ok(),
	      "'becomes circle at 2s-3s becomes star at 4s-5s' is two changes; a cube can't, and changes can't overlap");
}

// Draw-in (docs/44): 3D objects tracing their edges, then filling in.
static void test_draw_in(){
	std::printf("\ndraw-in:\n");
	mesh box("shapes/cube.obj"), tent("shapes/pyramid.obj"), ball("shapes/sphere.obj");
	check(box.outline_edges().size() == 12 && tent.outline_edges().size() == 8,
	      "a cube traces its 12 creases (not the 6 diagonals across its flat faces); a pyramid its 8");
	check(ball.outline_edges().size() == 1440, "a sphere has no creases, so it traces all 1440 edges of its grid");

	camera cam;
	cam.width = 120;
	cam.height = 120;
	cam.move(vec3(3.0f, 2.0f, 5.0f));
	cam.point_at(vec3(0.0f, 0.0f, 0.0f));
	cam.update(0.0f);
	auto picture = [&](bool drawing,float t){
		object o(box, px::Pixel(230, 130, 60));
		if(drawing) o.draw_in(1.0f, 3.0f);
		o.update(t);
		render r(cam.width, cam.height);
		r.begin(cam);
		o.draw(r);
		r.finish();
		return r.picture();
	};
	px::Image empty = picture(true, 0.5f), solid = picture(false, 0.0f), done = picture(true, 3.0f), half = picture(true, 1.6f);
	px::Image nothing(cam.width, cam.height);
	{ render blank(cam.width, cam.height); blank.begin(cam); blank.finish(); nothing = blank.picture(); }
	auto differ = [&](const px::Image& a,const px::Image& b){
		int n = 0;
		for(int y = 0;y<cam.height;y++) for(int x = 0;x<cam.width;x++){
			px::Pixel p = a.Get(x, y), q = b.Get(x, y);
			if(p.r != q.r || p.g != q.g || p.b != q.b) n++;
		}
		return n;
	};
	check(differ(empty, nothing) == 0, "before its draw-in starts, nothing is drawn");
	check(differ(done, solid) == 0, "when it's done, it's exactly the plain solid cube");
	check(differ(half, nothing) > 50 && differ(half, solid) > 50, "half way (lines tracing, faces not yet in) it's neither");

	parse_result ok = parse_scene("c = cube draw_in 1s-3s\nb = sphere draw_in\n");
	parse_result flat = parse_scene("r = circle draw_in\n");
	check(ok.ok() && ok.spec.objects[0].draw_start == 1.0f && ok.spec.objects[0].draw_end == 3.0f && ok.spec.objects[1].draw_end == 2.0f
	      && !flat.ok(), "'draw_in 1s-3s' and 'draw_in' (0 s to 2 s) are read; a flat shape uses create instead");
}

int main(){
	test_clipping();
	test_fly_camera();
	test_fonts();
	test_math();
	test_labels();
	test_text();
	test_lines();
	test_fades();
	test_easing();
	test_looping();
	test_scene_language();
	test_live_reload();
	test_hd();
	test_linear_light();
	test_smooth_shading();
	test_vector_paths();
	test_write();
	test_transform();
	test_flat_shapes();
	test_create();
	test_graphs();
	test_morph();
	test_draw_in();
	std::printf("\n%s: %d check%s failed\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
	return failures == 0 ? 0 : 1;
}
