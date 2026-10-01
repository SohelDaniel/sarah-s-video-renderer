#include "fly_camera.h"

#include <algorithm>
#include <cmath>


// Yaw and pitch -> a direction. Start looking along -z; tip up by pitch
// (that sets y, and shrinks the flat part to cos(pitch)); then turn by yaw
// around the vertical axis:
//   forward = ( cos(pitch)·sin(yaw),  sin(pitch),  -cos(pitch)·cos(yaw) )
vec3 fly_camera::forward()const{
	return vec3(std::cos(pitch) * std::sin(yaw), std::sin(pitch), -std::cos(pitch) * std::cos(yaw));
}

vec3 fly_camera::target()const{
	return eye + forward();
}

// The other way round: a direction -> yaw and pitch.
//   y = sin(pitch)                   ->  pitch = asin(y)
//   x / -z = sin(yaw) / cos(yaw)     ->  yaw = atan2(x, -z)
void fly_camera::look_from(const vec3& from,const vec3& to){
	eye = from;
	vec3 d = normalize(to - from);
	pitch = std::clamp(std::asin(std::clamp(d[1], -1.0f, 1.0f)), -max_pitch, max_pitch);
	yaw   = std::atan2(d[0], -d[2]);
}

void fly_camera::step(const controls& c,float dt){
	// 1. turn: the mouse moving right turns right, moving down looks down
	yaw   += c.turn_x * sensitivity;
	pitch -= c.turn_y * sensitivity;
	pitch  = std::clamp(pitch, -max_pitch, max_pitch);

	// 2. move. Like in Minecraft, W/S/A/D stay level (looking down and pressing
	// W doesn't dig you into the floor): use the flat versions of the
	// directions, with y = 0.
	//   flat forward = ( sin(yaw), 0, -cos(yaw) )
	//   right        = cross(flat forward, up) = ( cos(yaw), 0, sin(yaw) )
	vec3 ahead(std::sin(yaw), 0.0f, -std::cos(yaw));
	vec3 side(std::cos(yaw), 0.0f, std::sin(yaw));
	vec3 up(0.0f, 1.0f, 0.0f);

	vec3 move(0.0f, 0.0f, 0.0f);
	if(c.forward) move = move + ahead;
	if(c.back)    move = move - ahead;
	if(c.right)   move = move + side;
	if(c.left)    move = move - side;
	if(c.up)      move = move + up;
	if(c.down)    move = move - up;

	// W + D together would otherwise be √2 ≈ 1.41 times faster: walk the
	// same speed in every direction
	move = normalize(move);   // (normalize leaves (0,0,0) alone)

	float how_fast = speed * (c.fast ? fast_factor : 1.0f);
	eye = eye + move * (how_fast * dt);   // real seconds, so the same speed at any frame rate
}
