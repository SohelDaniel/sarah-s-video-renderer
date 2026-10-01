#pragma once
#include <stdexcept>
#include <string>
#include "vec3.h"
#include <vector>

struct triangle{
	int t[3] = {0,0,0};
	triangle(int a, int b, int c) : t{a, b, c} {}
	int& operator[](int i) {
	    if (i < 0 || i > 2) throw std::out_of_range("triangle index must be 0,1,2");
	    return t[i];
	}
	const int& operator[](int i) const {
    		if (i < 0 || i > 2) throw std::out_of_range("triangle index must be 0,1,2");
    		return t[i];
	}

};
class mesh{

public:
	explicit mesh(const std::string& file_name);
	int get_vertices_count()const;
	int get_faces_count()const;
	vec3 vertex(int i)const;
	triangle face(int i)const;
	// Distance from (0,0,0) to the farthest vertex: a sphere this big holds
	// the whole shape, however it's turned. Used by the layout solver.
	float bounding_radius()const;
	// The surface's direction at corner k (0, 1, 2) of face i, for smooth
	// shading (docs/36): the average of the faces around that vertex that
	// bend away from this face by less than the crease angle. Curved shapes
	// come out smooth; real corners (a cube's) stay sharp.
	vec3 corner_normal(int i,int k)const;
	static constexpr float crease_degrees = 40.0f;

private:
	std::string name;
	std::vector<vec3> vertices;
	std::vector<triangle> faces;
	float radius = 0.0f;
	std::vector<vec3> normals;   // 3 per face: normals[3 * i + k]
	void smooth_normals();
	float corner_angle(size_t i,int v)const;


};
