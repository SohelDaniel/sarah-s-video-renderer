#pragma once
#include <cmath>
#include "mat4.h"
#include "vec3.h"

// ============================================================================
//  Every matrix the renderer uses, one function each.
//
//  The pipeline a vertex goes through:
//
//    model space --model--> world space --view--> camera space
//                --projection--> clip space --divide by w--> NDC
//                --viewport--> pixels
//
//  NDC ("normalized device coordinates") is the cube -1..1 on every axis:
//  anything inside it is on screen.
// ============================================================================


// Move a point by (x, y, z). The translation sits in the last column,
// so it gets multiplied by the point's w = 1 and added on.
inline mat4<float> translate(float x,float y,float z){
	return mat4<float>{
		1, 0, 0, x,
		0, 1, 0, y,
		0, 0, 1, z,
		0, 0, 0, 1,
	};
}

// Rotation around the x axis by `angle` radians (counter-clockwise when
// looking from +x towards the origin). x stays put, y and z turn in a circle:
//   y' = y*cos - z*sin
//   z' = y*sin + z*cos
inline mat4<float> rotate_x(float angle){
	float c = std::cos(angle);
	float s = std::sin(angle);
	return mat4<float>{
		1, 0,  0, 0,
		0, c, -s, 0,
		0, s,  c, 0,
		0, 0,  0, 1,
	};
}

// Rotation around the y axis. Same idea, now x and z turn:
//   x' =  x*cos + z*sin
//   z' = -x*sin + z*cos
inline mat4<float> rotate_y(float angle){
	float c = std::cos(angle);
	float s = std::sin(angle);
	return mat4<float>{
		 c, 0, s, 0,
		 0, 1, 0, 0,
		-s, 0, c, 0,
		 0, 0, 0, 1,
	};
}

// View matrix: moves the whole world so the camera ends up at the origin
// looking down -z (that's the convention the projection below expects).
//
// First build the camera's own three axes:
//   back   = direction from target to eye   (camera looks along -back)
//   right  = perpendicular to up and back
//   cam_up = perpendicular to back and right (the "real" up, in case the
//            given up vector wasn't exactly perpendicular)
//
// A point's coordinate along an axis is dot(axis, point - eye). Putting the
// three axes in the rows does all three dot products at once; the last
// column is the "- eye" part, pulled out: dot(axis, p - eye) = dot(axis, p) - dot(axis, eye).
inline mat4<float> look_at(const vec3& eye,const vec3& target,const vec3& up){
	vec3 back   = normalize(eye - target);
	vec3 right  = normalize(cross(up, back));
	vec3 cam_up = cross(back, right);
	return mat4<float>{
		right[0],  right[1],  right[2],  -dot(right, eye),
		cam_up[0], cam_up[1], cam_up[2], -dot(cam_up, eye),
		back[0],   back[1],   back[2],   -dot(back, eye),
		0,         0,         0,         1,
	};
}

// Perspective projection. Takes camera space to clip space.
//
// Why things far away look small: by similar triangles, a point at depth d
// in front of the camera lands on the screen at  x/d, y/d.  In camera space
// "in front" means negative z, so d = -z.
//
// A matrix can only multiply and add, it can't divide. So the matrix copies
// -z into w (the bottom row 0 0 -1 0), and the renderer divides x, y, z
// by w afterwards. That step is the "perspective divide".
//
// focal = 1 / tan(fov_y / 2): scales things so the edge of the vertical field
// of view lands exactly on y = +-1. x is also divided by aspect (width/height)
// so a square in the world stays square on a wide image.
//
// Row 3 keeps depth for the depth test. We want, after dividing by w = -z:
//   z = -z_near  ->  -1   (closest visible)
//   z = -z_far   ->  +1   (furthest visible)
// With z_ndc = (A*z + B) / -z, solving those two equations gives
//   A = (z_far + z_near) / (z_near - z_far)
//   B = 2 * z_far * z_near / (z_near - z_far)
inline mat4<float> perspective(float fov_y,float aspect,float z_near,float z_far){
	float focal = 1.0f / std::tan(fov_y / 2.0f);
	float A = (z_far + z_near) / (z_near - z_far);
	float B = 2.0f * z_far * z_near / (z_near - z_far);
	return mat4<float>{
		focal / aspect, 0,     0,  0,
		0,              focal, 0,  0,
		0,              0,     A,  B,
		0,              0,    -1,  0,
	};
}

// Viewport: NDC (-1..1) to pixel coordinates.
//   x: -1..1 -> 0..width         x_px = width/2  * x + width/2
//   y: -1..1 -> height..0        y_px = -height/2 * y + height/2
// y is flipped because in NDC y goes up but in the image y goes down.
// z is left alone; it's only used for the depth test.
inline mat4<float> viewport(int width,int height){
	float w = width  / 2.0f;
	float h = height / 2.0f;
	return mat4<float>{
		w,  0, 0, w,
		0, -h, 0, h,
		0,  0, 1, 0,
		0,  0, 0, 1,
	};
}

// Apply a matrix to a 3D point (w = 1). Only for matrices without
// perspective (model/view), where w stays 1 and no divide is needed.
inline vec3 transform_point(const mat4<float>& m,const vec3& p){
	vec4<float> r = m * vec4<float>{p[0], p[1], p[2], 1.0f};
	return vec3(r.x, r.y, r.z);
}
