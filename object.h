#pragma once
#include "mat4.h"
#include "mesh.h"
#include "pixel.h"
#include "render.h"
#include "vec3.h"

// One thing in the scene: which mesh it uses, where it is, how it's turned,
// how big it is, and its color. It knows nothing about the camera or the
// image; those belong to the scene (one camera, one render per picture).
//
// The mesh is NOT copied, the object just points at it. So you can load
// cube.obj once and make 50 cube objects from it. The mesh has to stay alive
// as long as the objects using it.
class object{
public:
	object(const mesh& shape,px::Pixel color);

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

	vec3 get_position()const;
	// translate * rotate * scale: scale first, then turn, then move
	mat4<float> model_matrix()const;
	void draw(render& renderer)const;

private:
	const mesh* shape;
	px::Pixel color;
	vec3 position{0.0f, 0.0f, 0.0f};
	mat4<float> orientation = mat4<float>::identity();
	float size = 1.0f;
};
