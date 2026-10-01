#pragma once
#include "vec3.h"

// ============================================================================
//  Walking around with the keyboard and mouse (docs/20), like in Minecraft.
//
//  This is only the math: which way you're looking and where you are. The
//  window (window.cpp) turns real keys and mouse moves into `controls`, so
//  this file knows nothing about SDL and can be tested on its own.
// ============================================================================

// What the player is doing during one frame.
struct controls{
	bool forward = false, back = false;    // W, S
	bool left = false, right = false;      // A, D
	bool up = false, down = false;         // Space, Shift
	bool fast = false;                     // Ctrl: move faster
	float turn_x = 0.0f;                   // how far the mouse moved, in pixels
	float turn_y = 0.0f;                   //   (right and down are positive)
};

class fly_camera{
public:
	vec3 eye{0.0f, 0.0f, 0.0f};
	float yaw   = 0.0f;   // turn left/right, radians (0 = looking along -z)
	float pitch = 0.0f;   // look up/down, radians (kept between -89° and 89°)

	float speed       = 4.0f;     // world units per second
	float fast_factor = 3.0f;     // ... times this with Ctrl held
	float sensitivity = 0.0025f;  // radians per pixel of mouse movement

	// Start from wherever a camera is and whatever it's looking at.
	void look_from(const vec3& from,const vec3& target);
	// Turn with the mouse, then move with the keys, for one frame of dt seconds.
	void step(const controls& c,float dt);

	// The way you're looking (a unit vector), and a point to look at
	vec3 forward()const;
	vec3 target()const;

	// pitch never reaches straight up/down: look_at breaks there (docs/04)
	static constexpr float max_pitch = 89.0f * 3.14159265f / 180.0f;
};
