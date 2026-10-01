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
	// Makes the image and depth buffer once. Reuse the same render for every
	// frame so we don't allocate a new picture 60 times a second.
	// samples = 2 draws everything twice as wide and tall, then averages each
	// 2x2 block into one pixel: smooth edges (anti-aliasing, docs/25).
	render(int width,int height,int samples = 1);

	// Start a new picture: wipe the image and depth buffer, and set up the
	// matrices for where the camera is right now. Call before drawing.
	void begin(const camera& cam);

	// Draw every triangle of `model`, placed in the world by `model_matrix`.
	void draw_mesh(const mesh& model,const mat4<float>& model_matrix,px::Pixel color);
	// See-through (docs/24): opacity between 0 and 1. It's only remembered
	// here, and drawn by finish(): after everything solid, farthest first.
	void draw_see_through(const mesh& model,const mat4<float>& model_matrix,px::Pixel color,float opacity);
	// Draw one triangle given in world space (opacity 1 = solid).
	void draw(vec3 v1,vec3 v2,vec3 v3,px::Pixel color,float opacity = 1.0f);
	// Outline of a sphere (a bounding sphere), as a circle on screen. It's
	// only remembered here and drawn by finish(), on top of everything:
	// red if it overlaps another one in this frame, grey if not.
	void draw_bounds(vec3 center,float radius);
	// A line between two points in the world, `width` pixels wide, with
	// smooth edges, hidden behind things that are in front of it (docs/26).
	// Like see-through things, it's drawn by finish(), after the solid ones.
	void draw_line(vec3 a,vec3 b,float width,px::Pixel color);
	// A line with an arrowhead at b.
	void draw_arrow(vec3 a,vec3 b,px::Pixel color);

	// Where a point in the world lands on the finished picture, in pixels,
	// and how big a sphere of radius r around it looks there (docs/28).
	// False if it's behind the camera.
	bool where_on_screen(const vec3& p,float r,float& x,float& y,float& radius)const;
	// A smooth line straight on the finished picture (no depth): label
	// leader lines (docs/28). Drawn by finish(), just before the text.
	void draw_screen_line(float x0,float y0,float x1,float y1,float width,px::Pixel color);

	// Text with smooth outline letters (docs/29), `size` pixels tall (the em),
	// with its top-left corner at pixel (x, y) of the finished picture, and a
	// soft shadow so it reads over anything. Drawn last, on top of everything.
	// Falls back to the bitmap font if fonts/ is missing.
	void draw_text(float x,float y,const std::string& text,float size,px::Pixel color);
	// How wide and tall that text will be, in pixels
	static float text_width(const std::string& text,float size);
	static float text_height(float size);

	// The 8x8 bitmap font (docs/27): each font pixel a scale x scale square.
	void draw_bitmap_text(int x,int y,const std::string& text,int scale,px::Pixel color);
	static int bitmap_text_width(const std::string& text,int scale);
	static int bitmap_text_height(int scale);

	// How much of the pixel at (px, py) a line from (x0,y0) to (x1,y1),
	// `width` wide, covers: 0..1 (docs/26). Public so the tests can check it.
	static float line_coverage(float px,float py,float x0,float y0,float x1,float y1,float width);

	// Draw what has to wait: see-through objects, then the circles on top.
	// Call after all objects.
	void finish();
	bool save(const std::string& filename)const;

	// Cut triangles at the near plane instead of dropping them (docs/19).
	// Only switched off to show what goes wrong without it.
	bool clipping = true;
	// The finished picture, e.g. to show it in a window.
	const px::Image& picture()const;

private:
	// World space -> screen. Result: x,y in pixels, z = depth (-1 near .. 1 far).
	// Returns false if the point is behind the camera.
	bool project(const vec3& world,vec3& screen)const;
	// Clip space (after the projection matrix, before dividing by w) -> screen.
	vec3 to_pixels(const vec4<float>& clip)const;
	// One triangle, given in world space and in clip space: cut it at the
	// near plane if it crosses it (docs/19), then fill what's left.
	void clip_and_fill(const vec3 world[3],const vec4<float> clip[3],px::Pixel color);
	// Fill one triangle. v = world space (for lighting), s = screen space.
	void fill(const vec3& v1,const vec3& v2,const vec3& v3,
	          const vec3& s1,const vec3& s2,const vec3& s3,px::Pixel color);

	px::Image image;                   // what's drawn into (samples times bigger)
	px::Image result;                  // the finished picture, when samples > 1
	int samples = 1;
	px::Image& out();                  // the picture at its real size
	std::vector<float> depth;          // closest depth drawn so far, per pixel
	mat4<float> view_projection;       // projection * view
	mat4<float> to_screen;             // viewport
	vec3 light_dir;                    // direction towards the light
	vec3 camera_up;                    // the camera's up axis in world space

	struct circle{
		vec3 center;
		float radius;
	};
	std::vector<circle> overlay;       // waiting for finish()

	struct see_through{
		const mesh* model;
		mat4<float> model_matrix;
		px::Pixel color;
		float opacity;
		float distance;                // from the camera, for sorting
	};
	std::vector<see_through> waiting;  // waiting for finish()

	struct line{
		vec3 a, b;
		float width;
		px::Pixel color;
		bool arrowhead;
	};
	std::vector<line> lines;           // waiting for finish()

	struct text_item{
		float x, y;
		std::string text;
		float size;            // pixels for outline text; the scale for bitmap text
		px::Pixel color;
		bool bitmap;
	};
	std::vector<text_item> texts;      // waiting for finish()

	struct screen_line{
		float x0, y0, x1, y1, width;
		px::Pixel color;
	};
	std::vector<screen_line> screen_lines;   // waiting for finish()
	void paint_text(const text_item& t,int dx,int dy,px::Pixel color);
	void paint_outline_text(const text_item& t,float dx,float dy,px::Pixel color);
	void rasterize_line(const line& l);
	bool clip_segment(vec3& a,vec3& b,vec3& sa,vec3& sb)const;
	float opacity = 1.0f;              // of what's being drawn right now
	vec3 camera_eye;

	// scratch space for draw_mesh, kept between calls so drawing many
	// objects doesn't allocate new memory every time
	std::vector<vec3> world_verts;
	std::vector<vec4<float>> clip_verts;
};
