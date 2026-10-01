#pragma once
#include "mat4.h"
#include "timeline.h"
#include "transform.h"
#include "vec3.h"

#include <string>
#include <vector>

// Only a mention that `object` exists: render.h includes this file, so
// including object.h/render.h here would make them include each other.
// camera.cpp includes the real thing.
class object;

// Where the camera is and what it looks at, at one moment.
struct viewpoint{
	vec3 eye{0.0f, 0.0f, 4.0f};     // camera position
	vec3 target{0.0f, 0.0f, 0.0f};  // point it looks at
};

// Where we look from, how wide the lens is, and where the pictures go.
// Like an object, it can be moved right away (how it starts) or over time.
// The matrices are built in transform.h; the rest is in camera.cpp.
class camera{
public:
	// ---- the lens: stays the same the whole time ----
	vec3 up{0.0f, 1.0f, 0.0f};      // which way is "up" on screen

	float fov_y  = 50.0f * 3.14159265f / 180.0f; // vertical field of view, radians
	float z_near = 0.1f;   // anything closer than this is cut off
	float z_far  = 100.0f; // anything further than this is cut off

	int width  = 640;      // picture size in pixels
	int height = 480;
	std::string folder = "out";  // take() saves to folder/shot_1.png, shot_2.png, ...

	// ---- right away: sets how the camera starts (at second 0) ----
	void move(vec3 eye);
	void point_at(vec3 target);

	// ---- over time: flies there between second `start` and `end` ----
	void move(vec3 eye,float start,float end,rate how = rate::linear);
	void point_at(vec3 target,float start,float end,rate how = rate::linear);

	// Work out where the camera is at time t (seconds). Call once per frame.
	void update(float t);

	// Put the camera somewhere for this frame only, without touching its
	// scripted path (the free camera, docs/20, uses this; going back to the
	// script then carries on exactly where the script is).
	void set_view(vec3 eye,vec3 target);
	vec3 eye()const;
	vec3 target()const;

	mat4<float> view()const{
		return look_at(now.eye, now.target, up);
	}
	mat4<float> projection(float aspect)const{
		return perspective(fov_y, aspect, z_near, z_far);
	}

	// Take a picture of these objects from where the camera is now and save
	// it as the next numbered shot in `folder`. Throws if it can't be saved.
	void take(const std::vector<const object*>& scene);

private:
	timeline<viewpoint> path;  // how it starts + every change scheduled on it
	viewpoint now;             // where it is at the current time
	int shots_taken = 0;
};
