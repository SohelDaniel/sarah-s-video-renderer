#include "camera.h"
#include "mesh.h"
#include "object.h"
#include "player.h"

#include <iostream>
#include <exception>
#include <string>
#include <vector>

// usage: ./main [1|2]
//   ./main    or  ./main 1   the old 3-picture scene, as a 12 second video
//   ./main 2                 a little solar system, 20 seconds

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

	// Planet two's ring: same place as planet two, tipped a little
	// (the torus lies flat to begin with).
	object ring(torus, px::Pixel(200, 120, 220));
	ring.move(vec3(-7.0f, 0.0f, 0.0f));
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
	// the ring gets the exact same orbit, so it stays around planet two.
	// (There's no "attach the ring to the planet" yet, see LIVE.md.)
	ring.rotate_around(center, -6.283f, 0.0f, 0.0f, 20.0f);

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

int main(int argc,char** argv){
	std::string which = argc > 1 ? argv[1] : "1";
	try {
		if(which == "1")      example_scene();
		else if(which == "2") solar_system();
		else {
			std::cerr << "usage: ./main [1|2]\n";
			return 1;
		}
	} catch (const std::exception& e) {
		std::cerr << "error: " << e.what() << "\n";
		return 1;
	}
	return 0;
}
