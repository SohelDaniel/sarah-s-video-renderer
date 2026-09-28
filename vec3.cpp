#include "vec3.h"
#include <cmath>


float vec3::det(vec3 v,vec3 w){
	return (v[0]*w[1]) - (v[1]*w[0]);
}

vec3 operator+(const vec3& v,const vec3& w){
	return vec3(v[0]+w[0], v[1]+w[1], v[2]+w[2]);
}
vec3 operator-(const vec3& v,const vec3& w){
	return vec3(v[0]-w[0], v[1]-w[1], v[2]-w[2]);
}
vec3 operator*(const vec3& v,float s){
	return vec3(v[0]*s, v[1]*s, v[2]*s);
}
float dot(const vec3& v,const vec3& w){
	return v[0]*w[0] + v[1]*w[1] + v[2]*w[2];
}
vec3 cross(const vec3& v,const vec3& w){
	return vec3(v[1]*w[2] - v[2]*w[1],
	            v[2]*w[0] - v[0]*w[2],
	            v[0]*w[1] - v[1]*w[0]);
}
vec3 normalize(const vec3& v){
	float len = std::sqrt(dot(v,v));
	if(len == 0.0f) return v;
	return v * (1.0f/len);
}
