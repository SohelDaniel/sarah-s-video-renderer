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

	px::Image image;
	std::vector<float> depth;          // closest depth drawn so far, per pixel
	mat4<float> view_projection;       // projection * view
	mat4<float> to_screen;             // viewport
	vec3 light_dir;                    // direction towards the light
};
