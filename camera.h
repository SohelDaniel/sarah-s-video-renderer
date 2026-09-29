#pragma once
#include "mat4.h"
#include "transform.h"
#include "vec3.h"

#include <string>
#include <vector>

// Only a mention that `object` exists: render.h includes this file, so
// including object.h/render.h here would make them include each other.
// camera.cpp includes the real thing.
class object;

// Where we look from, how wide the lens is, and where the pictures go.
// The matrices are built in transform.h; take() is in camera.cpp.
struct camera{
	vec3 eye{0.0f, 0.0f, 4.0f};     // camera position
	vec3 target{0.0f, 0.0f, 0.0f};  // point it looks at
	vec3 up{0.0f, 1.0f, 0.0f};      // which way is "up" on screen

	float fov_y  = 50.0f * 3.14159265f / 180.0f; // vertical field of view, radians
	float z_near = 0.1f;   // anything closer than this is cut off
	float z_far  = 100.0f; // anything further than this is cut off

	int width  = 640;      // picture size in pixels
	int height = 480;
	std::string folder = "out";  // pictures go to folder/shot_1.png, shot_2.png, ...

	mat4<float> view()const{
		return look_at(eye, target, up);
	}
	mat4<float> projection(float aspect)const{
		return perspective(fov_y, aspect, z_near, z_far);
	}

	// Take a picture of these objects from where the camera is now and save
	// it as the next numbered shot in `folder`. Throws if it can't be saved.
	void take(const std::vector<const object*>& scene);

private:
	int shots_taken = 0;
};
