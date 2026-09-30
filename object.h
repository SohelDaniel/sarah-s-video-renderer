#pragma once
#include "mat4.h"
#include "mesh.h"
#include "pixel.h"
#include "render.h"
#include "timeline.h"
#include "vec3.h"

// Where an object is, how it's turned and how big it is, at one moment.
struct pose{
	vec3 position{0.0f, 0.0f, 0.0f};
	float rot_y = 0.0f;  // spin around the vertical axis, radians
	float rot_x = 0.0f;  // tip forward/back, radians
	float size  = 1.0f;  // 1 = normal size, 2 = twice as big, 0.5 = half
};

// One thing in the scene: which mesh it uses, its color, and how it moves
// over time. It knows nothing about the camera or the image; those belong
// to the scene (one camera, one render per picture).
//
// The mesh is NOT copied, the object just points at it. So you can load
// cube.obj once and make 50 cube objects from it. The mesh has to stay alive
// as long as the objects using it.
class object{
public:
	object(const mesh& shape,px::Pixel color);

	// ---- right away: sets how the object starts (at second 0) ----

	// put it at a position (world units)
	void move(vec3 to);
	// spin around itself: sets how it's turned (radians)
	void rotate(float rot_y,float rot_x);
	//to roate around a point p  = (10,3,1);
	//translate(10,3,1) * rotate_y(3.14159) * translate (-10,-3,-1)
	//first shift everything so the orgin is at that point
	//then rotate around that point as the object always rotates around the orgin
	//then shift back
	void rotate_around(vec3 around,float rot_y,float rot_x);
	// 1 = normal size, 2 = twice as big, 0.5 = half
	void scale(float scale_by);

	// ---- over time: the same, but it happens between second `start` and `end` ----
	// move, rotate and scale go TO the value given.
	// rotate_around swings BY the angles given (a quarter orbit, a full orbit...).
	void move(vec3 to,float start,float end);
	void rotate(float rot_y,float rot_x,float start,float end);
	void rotate_around(vec3 around,float rot_y,float rot_x,float start,float end);
	void scale(float scale_by,float start,float end);

	// Work out where the object is at time t (seconds). Call once per frame.
	void update(float t);

	vec3 get_position()const;
	// translate * rotate * scale: scale first, then turn, then move
	mat4<float> model_matrix()const;
	void draw(render& renderer)const;
	// Also draw the object's bounding sphere as a circle (for the layout
	// solver's pictures). Grey = fine, red = overlapping something.
	void show_bounds(px::Pixel circle_color);

private:
	const mesh* shape;
	px::Pixel color;
	timeline<pose> motion;  // how it starts + every change scheduled on it
	pose now;               // where it is at the current time
	bool bounds_on = false;
	px::Pixel bounds_color;
};
