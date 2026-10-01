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
	float opacity = 1.0f; // 1 = solid, 0 = invisible (docs/24)
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
	// a flat shape (docs/40): unit size, its plane is its own x and y
	object(const flat_shape& flat,px::Pixel color);
	bool is_flat()const{ return flat != nullptr; }
	// Create (docs/41): how much of a flat shape is drawn
	void set_flat_look(flat_look look);

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
	// The last argument says how it eases (docs/23): linear = steady.
	void move(vec3 to,float start,float end,rate how = rate::linear);
	void rotate(float rot_y,float rot_x,float start,float end,rate how = rate::linear);
	void rotate_around(vec3 around,float rot_y,float rot_x,float start,float end,rate how = rate::linear);
	void scale(float scale_by,float start,float end,rate how = rate::linear);
	// How solid it is (docs/24): 1 = solid, 0 = invisible.
	void set_opacity(float opacity);
	void fade(float to,float start,float end,rate how = rate::linear);

	// Work out where the object is at time t (seconds). Call once per frame.
	void update(float t);

	// ---- parent/child (docs/17): follow another object around ----
	// Only the position is inherited, not the spin: a moon's orbit shouldn't
	// swing round every time its planet turns.

	// From the start: this object's position is RELATIVE to the parent's,
	// so (0,0,0) means "right where the parent is".
	void attach_to(const object* parent);
	// Moves on its own until `time`; from then on it keeps the same offset
	// from the parent and rides along with it (something that hit it and stuck).
	void stick_to(const object* parent,float time);

	// Where it really is, after following its parent (at the time of the
	// last update)
	vec3 get_position()const;
	// The radius of its bounding sphere right now (mesh radius x size)
	float bounding_radius()const;
	float opacity()const;
	// ... and at any time t, without changing anything
	vec3 position_at(float t)const;
	// translate * rotate * scale: scale first, then turn, then move
	mat4<float> model_matrix()const;
	void draw(render& renderer)const;
	// Also draw the object's bounding sphere as a circle (for the layout
	// solver's pictures). Grey = fine, red = overlapping something in that frame.
	void show_bounds();

private:
	const mesh* shape = nullptr;
	const flat_shape* flat = nullptr;  // drawn instead of a mesh if set
	flat_look look;
	px::Pixel color;
	timeline<pose> motion;  // how it starts + every change scheduled on it
	pose now;               // where it is at the current time (its own motion)
	float now_time = 0.0f;  // the t of the last update

	enum class link{ none, attached, stuck };
	link follows = link::none;
	const object* parent = nullptr;
	float stick_time = 0.0f;
	bool bounds_on = false;
};
