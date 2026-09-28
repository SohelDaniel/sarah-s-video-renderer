#pragma once
#include <stdexcept>

struct vec3{
	float a = 0.0f;
	float b = 0.0f;
	float c = 0.0f;

	vec3() = default;

	vec3(float x,float y,float z): a(x), b(y), c(z){}
	float& operator[](int i){
		if(i == 0)return a;
		else if(i == 1)return b;
		else if(i == 2)return c;
		else throw std::out_of_range("vec3 index must be 0,1,2");
	}
	const float& operator[](int i)const{
		if(i == 0)return a;
		else if(i == 1)return b;
		else if(i == 2)return c;
		else throw std::out_of_range("vec3 index must be 0,1,2");
	}
	// 2D cross product of the x,y parts: the signed area (x2) of the
	// parallelogram spanned by v and w. Positive if w is counter-clockwise from v.
	static float det(vec3 v,vec3 w);
};

vec3 operator+(const vec3& v,const vec3& w);
vec3 operator-(const vec3& v,const vec3& w);
vec3 operator*(const vec3& v,float s);
float dot(const vec3& v,const vec3& w);
vec3 cross(const vec3& v,const vec3& w);
vec3 normalize(const vec3& v);
