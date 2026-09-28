#include "object.h"
#include "transform.h"


object::object(const mesh& shape,px::Pixel color)
	: shape(&shape),color(color){}

void object::move(vec3 to){
	position = to;
}

void object::rotate(float rot_y,float rot_x){
	orientation = rotate_y(rot_y) * rotate_x(rot_x);
}

void object::rotate_around(vec3 around,float rot_y,float rot_x){
	mat4<float> turn = rotate_y(rot_y) * rotate_x(rot_x);

	// translate(around) * turn * translate(-around), done on the position:
	// shift so `around` is the origin, turn, shift back
	position = around + transform_point(turn, position - around);

	// the object itself turns by the same amount, so the same side
	// keeps facing the point it swings around (like the moon)
	orientation = turn * orientation;
}

void object::scale(float scale_by){
	size = scale_by;
}

vec3 object::get_position()const{
	return position;
}

mat4<float> object::model_matrix()const{
	// ::scale is the matrix from transform.h, not this class's scale()
	return translate(position[0], position[1], position[2]) * orientation * ::scale(size, size, size);
}

void object::draw(render& renderer)const{
	renderer.draw_mesh(*shape, model_matrix(), color);
}
