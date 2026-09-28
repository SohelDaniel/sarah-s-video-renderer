#pragma once
#include "camera.h"
#include "mat4.h"
#include "mesh.h"
#include "pixel.h"
#include <string>
#include <vector>

struct pixel{
	int x;
	int y;
};
class render{
public:
	render(int width,int height,const camera& cam);

	// Draw every triangle of `model`, placed in the world by `model_matrix`.
	void draw_mesh(const mesh& model,const mat4<float>& model_matrix,px::Pixel color);
	// Draw one triangle given in world space.
	void draw(vec3 v1,vec3 v2,vec3 v3,px::Pixel color);
	bool save(const std::string& filename)const;

private:
	// World space -> screen. Result: x,y in pixels, z = depth (-1 near .. 1 far).
	// Returns false if the point is behind the camera.
	bool project(const vec3& world,vec3& screen)const;
	// Fill one triangle. v = world space (for lighting), s = screen space.
	void fill(const vec3& v1,const vec3& v2,const vec3& v3,
	          const vec3& s1,const vec3& s2,const vec3& s3,px::Pixel color);

	px::Image image;
	std::vector<float> depth;          // closest depth drawn so far, per pixel
	mat4<float> view_projection;       // projection * view
	mat4<float> to_screen;             // viewport
	vec3 light_dir;                    // direction towards the light

	// scratch space for draw_mesh, kept between calls so drawing many
	// objects doesn't allocate new memory every time
	std::vector<vec3> world_verts;
	std::vector<vec3> screen_verts;
	std::vector<char> visible;
};
