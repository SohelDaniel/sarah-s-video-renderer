#include "player.h"
#include "fly_camera.h"
#include "render.h"
#include "window.h"

#include <chrono>
#include <cstdio>
#include <iostream>
#include <stdexcept>


player::player(float seconds)
	: seconds(seconds){}

void player::play(camera& cam,const std::vector<object*>& scene){
	fixed_source source(cam, scene, seconds);
	play(source);
}

void player::play(frame_source& source){
	if(!record_to.empty()){
		record(source, record_to);
		return;
	}
	window screen("sarah's video player", source.cam().width, source.cam().height);
	render renderer(source.cam().width, source.cam().height, samples);  // made once, reused every frame
	renderer.clipping = clipping;
	renderer.show_circles = source.cam().circles;

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
		source.draw_overlays(renderer, t);   // arrows (docs/26)
		renderer.finish();

		// 4. on screen
		screen.show(renderer.picture());
		frames++;
	}

	std::cout << "played " << frames << " frames in " << real << " s ("
	          << frames / real << " frames per second)\n";
}

// A file name inside single quotes, for the shell: ' itself becomes '\''
// (end the quotes, an escaped quote, start them again). The repo's own
// folder has a ' in its name, so this matters.
static std::string shell_quote(const std::string& text){
	std::string out = "'";
	for(char c : text){
		if(c == '\'') out += "'\\''";
		else out += c;
	}
	return out + "'";
}

void player::record(frame_source& source,const std::string& filename){
	camera& first = source.cam();
	int width = first.width, height = first.height;
	render renderer(width, height, samples);
	renderer.show_circles = first.circles;

	// ffmpeg reads raw pictures from its input ("-i -"): 4 bytes per pixel,
	// r g b a, width x height each, and turns them into an H.264 video.
	// yuv420p is the color format every video player understands.
	std::string command = "ffmpeg -y -loglevel error -f rawvideo -pix_fmt rgba -s "
	                    + std::to_string(width) + "x" + std::to_string(height)
	                    + " -framerate " + std::to_string(frames_per_second)
	                    + " -i - -c:v libx264 -pix_fmt yuv420p -crf 18 " + shell_quote(filename);
	FILE* pipe = popen(command.c_str(), "w");
	if(!pipe) throw std::runtime_error("could not start ffmpeg (install it with: brew install ffmpeg)");

	// FIXED steps of time, not the real clock: frame k shows the moment
	// k / 60 s, however long it took to draw. So nothing is skipped and the
	// video plays at exactly the right speed (docs/25).
	int frames = int(std::ceil(source.seconds() * frames_per_second));
	std::cout << "recording " << frames << " frames (" << source.seconds() << " s at "
	          << frames_per_second << " fps) to " << filename << std::endl;
	for(int k = 0;k<frames;k++){
		float t = float(k) / float(frames_per_second);
		camera& cam = source.cam();
		cam.update(t);
		for(object* o : source.objects()) o->update(t);
		renderer.begin(cam);
		for(const object* o : source.objects()) o->draw(renderer);
		source.draw_overlays(renderer, t);
		renderer.finish();
		const px::Image& picture = renderer.picture();
		fwrite(picture.Data(), sizeof(px::Pixel), size_t(width) * size_t(height), pipe);
	}
	int status = pclose(pipe);
	if(status != 0) throw std::runtime_error("ffmpeg failed (exit " + std::to_string(status) + "): is it installed? brew install ffmpeg");
	std::cout << "wrote " << filename << std::endl;
}

void player::save_still(camera& cam,const std::vector<object*>& scene,float t,const std::string& filename){
	fixed_source source(cam, scene, seconds);
	save_still(source, t, filename);
}

void player::save_still(frame_source& source,float t,const std::string& filename){
	camera& cam = source.cam();
	render renderer(cam.width, cam.height, samples);
	renderer.clipping = clipping;
	renderer.show_circles = cam.circles;
	cam.update(t);
	for(object* o : source.objects()){
		o->update(t);
	}
	renderer.begin(cam);
	for(const object* o : source.objects()){
		o->draw(renderer);
	}
	source.draw_overlays(renderer, t);
	renderer.finish();
	if(!renderer.save(filename)){
		throw std::runtime_error("could not write " + filename);
	}
	std::cout << "wrote " << filename << "\n";
}
