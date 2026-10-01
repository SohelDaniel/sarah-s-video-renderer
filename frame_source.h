#pragma once
#include "camera.h"
#include "object.h"
#include "render.h"

#include <vector>

// Where the player gets what to draw (docs/22). An interface: the player
// only needs a camera, the objects and how long the video is, and doesn't
// care where they come from.
//   fixed_source : a scene built once in C++ (scenes 1-5)
//   live_scene   : a .dan file, read again whenever it changes (live_scene.h)
class frame_source{
public:
	virtual ~frame_source() = default;
	virtual camera& cam() = 0;
	virtual const std::vector<object*>& objects() = 0;
	virtual float seconds() = 0;
	// called once per frame, before anything is drawn: a chance to change
	// what's in the scene (live_scene checks its file here)
	virtual void poll(){}
	// called after the objects are drawn: arrows and other things that
	// aren't objects (docs/26)
	virtual void draw_overlays(render& /*renderer*/,float /*t*/){}
};

// The simple case: a camera and objects that someone else owns.
class fixed_source : public frame_source{
public:
	fixed_source(camera& c,const std::vector<object*>& scene,float length)
		: c(c), scene(scene), length(length){}
	camera& cam() override{ return c; }
	const std::vector<object*>& objects() override{ return scene; }
	float seconds() override{ return length; }

private:
	camera& c;
	std::vector<object*> scene;
	float length;
};
