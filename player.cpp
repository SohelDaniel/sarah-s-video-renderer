#include "player.h"
#include "render.h"
#include "window.h"

#include <chrono>
#include <iostream>


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

		// 4. on screen
		screen.show(renderer.picture());
		frames++;
	}

	std::cout << "played " << frames << " frames in " << t << " s ("
	          << frames / t << " frames per second)\n";
}
