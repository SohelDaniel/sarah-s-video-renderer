#include "mesh.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>

mesh::mesh(const std::string& file_name)
	:name(file_name){
	std::ifstream in(name);
	if(!in){
		throw std::invalid_argument("could not open file " + name);
	}

	std::string line;
	while(std::getline(in,line)){
		if(line.compare(0,2,"v ") == 0){
			std::string tag;
			std::istringstream iss(line);
			float a = 0.0f;
			float b = 0.0f;
			float c = 0.0f;
			iss>>tag>>a>>b>>c;
			vec3 temp(a,b,c);
			vertices.push_back(temp);
		}else if(line.compare(0,2,"f ") == 0){
			// "f 1 2 3" or "f 1/4/7 2/5/8 3/6/9": the vertex index is the
			// number before the first '/'. Only triangles are supported.
			std::istringstream iss(line.substr(2));
			std::string token;
			int index[3];
			for(int k = 0;k<3;k++){
				if(!(iss>>token)){
					throw std::runtime_error("face needs 3 vertices in " + name + ": " + line);
				}
				index[k] = std::stoi(token.substr(0, token.find('/')));
			}
			triangle temp(index[0],index[1],index[2]);
			faces.push_back(temp);
		}
	}
	in.close();

	// .obj indices start at 1
	for(const triangle& t : faces){
		for(int k = 0;k<3;k++){
			if(t[k] < 1 || t[k] > get_vertices_count()){
				throw std::out_of_range("face uses a vertex that doesn't exist in " + name);
			}
		}
	}

	// bounding radius: the farthest vertex from the center
	for(const vec3& v : vertices){
		radius = std::max(radius, std::sqrt(dot(v, v)));
	}
	smooth_normals();
}

// ---------------------------------------------------------------------------
//  Corner normals (docs/36).
//
//  A face's own normal is cross(b - a, c - a), made 1 long: it points out of
//  the shape (faces go counter-clockwise seen from outside). A flat-shaded
//  face uses it everywhere, so every face of a sphere shows as a facet.
//
//  For smooth shading, each CORNER gets a normal: the average of the face
//  normals of every face that shares that vertex, each weighted by its
//  angle at that corner... but only the faces that bend away from this one
//  by less than the crease angle:
//        dot(n_this, n_other) >= cos(40°) = 0.766
//  A sphere's neighbouring faces differ by ~11°, so all count: smooth. A
//  cube's three faces at a corner differ by 90°, so each corner only counts
//  its own face: the cube stays sharp.
// ---------------------------------------------------------------------------
// The angle of face i's corner at vertex v (radians): how much of the view
// round that vertex this face takes up, so a thin sliver counts for less
// than a wide face (docs/36).
float mesh::corner_angle(size_t i,int v)const{
	const triangle& t = faces[i];
	int k = t[0] == v ? 0 : t[1] == v ? 1 : 2;
	vec3 here = vertices[size_t(t[k] - 1)];
	vec3 to_a = normalize(vertices[size_t(t[(k + 1) % 3] - 1)] - here);
	vec3 to_b = normalize(vertices[size_t(t[(k + 2) % 3] - 1)] - here);
	return std::acos(std::clamp(dot(to_a, to_b), -1.0f, 1.0f));
}

void mesh::smooth_normals(){
	std::vector<vec3> face_normal(faces.size());
	std::vector<std::vector<int>> faces_at(vertices.size());   // vertex -> faces using it
	for(size_t i = 0;i<faces.size();i++){
		const triangle& t = faces[i];
		vec3 a = vertices[t[0] - 1], b = vertices[t[1] - 1], c = vertices[t[2] - 1];
		face_normal[i] = normalize(cross(b - a, c - a));
		for(int k = 0;k<3;k++) faces_at[size_t(t[k] - 1)].push_back(int(i));
	}
	const float least = std::cos(crease_degrees * 3.14159265f / 180.0f);
	find_edges(face_normal);
	normals.assign(faces.size() * 3, vec3(0.0f, 0.0f, 0.0f));
	for(size_t i = 0;i<faces.size();i++){
		for(int k = 0;k<3;k++){
			int v = faces[i][k];
			vec3 sum(0.0f, 0.0f, 0.0f);
			for(int j : faces_at[size_t(v - 1)]){
				if(dot(face_normal[i], face_normal[size_t(j)]) < least) continue;
				sum = sum + face_normal[size_t(j)] * corner_angle(size_t(j), v);
			}
			normals[3 * i + size_t(k)] = normalize(sum);   // never 0: face i always counts itself
		}
	}
}

// Every edge belongs to one face (an open edge) or two. An edge is a crease
// if its two faces bend away from each other by more than the crease angle,
// the same test as for smooth shading (docs/36):
//     dot(n1, n2) < cos 40° = 0.766
// (docs/44)
void mesh::find_edges(const std::vector<vec3>& face_normal){
	std::map<std::pair<int, int>, std::vector<size_t>> faces_of;   // edge (smaller vertex first) -> its faces
	for(size_t i = 0;i<faces.size();i++){
		for(int k = 0;k<3;k++){
			int a = faces[i][k] - 1, b = faces[i][(k + 1) % 3] - 1;
			faces_of[{std::min(a, b), std::max(a, b)}].push_back(i);
		}
	}
	const float least = std::cos(crease_degrees * 3.14159265f / 180.0f);
	std::vector<std::pair<int, int>> all;
	for(const auto& [edge, around] : faces_of){
		all.push_back(edge);
		bool crease = around.size() != 2 || dot(face_normal[around[0]], face_normal[around[1]]) < least;
		if(crease) edges.push_back(edge);
	}
	if(edges.empty()) edges = all;   // smooth all over: trace its whole grid
}

vec3 mesh::corner_normal(int i,int k)const{
	return normals[3 * size_t(i) + size_t(k)];
}
int mesh::get_vertices_count()const{
	return vertices.size();
}
int mesh::get_faces_count()const{
	return faces.size();
}
vec3 mesh::vertex(int i)const{
	return vertices[i];
}
triangle mesh::face(int i)const{
	return faces[i];
}
float mesh::bounding_radius()const{
	return radius;
}
