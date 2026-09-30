#include "player.h"
#include "render.h"
#include "window.h"

#include <chrono>
#include <iostream>
#include <stdexcept>


player::player(float seconds)
	: seconds(seconds){}

void player::play(camera& cam,const std::vector<object*>& scene){
	window screen("sarah's video player", cam.width, cam.height);
	render renderer(cam.width, cam.height);  // made once, reused every frame

	// steady_clock only ever goes forward (unlike the wall clock, which can
	// jump if the computer changes its time), so it's the one for timing
	using clock = std::chrono::steady_clock;
	const clock::time_point started = clock::now();
	float t = 0.0f;
	int frames = 0;

	while(screen.is_open()){
		// 1. seconds since we started playing. Using real time (not a frame
		// count) means a slow computer shows fewer frames of the same motion,
		// instead of playing in slow motion.
		t = std::chrono::duration<float>(clock::now() - started).count();
		if(t > seconds) break;

		// 2. everything goes to where it is at time t
		cam.update(t);
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

	std::cout << "played " << frames << " frames in " << t << " s ("
	          << frames / t << " frames per second)\n";
}

void player::save_still(camera& cam,const std::vector<object*>& scene,float t,const std::string& filename){
	render renderer(cam.width, cam.height);
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
