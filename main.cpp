#include "camera.h"
#include "mesh.h"
#include "object.h"
#include "fly_camera.h"
#include "player.h"
#include "render.h"
#include "live_scene.h"
#include "scene_parser.h"
#include "scene_spec.h"
#include "test_scenes.h"
#include "world.h"

#include <iostream>
#include <exception>
#include <string>
#include <vector>

// usage: ./main [1|2|3] [solver step] [picture.png]
//   ./main    or  ./main 1   the old 3-picture scene, as a 12 second video
//   ./main 2                 a little solar system, 20 seconds
//   ./main 3 framed           a "lazy AI" scene, placed by the layout solver
//                             (steps: naive, greedy, refined, framed)
//   ./main 3 framed out.png   same, but save one picture instead of playing
//   ./main 4 naive            a "lazy AI" animation: orbits and a fly-by (docs/16)
//   ./main 4 naive x.png 7.5  same, but save the picture at 7.5 seconds
//   ./main 5 framed           things that collide on purpose (docs/18)
//   ./main clip on|off        standing in a scene, with/without near-plane clipping (docs/19)
//   ./main walk prefix        pictures of walking around scene 3 with pretend keys (docs/20)
//   ./main ease x.png         every easing curve as a row of snapshots (docs/23)
//   ./main scenes/x.dan       a scene written in the dan language (docs/21); edit and
//                             save the file while it plays and it reloads (docs/22)
//   ./main scenes/x.dan framed x.png 7   ... saved as a picture at 7 seconds
//   In the window: Tab = free camera (WASD + mouse, Space/Shift up/down,
//   Ctrl faster), Esc = back to the scripted camera.
//   ./main stress crowd                 one of the stress test scenes (docs/15)
//   ./main stress crowd greedy x.png    ... after one solver step, saved as a picture
//                                       (names: see test_scenes.cpp or `make test`)

// Scene 1: the same scene as the old 3 pictures, but now as a video: first we say how
// everything starts, then what changes and WHEN (from second a to second b).
// Nothing moves while main() runs; the player plays it all back afterwards.
void example_scene(){
	// Load each .obj ONCE. Objects only point at these, so two objects
	// using the same mesh don't load or copy it twice.
	mesh main_shape("shapes/cube.obj");
	mesh sphere("shapes/sphere.obj");
	mesh torus("shapes/torus.obj");

	// ================= How everything starts (second 0) =================

	// Object 1: in the middle, spun 45° on y and tipped 20° on x.
	object one(main_shape, px::Pixel(230, 130, 60));
	one.rotate(0.785f, 0.35f);

	// Object 2: moved to the left and a bit back, a bit smaller.
	object two(sphere, px::Pixel(80, 160, 230));
	two.move(vec3(-3.0f, 0.0f, -1.0f));
	two.scale(0.8f);

	// Object 3: starts on the right at (3,0,0), tipped on x, then swung 45°
	// around object 1 -> ends up at about (2.12, 0, -2.12), behind and right.
	object three(torus, px::Pixel(120, 200, 90));
	three.move(vec3(3.0f, 0.0f, 0.0f));
	three.rotate(0.0f, 1.0f);
	three.rotate_around(one.get_position(), 0.785f, 0.0f);

	// Everything the camera should film.
	std::vector<object*> scene = {&one, &two, &three};

	// One camera, looking at object 1.
	camera cam;
	cam.move(vec3(0.0f, 1.5f, 7.0f));
	cam.point_at(one.get_position());

	// ================= What happens, and when =================

	// seconds 2-5: the cube spins further on y
	one.rotate(1.6f, 0.35f, 2.0f, 5.0f);

	// seconds 3-6: the sphere moves up and toward the middle,
	// and grows back to normal size at the same time
	two.move(vec3(-1.8f, 1.4f, 0.5f), 3.0f, 6.0f);
	two.scale(1.0f, 3.0f, 6.0f);

	// seconds 7-10: the camera turns and flies over to look at object 3
	cam.point_at(three.get_position(), 7.0f, 10.0f);
	cam.move(three.get_position() + vec3(0.5f, 1.5f, 4.0f), 7.0f, 10.0f);

	// ================= Play it =================
	player video(12.0f);
	video.play(cam, scene);
}

// Scene 2: a sun, two planets orbiting it (one with a ring), a comet flying
// through, and a camera that swoops down low and then rises up overhead.
// Shows off the things scene 1 doesn't: orbits over time, one object doing
// several moves in a row, and different changes overlapping.
void solar_system(){
	mesh sphere("shapes/sphere.obj");
	mesh torus("shapes/torus.obj");
	mesh icosahedron("shapes/icosahedron.obj");
	mesh octahedron("shapes/octahedron.obj");
	mesh pyramid("shapes/pyramid.obj");

	// ================= How everything starts (second 0) =================

	// The sun: in the middle.
	object sun(sphere, px::Pixel(250, 200, 60));
	sun.scale(1.1f);

	// Planet one: close in, small and blue.
	object planet_one(icosahedron, px::Pixel(80, 160, 230));
	planet_one.move(vec3(5.0f, 0.0f, 0.0f));
	planet_one.scale(0.5f);

	// Planet two: further out, red, on the other side.
	object planet_two(octahedron, px::Pixel(220, 80, 70));
	planet_two.move(vec3(-7.0f, 0.0f, 0.0f));
	planet_two.scale(0.6f);

	// Planet two's ring, attached to planet two (docs/17): its position is
	// relative to the planet, so (0,0,0) = right around it, wherever the
	// planet goes. Tipped a little, since the torus lies flat.
	object ring(torus, px::Pixel(200, 120, 220));
	ring.attach_to(&planet_two);
	ring.scale(1.7f);
	ring.rotate(0.0f, 0.4f);

	// The comet: starts far away, back and to the left.
	object comet(pyramid, px::Pixel(235, 235, 245));
	comet.move(vec3(-9.0f, 3.0f, -4.0f));
	comet.scale(0.4f);

	std::vector<object*> scene = {&sun, &planet_one, &planet_two, &ring, &comet};

	// The camera: up and back, looking at the sun.
	camera cam;
	cam.move(vec3(0.0f, 7.0f, 14.0f));
	cam.point_at(sun.get_position());

	// ================= What happens, and when =================

	const vec3 center = sun.get_position();

	// the whole 20 seconds: planet one goes round twice (2 x 6.283 radians),
	// planet two goes round once the other way (negative = other direction)
	planet_one.rotate_around(center, 12.566f, 0.0f, 0.0f, 20.0f);
	planet_two.rotate_around(center, -6.283f, 0.0f, 0.0f, 20.0f);

	// the sun breathes: grows for 10 seconds, then shrinks back.
	// The second scale starts from wherever the first one ended.
	sun.scale(1.35f, 0.0f, 10.0f);
	sun.scale(1.1f, 10.0f, 20.0f);

	// the comet flies two legs, and tumbles the whole way (seconds 2-15)
	comet.move(vec3(8.0f, -1.0f, 3.0f), 2.0f, 8.0f);   // leg 1
	comet.move(vec3(-3.0f, 4.0f, 6.0f), 9.0f, 15.0f);  // leg 2 starts where leg 1 ended
	comet.rotate(12.566f, 3.0f, 2.0f, 15.0f);

	// the camera: seconds 6-11 swoops down low to the side,
	// then seconds 12-17 rises up to look down from above
	cam.move(vec3(9.0f, 1.5f, 6.0f), 6.0f, 11.0f);
	cam.move(vec3(0.0f, 14.0f, 4.0f), 12.0f, 17.0f);

	// ================= Play it =================
	player video(20.0f);
	video.play(cam, scene);
}

// Near-plane clipping (docs/19): standing on a big floor, right next to a
// column. Half the floor and the near side of the column are behind the
// camera, so their triangles cross the near plane.
void clipping_demo(bool clip,const std::string& picture){
	mesh plane("shapes/plane.obj");
	mesh cube("shapes/cube.obj");
	mesh sphere("shapes/sphere.obj");

	object floor(plane, px::Pixel(90, 110, 140));
	floor.scale(30.0f);
	floor.move(vec3(0.0f, -1.0f, 0.0f));

	object column(cube, px::Pixel(230, 130, 60));   // right beside the camera
	column.move(vec3(1.0f, -0.2f, 3.2f));
	column.scale(0.8f);

	object ball(sphere, px::Pixel(80, 160, 230));
	ball.move(vec3(-2.0f, 0.0f, -6.0f));
	object block(cube, px::Pixel(120, 200, 90));
	block.move(vec3(3.0f, 0.0f, -10.0f));

	std::vector<object*> scene = {&floor, &column, &ball, &block};
	camera cam;
	cam.move(vec3(0.0f, 0.3f, 4.0f));
	cam.point_at(vec3(0.0f, 0.0f, -6.0f));

	player video(10.0f);
	video.clipping = clip;
	if(picture.empty()) video.play(cam, scene);
	else                video.save_still(cam, scene, 0.0f, picture);
}

// Easing (docs/23), as one picture: each row is a ball going left to right
// with a different rate, photographed at 11 equal moments (t = 0, 0.1, ..
// 1). Where the snapshots bunch up it's slow; where they spread out, fast.
//   rows, top to bottom: linear, smooth, sine, rush_into, rush_from, there_and_back
void easing_demo(const std::string& picture){
	mesh sphere("shapes/sphere.obj");
	const rate rates[6] = {rate::linear, rate::smooth, rate::sine, rate::rush_into, rate::rush_from, rate::there_and_back};
	const px::Pixel colors[6] = {px::Pixel(200, 200, 210), px::Pixel(80, 160, 230), px::Pixel(120, 200, 90),
	                             px::Pixel(240, 220, 80), px::Pixel(230, 130, 60), px::Pixel(200, 120, 220)};
	std::vector<object> balls;
	balls.reserve(66);
	for(int row = 0;row<6;row++){
		for(int k = 0;k<=10;k++){
			float f = shape(rates[row], k / 10.0f);
			object ball(sphere, colors[row]);
			ball.scale(0.28f);
			ball.move(vec3(-6.0f + 12.0f * f, 3.75f - 1.5f * row, 0.0f));
			balls.push_back(ball);
		}
	}
	camera cam;
	cam.move(vec3(0.0f, 0.0f, 13.0f));
	cam.point_at(vec3(0.0f, 0.0f, 0.0f));
	cam.update(0.0f);
	render r(cam.width, cam.height);
	r.begin(cam);
	for(const object& b : balls) b.draw(r);
	r.finish();
	r.save(picture);
	std::cout << "wrote " << picture << "\n";
}

// The free camera (docs/20), driven by pretend key presses instead of a real
// keyboard, so the docs can show what walking looks like. Start at the
// solved camera of scene 3; walk forward while going down (W + Shift); then
// turn right while stepping left (mouse + A), which circles round the
// scene like walking round a statue.
void walk_demo(const std::string& prefix){
	world w(lazy_ai_scene(), layout::method::framed);
	std::vector<object*> scene = w.scene();
	for(object* o : scene) o->update(0.0f);

	fly_camera fly;
	fly.look_from(w.cam.eye(), w.cam.target());

	controls forward_down;
	forward_down.forward = true;
	forward_down.down = true;
	controls circle;
	circle.left = true;
	circle.turn_x = 30.0f;   // 30 pixels of mouse per step, 10 steps: 300 pixels

	for(int k = 0;k<3;k++){
		if(k == 1) fly.step(forward_down, 1.0f);
		if(k == 2) for(int s = 0;s<10;s++) fly.step(circle, 0.15f);
		w.cam.set_view(fly.eye, fly.target());

		render picture(w.cam.width, w.cam.height);
		picture.begin(w.cam);
		for(const object* o : scene) o->draw(picture);
		picture.finish();
		std::string file = prefix + std::to_string(k + 1) + ".png";
		picture.save(file);
		std::cout << "wrote " << file << " (eye at " << fly.eye[0] << ", " << fly.eye[1] << ", " << fly.eye[2]
		          << ", yaw " << fly.yaw * 180.0f / 3.14159265f << " deg, pitch "
		          << fly.pitch * 180.0f / 3.14159265f << " deg)\n";
	}
}

// Solve a described scene, print the report, then play it (still objects
// slowly spin in place, moving ones follow their paths) or save the one
// picture at time `t`. Scene 3 and 4 live in test_scenes.cpp.
void solved_scene(const scene_spec& spec,layout::method still_how,motion_plan::method moving_how,
                  const std::string& picture,float t){
	world w(spec, still_how, moving_how);
	std::cout << w.report();

	for(object* o : w.still_objects()){
		o->rotate(0.6f + 6.283f, 0.3f, 0.0f, 20.0f);   // one full spin over 20 s
	}

	player video(w.duration());
	if(picture.empty()) video.play(w.cam, w.scene());
	else                video.save_still(w.cam, w.scene(), t, picture);
}

layout::method still_step(const std::string& step){
	if(step == "naive")   return layout::method::naive;
	if(step == "greedy")  return layout::method::greedy;
	if(step == "refined") return layout::method::refined;
	if(step == "framed")  return layout::method::framed;
	throw std::invalid_argument("unknown solver step \"" + step + "\" (try: naive, greedy, refined, framed)");
}

motion_plan::method motion_step(const std::string& step){
	if(step == "naive")  return motion_plan::method::naive;
	if(step == "orbits")  return motion_plan::method::orbits;
	if(step == "flights") return motion_plan::method::flights;
	if(step == "framed")  return motion_plan::method::framed;
	throw std::invalid_argument("unknown motion step \"" + step + "\" (try: naive, orbits, flights, framed)");
}

int main(int argc,char** argv){
	std::string which = argc > 1 ? argv[1] : "1";
	try {
		// a .dan file (docs/21). Played live: saving the file again reloads
		// it, and every load writes <file>.report for the AI (docs/22).
		if(which.size() > 4 && which.substr(which.size() - 4) == ".dan"){
			live_scene live(which, motion_step(argc > 2 ? argv[2] : "framed"));
			std::cout << live.last_report();
			std::string picture = argc > 3 ? argv[3] : "";
			if(!picture.empty()){
				if(!live.has_scene()) return 1;
				player stills(live.seconds());
				stills.save_still(live.cam(), live.objects(), argc > 4 ? std::stof(argv[4]) : 0.0f, picture);
				return 0;
			}
			player video(live.seconds());
			video.play(live);
			return 0;
		}
		if(which == "1")      example_scene();
		else if(which == "2") solar_system();
		else if(which == "3"){
			solved_scene(lazy_ai_scene(), still_step(argc > 2 ? argv[2] : "framed"), motion_plan::method::naive,
			             argc > 3 ? argv[3] : "", 0.0f);
		}
		else if(which == "ease"){
			easing_demo(argc > 2 ? argv[2] : "easing.png");
		}
		else if(which == "walk"){
			walk_demo(argc > 2 ? argv[2] : "walk-");
		}
		else if(which == "clip"){
			clipping_demo(argc > 2 ? std::string(argv[2]) != "off" : true, argc > 3 ? argv[3] : "");
		}
		else if(which == "5"){
			solved_scene(impact_scene(), layout::method::framed, motion_step(argc > 2 ? argv[2] : "framed"),
			             argc > 3 ? argv[3] : "", argc > 4 ? std::stof(argv[4]) : 0.0f);
		}
		else if(which == "4"){
			solved_scene(lazy_motion_scene(), layout::method::framed, motion_step(argc > 2 ? argv[2] : "framed"),
			             argc > 3 ? argv[3] : "", argc > 4 ? std::stof(argv[4]) : 0.0f);
		}
		else if(which == "stress" && argc > 2){
			bool found = false;
			for(const test_scene& t : all_test_scenes()){
				if(t.name != argv[2]) continue;
				found = true;
				std::cout << t.name << ": " << t.attacks << "\n";
				solved_scene(t.spec, still_step(argc > 3 ? argv[3] : "framed"), motion_plan::method::framed,
				             argc > 4 ? argv[4] : "", 0.0f);
			}
			if(!found) throw std::invalid_argument(std::string("no test scene called \"") + argv[2] + "\"");
		}
		else {
			std::cerr << "usage: ./main [1|2|3] [solver step] [picture.png]\n"
			             "       ./main 4 [motion step] [picture.png] [time]\n"
			             "       ./main stress <scene> [solver step] [picture.png]\n";
			return 1;
		}
	} catch (const std::exception& e) {
		std::cerr << "error: " << e.what() << "\n";
		return 1;
	}
	return 0;
}
