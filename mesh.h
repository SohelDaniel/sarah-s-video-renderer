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

private:
	std::string name;
	std::vector<vec3> vertices;
	std::vector<triangle> faces;


};
