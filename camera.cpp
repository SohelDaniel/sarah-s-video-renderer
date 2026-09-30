#include "camera.h"
#include "object.h"
#include "render.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>


// ---- right away: change the starting viewpoint ----

void camera::move(vec3 eye){
	path.initial.eye = eye;
	now = path.initial;
}

void camera::point_at(vec3 target){
	path.initial.target = target;
	now = path.initial;
}

// ---- over time: schedule a change; f is how far through it we are (0..1) ----

void camera::move(vec3 eye,float start,float end){
	path.add({start, end}, [eye](viewpoint& v,float f){
		v.eye = lerp(v.eye, eye, f);
	});
}

void camera::point_at(vec3 target,float start,float end){
	path.add({start, end}, [target](viewpoint& v,float f){
		v.target = lerp(v.target, target, f);
	});
}

void camera::update(float t){
	now = path.at(t);
}

void camera::take(const std::vector<const object*>& scene){
	// fresh image + depth buffer every picture, then draw everything into it
	render renderer(width, height);
	renderer.begin(*this);
	for(const object* o : scene){
		o->draw(renderer);
	}
	renderer.finish();

	std::filesystem::create_directories(folder);
	std::string filename = folder + "/shot_" + std::to_string(++shots_taken) + ".png";
	if(!renderer.save(filename)){
		throw std::runtime_error("could not write " + filename);
	}
	std::cout << "wrote " << filename << "\n";
}
