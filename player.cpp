#include "player.h"
#include "fly_camera.h"
#include "render.h"
#include "window.h"

#include <chrono>
#include <iostream>
#include <stdexcept>


player::player(float seconds)
	: seconds(seconds){}

void player::play(camera& cam,const std::vector<object*>& scene){
	fixed_source source(cam, scene, seconds);
	play(source);
}

void player::play(frame_source& source){
	window screen("sarah's video player", source.cam().width, source.cam().height);
	render renderer(source.cam().width, source.cam().height);  // made once, reused every frame
	renderer.clipping = clipping;

	// steady_clock only ever goes forward (unlike the wall clock, which can
	// jump if the computer changes its time), so it's the one for timing
	using clock = std::chrono::steady_clock;
	const clock::time_point started = clock::now();
	float real = 0.0f;       // real seconds since we started
	int frames = 0;

	fly_camera fly;          // the free camera (docs/20)
	bool free = false;
	std::cout << "click the window, then press Tab to walk around (WASD + mouse, Esc to stop)\n";
	float last = 0.0f;       // `real` of the previous frame, for real dt

	while(screen.is_open()){
		// a chance for the source to change the scene (a .dan file that was
		// saved again, docs/22). The camera and objects are asked for again
		// every frame, so a new scene simply takes over.
		source.poll();
		camera& cam = source.cam();
		const std::vector<object*>& scene = source.objects();
		float seconds = source.seconds();

		// 1. seconds since we started playing. Using real time (not a frame
		// count) means a slow computer shows fewer frames of the same motion,
		// instead of playing in slow motion.
		real = std::chrono::duration<float>(clock::now() - started).count();
		if(!loop && real > seconds) break;
		float dt = real - last;   // real seconds since the last frame: for walking
		last = real;
		// the scene's own time: it goes round and round when looping. The
		// timeline replays from the start every frame anyway (docs/09), so
		// jumping back to 0 needs nothing else at all.
		float t = loop ? loop_time(real, seconds) : real;

		// Tab: switch camera. Going free starts exactly where the scripted
		// camera is now; going back hands control back to the script.
		if(screen.toggled() || (free && screen.escaped())){
			free = !free;
			if(free) fly.look_from(cam.eye(), cam.target());
			else std::cout << "you walked to (" << fly.eye[0] << ", " << fly.eye[1] << ", " << fly.eye[2] << ")\n";
			screen.capture_mouse(free);
			std::cout << (free ? "free camera: on (WASD to move, mouse to look, Esc to stop)"
			                   : "free camera: off (back to the scripted camera)") << std::endl;
		}

		// 2. everything goes to where it is at time t. (The free camera is
		// separate from the scene, so you stay where you walked to even when
		// the scene is reloaded.)
		if(free){
			fly.step(screen.read_controls(), dt);
			cam.set_view(fly.eye, fly.target());
		}else{
			screen.read_controls();   // throw away mouse moves made meanwhile
			cam.update(t);
		}
		for(object* o : scene){
			o->update(t);
		}

		// 3. a fresh picture, drawn by our own rasterizer
		renderer.begin(cam);
		for(const object* o : scene){
			o->draw(renderer);
		}
		renderer.finish();

		// 4. on screen
		screen.show(renderer.picture());
		frames++;
	}

	std::cout << "played " << frames << " frames in " << real << " s ("
	          << frames / real << " frames per second)\n";
}

void player::save_still(camera& cam,const std::vector<object*>& scene,float t,const std::string& filename){
	render renderer(cam.width, cam.height);
	renderer.clipping = clipping;
	cam.update(t);
	for(object* o : scene){
		o->update(t);
	}
	renderer.begin(cam);
	for(const object* o : scene){
		o->draw(renderer);
	}
	renderer.finish();
	if(!renderer.save(filename)){
		throw std::runtime_error("could not write " + filename);
	}
	std::cout << "wrote " << filename << "\n";
}
