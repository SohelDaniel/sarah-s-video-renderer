#pragma once
#include <initializer_list>
#include "vec3.h"

// A point in homogeneous coordinates. w = 1 for a normal 3D point.
// After the projection matrix, w holds the depth we divide by.
template<typename T>
struct vec4{
	T x = 0;
	T y = 0;
	T z = 0;
	T w = 0;
};

// 4x4 matrix, stored row by row: m[r*4 + c].
// It multiplies column vectors, so  p' = M * p  and  (A * B) * p = A * (B * p),
// meaning A * B applies B first, then A.
template<typename T>
class mat4 {
public:
	mat4() : m{} {}
	// Values row by row, so the code looks like the matrix on paper.
	mat4(std::initializer_list<T> values) : m{} {
		int k = 0;
		for(T v : values){
			if(k == 16) break;
			m[k++] = v;
		}
	}
	static mat4 identity(){
		mat4 result;
		for(int k = 0;k<4;++k) result(k,k) = 1;
		return result;
	}

	T& operator()(int r,int c){ return m[r*4 + c]; }
	const T& operator()(int r,int c)const{ return m[r*4 + c]; }

	mat4 operator+(const mat4& other)const{
		mat4 result = *this;
		for(int k = 0;k<16;++k){
			result.m[k] += other.m[k];
		}
		return result;
	}
	mat4 operator-(const mat4& other)const{
		mat4 result = *this;
		for(int k = 0;k<16;++k){
			result.m[k] -= other.m[k];
		}
		return result;
	}

	mat4 operator*(const mat4& other) const {
    		mat4 result;
    		for (int r = 0; r < 4; ++r){
        		for (int c = 0; c < 4; ++c){
            			for (int k = 0; k < 4; ++k)
                			result.m[r*4 + c] += m[r*4 + k] * other.m[k*4 + c];
			}
		}
    		return result;
	}
	mat4 operator*(T s) const {
    		mat4 result = *this;
    		for (int k = 0; k < 16; ++k)
        		result.m[k] *= s;
    		return result;
	}
	vec4<T> operator*(const vec4<T>& v) const {
		const T in[4] = {v.x, v.y, v.z, v.w};
		T out[4] = {0, 0, 0, 0};
		for (int r = 0; r < 4; ++r)
			for (int k = 0; k < 4; ++k)
				out[r] += m[r*4 + k] * in[k];
		return vec4<T>{out[0], out[1], out[2], out[3]};
	}

private:
    T m[16];
};
template<typename T>
mat4<T> operator*(T s, const mat4<T>& A) {
    return A * s;
}
