#include "camera.h"
#include "object.h"
#include "render.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>


void camera::take(const std::vector<const object*>& scene){
	// fresh image + depth buffer every picture, then draw everything into it
	render renderer(width, height, *this);
	for(const object* o : scene){
		o->draw(renderer);
	}

	std::filesystem::create_directories(folder);
	std::string filename = folder + "/shot_" + std::to_string(++shots_taken) + ".png";
	if(!renderer.save(filename)){
		throw std::runtime_error("could not write " + filename);
	}
	std::cout << "wrote " << filename << "\n";
}
