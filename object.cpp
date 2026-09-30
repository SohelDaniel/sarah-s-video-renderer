#include "object.h"
#include "transform.h"


// Swing a pose around the point `around`, by `amount` (0..1) of the full turn.
// Shared by the right-away and the over-time rotate_around.
static void orbit(pose& p,vec3 around,float rot_y,float rot_x,float amount){
	float turn_y = rot_y * amount;
	float turn_x = rot_x * amount;
	mat4<float> turn = rotate_y(turn_y) * rotate_x(turn_x);

	// translate(around) * turn * translate(-around), done on the position:
	// shift so `around` is the origin, turn, shift back
	p.position = around + transform_point(turn, p.position - around);

	// the object itself turns by the same amount, so the same side keeps
	// facing the point it swings around (like the moon). Adding the angles
	// is exact for a turn around y (the usual orbit). With an x part too it's
	// only close, because two rotations don't simply add up.
	p.rot_y += turn_y;
	p.rot_x += turn_x;
}


object::object(const mesh& shape,px::Pixel color)
	: shape(&shape),color(color){}

// ---- right away: change the starting pose ----

void object::move(vec3 to){
	motion.initial.position = to;
	now = motion.initial;
}

void object::rotate(float rot_y,float rot_x){
	motion.initial.rot_y = rot_y;
	motion.initial.rot_x = rot_x;
	now = motion.initial;
}

void object::rotate_around(vec3 around,float rot_y,float rot_x){
	orbit(motion.initial, around, rot_y, rot_x, 1.0f);
	now = motion.initial;
}

void object::scale(float scale_by){
	motion.initial.size = scale_by;
	now = motion.initial;
}

// ---- over time: schedule a change; f is how far through it we are (0..1) ----

void object::move(vec3 to,float start,float end){
	motion.add({start, end}, [to](pose& p,float f){
		p.position = lerp(p.position, to, f);
	});
}

void object::rotate(float rot_y,float rot_x,float start,float end){
	motion.add({start, end}, [rot_y, rot_x](pose& p,float f){
		p.rot_y = lerp(p.rot_y, rot_y, f);
		p.rot_x = lerp(p.rot_x, rot_x, f);
	});
}

void object::rotate_around(vec3 around,float rot_y,float rot_x,float start,float end){
	motion.add({start, end}, [around, rot_y, rot_x](pose& p,float f){
		orbit(p, around, rot_y, rot_x, f);
	});
}

void object::scale(float scale_by,float start,float end){
	motion.add({start, end}, [scale_by](pose& p,float f){
		p.size = lerp(p.size, scale_by, f);
	});
}

void object::update(float t){
	now = motion.at(t);
}

vec3 object::get_position()const{
	return now.position;
}

mat4<float> object::model_matrix()const{
	// ::scale is the matrix from transform.h, not this class's scale()
	return translate(now.position[0], now.position[1], now.position[2])
	     * rotate_y(now.rot_y) * rotate_x(now.rot_x)
	     * ::scale(now.size, now.size, now.size);
}

void object::draw(render& renderer)const{
	renderer.draw_mesh(*shape, model_matrix(), color);
	if(bounds_on){
		renderer.draw_bounds(now.position, shape->bounding_radius() * now.size, bounds_color);
	}
}

void object::show_bounds(px::Pixel circle_color){
	bounds_on = true;
	bounds_color = circle_color;
}
