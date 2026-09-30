#pragma once
#include "camera.h"
#include "object.h"

#include <vector>

// Plays a scene live in a window for a set number of seconds.
//
// Every frame it does the same four things:
//   1. check the clock: how many seconds since we started?  -> t
//   2. move the camera and every object to where they are at time t
//   3. draw a fresh picture with our own rasterizer
//   4. show it in the window
// Then again, about 60 times a second, until time is up or the window is closed.
class player{
public:
	// how long the video lasts, in seconds
	explicit player(float seconds);

	void play(camera& cam,const std::vector<object*>& scene);

private:
	float seconds;
};
