#pragma once
#include "mat4.h"
#include "transform.h"
#include "vec3.h"

// Where we look from and how wide the lens is.
// The camera only produces matrices; see transform.h for how they're built.
struct camera{
	vec3 eye{0.0f, 0.0f, 4.0f};     // camera position
	vec3 target{0.0f, 0.0f, 0.0f};  // point it looks at
	vec3 up{0.0f, 1.0f, 0.0f};      // which way is "up" on screen

	float fov_y  = 50.0f * 3.14159265f / 180.0f; // vertical field of view, radians
	float z_near = 0.1f;   // anything closer than this is cut off
	float z_far  = 100.0f; // anything further than this is cut off

	mat4<float> view()const{
		return look_at(eye, target, up);
	}
	mat4<float> projection(float aspect)const{
		return perspective(fov_y, aspect, z_near, z_far);
	}
};
