#pragma once
#include "camera.h"
#include "frame_source.h"
#include "object.h"

#include <string>
#include <vector>

// Plays a scene live in a window for a set number of seconds.
//
// Every frame it does the same four things:
//   1. check the clock: how many seconds since we started?  -> t
//   2. move the camera and every object to where they are at time t
//      (or, in free mode, move the camera with the keys and mouse, docs/20)
//   3. draw a fresh picture with our own rasterizer
//   4. show it in the window
// Then again, about 60 times a second, until the window is closed. The
// video loops: after the last second it starts again from 0 (docs/20).
//
// Keys: Tab = free camera on/off, WASD = move, mouse = look, Space/Shift =
// up/down, Ctrl = faster, Esc = back to the scripted camera.
class player{
public:
	// how long the video lasts, in seconds
	explicit player(float seconds);

	// cut triangles at the near plane (docs/19); off only to show why it matters
	bool clipping = true;
	// start again from 0 when the video ends, until the window is closed
	bool loop = true;

	void play(camera& cam,const std::vector<object*>& scene);
	// Play from any source of frames, e.g. a .dan file that reloads itself
	// when it changes (docs/22). The video's length comes from the source.
	void play(frame_source& source);

	// Instead of playing: draw the single frame at time t and save it as a
	// PNG. Used for the pictures in docs/.
	void save_still(camera& cam,const std::vector<object*>& scene,float t,const std::string& filename);

private:
	float seconds;
};
