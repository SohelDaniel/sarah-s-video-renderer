#include "render.h"
#include "font.h"
#include "font8x8.h"
#include "timeline.h"
#include "transform.h"

#include <algorithm>
#include <cmath>
#include <limits>


// the color behind everything
static const px::Pixel background(20, 20, 28);

render::render(int width,int height,int samples)
	:image(width * samples, height * samples, background),
	 result(width, height, background),
	 samples(samples),
	 depth(size_t(width * samples) * size_t(height * samples), std::numeric_limits<float>::infinity()),
	 to_screen(viewport(width * samples, height * samples)),
	 light_dir(normalize(vec3(0.4f, 0.8f, 0.6f))){}

px::Image& render::out(){
	return samples > 1 ? result : image;
}

void render::begin(const camera& cam){
	image.Clear(background);
	std::fill(depth.begin(), depth.end(), std::numeric_limits<float>::infinity());
	float aspect = float(image.Width()) / float(image.Height());
	mat4<float> view = cam.view();
	view_projection = cam.projection(aspect) * view;
	// row 2 of the view matrix is the camera's up axis (docs/04)
	camera_up = vec3(view(1, 0), view(1, 1), view(1, 2));
	camera_eye = cam.eye();
	overlay.clear();
	waiting.clear();
	lines.clear();
	texts.clear();
	screen_lines.clear();
	maths.clear();
}

void render::draw_mesh(const mesh& model,const mat4<float>& model_matrix,px::Pixel color){
	// Transform every vertex ONCE up front. A vertex is shared by several
	// triangles (about 6 on the sphere), so doing it per triangle would repeat
	// the same matrix math over and over. It's kept in clip space (docs/05),
	// before dividing by w, because the near-plane cut has to happen there.
	int count = model.get_vertices_count();
	world_verts.resize(count);
	clip_verts.resize(count);
	for(int i = 0;i<count;i++){
		world_verts[i] = transform_point(model_matrix, model.vertex(i));
		const vec3& w = world_verts[i];
		clip_verts[i] = view_projection * vec4<float>{w[0], w[1], w[2], 1.0f};
	}

	for(int i = 0;i<model.get_faces_count();i++){
		triangle t = model.face(i);
		// .obj indices start at 1
		int k[3] = {t[0] - 1, t[1] - 1, t[2] - 1};
		const vec3 world[3] = {world_verts[k[0]], world_verts[k[1]], world_verts[k[2]]};
		const vec4<float> clip[3] = {clip_verts[k[0]], clip_verts[k[1]], clip_verts[k[2]]};
		clip_and_fill(world, clip, color);
	}
}

// ---------------------------------------------------------------------------
//  Near-plane clipping (docs/19).
//
//  In clip space, a point is in front of the near plane when z_ndc >= -1,
//  that is z/w >= -1, that is (w > 0)   d = z + w >= 0.
//  d is a straight-line function of the point, so along an edge from a to b
//  it changes evenly, and it's 0 exactly at
//        t = d_a / (d_a - d_b)          (between 0 and 1 when a, b are on different sides)
//  Sutherland-Hodgman: walk round the triangle's edges, keep the corners in
//  front, and add the crossing point wherever an edge goes through the
//  plane. A triangle with 1 corner behind becomes 4 corners (2 triangles),
//  with 2 corners behind it becomes a smaller triangle.
// ---------------------------------------------------------------------------
void render::clip_and_fill(const vec3 world[3],const vec4<float> clip[3],px::Pixel color){
	if(!clipping){
		// the old way: drop the whole triangle if any corner is behind the camera
		for(int k = 0;k<3;k++){
			if(clip[k].w <= 0.0f) return;
		}
		fill(world[0], world[1], world[2], to_pixels(clip[0]), to_pixels(clip[1]), to_pixels(clip[2]), color);
		return;
	}

	float d[3];
	int in_front = 0;
	for(int k = 0;k<3;k++){
		d[k] = clip[k].z + clip[k].w;
		if(d[k] >= 0.0f) in_front++;
	}
	if(in_front == 0) return;                // all behind the near plane
	if(in_front == 3){                       // nothing to cut
		fill(world[0], world[1], world[2], to_pixels(clip[0]), to_pixels(clip[1]), to_pixels(clip[2]), color);
		return;
	}

	vec3 out_world[4];
	vec4<float> out_clip[4];
	int n = 0;
	for(int a = 0;a<3;a++){
		int b = (a + 1) % 3;
		if(d[a] >= 0.0f){
			out_world[n] = world[a];
			out_clip[n] = clip[a];
			n++;
		}
		if((d[a] >= 0.0f) != (d[b] >= 0.0f)){
			float t = d[a] / (d[a] - d[b]);
			out_world[n] = lerp(world[a], world[b], t);
			out_clip[n] = vec4<float>{clip[a].x + (clip[b].x - clip[a].x) * t,
			                          clip[a].y + (clip[b].y - clip[a].y) * t,
			                          clip[a].z + (clip[b].z - clip[a].z) * t,
			                          clip[a].w + (clip[b].w - clip[a].w) * t};
			n++;
		}
	}
	// a fan from the first corner keeps the winding (front/back) the same
	vec3 s0 = to_pixels(out_clip[0]);
	for(int k = 1;k + 1<n;k++){
		fill(out_world[0], out_world[k], out_world[k + 1], s0, to_pixels(out_clip[k]), to_pixels(out_clip[k + 1]), color);
	}
}

vec3 render::to_pixels(const vec4<float>& clip)const{
	// perspective divide: this is where far things get small
	vec4<float> ndc{clip.x / clip.w, clip.y / clip.w, clip.z / clip.w, 1.0f};
	vec4<float> pos = to_screen * ndc;
	return vec3(pos.x, pos.y, pos.z);
}

bool render::project(const vec3& world,vec3& screen)const{
	vec4<float> clip = view_projection * vec4<float>{world[0], world[1], world[2], 1.0f};
	if(clip.w <= 0.0f) return false;
	screen = to_pixels(clip);
	return true;
}

void render::draw(vec3 v1,vec3 v2,vec3 v3,px::Pixel color,float see_through_by){
	opacity = see_through_by;
	const vec3 world[3] = {v1, v2, v3};
	vec4<float> clip[3];
	for(int k = 0;k<3;k++){
		clip[k] = view_projection * vec4<float>{world[k][0], world[k][1], world[k][2], 1.0f};
	}
	clip_and_fill(world, clip, color);
	opacity = 1.0f;
}

// ---------------------------------------------------------------------------
//  Lines and arrows (docs/26).
//
//  A line is drawn in screen space: project both ends, then for every pixel
//  near it, measure the distance d from the pixel's center to the segment
//  (the closest point is found by projecting onto it, docs/16):
//      coverage = clamp(width/2 + 1/2 - d, 0, 1)
//  1 well inside, 0 well outside, and in between for the one pixel the edge
//  passes through: that's what makes the edge smooth (anti-aliased) without
//  drawing anything bigger. The coverage becomes the pixel's alpha.
// ---------------------------------------------------------------------------
void render::draw_line(vec3 a,vec3 b,float width,px::Pixel color){
	lines.push_back({a, b, width, color, false});
}

void render::draw_arrow(vec3 a,vec3 b,px::Pixel color){
	lines.push_back({a, b, 2.5f * ui_scale(), color, true});
}

float render::line_coverage(float px,float py,float x0,float y0,float x1,float y1,float width){
	float dx = x1 - x0, dy = y1 - y0;
	float length2 = dx * dx + dy * dy;
	float s = length2 > 0.0f ? std::clamp(((px - x0) * dx + (py - y0) * dy) / length2, 0.0f, 1.0f) : 0.0f;
	float cx = x0 + s * dx - px, cy = y0 + s * dy - py;
	float d = std::sqrt(cx * cx + cy * cy);
	return std::clamp(width / 2.0f + 0.5f - d, 0.0f, 1.0f);
}

// Keep only the part of the segment in front of the near plane (the same
// test as for triangles, docs/19), and project its ends to the screen.
bool render::clip_segment(vec3& a,vec3& b,vec3& sa,vec3& sb)const{
	vec4<float> ca = view_projection * vec4<float>{a[0], a[1], a[2], 1.0f};
	vec4<float> cb = view_projection * vec4<float>{b[0], b[1], b[2], 1.0f};
	float da = ca.z + ca.w, db = cb.z + cb.w;
	if(da < 0.0f && db < 0.0f) return false;             // all behind the camera
	if(da < 0.0f || db < 0.0f){
		float t = da / (da - db);
		vec4<float> cut{ca.x + (cb.x - ca.x) * t, ca.y + (cb.y - ca.y) * t, ca.z + (cb.z - ca.z) * t, ca.w + (cb.w - ca.w) * t};
		vec3 cut_world = lerp(a, b, t);
		if(da < 0.0f){ ca = cut; a = cut_world; }
		else         { cb = cut; b = cut_world; }
	}
	sa = to_pixels(ca);
	sb = to_pixels(cb);
	return true;
}

void render::rasterize_line(const line& l){
	vec3 a = l.a, b = l.b, sa, sb;
	if(!clip_segment(a, b, sa, sb)) return;
	float width = l.width * samples;                      // the image may be drawn bigger (docs/25)

	// the arrowhead: a triangle at b pointing along the line, on screen
	float head = 0.0f;
	vec3 tip = sb, left, right;
	float dx = sb[0] - sa[0], dy = sb[1] - sa[1];
	float length = std::sqrt(dx * dx + dy * dy);
	if(length < 1.0f) return;
	float ux = dx / length, uy = dy / length;                // along the line
	if(l.arrowhead){
		head = std::min(14.0f * ui_scale() * samples, length * 0.5f);   // its length, in pixels
		vec3 base(sb[0] - ux * head, sb[1] - uy * head, sb[2]);
		left  = vec3(base[0] - uy * head * 0.45f, base[1] + ux * head * 0.45f, sb[2]);
		right = vec3(base[0] + uy * head * 0.45f, base[1] - ux * head * 0.45f, sb[2]);
		sb = base;                                           // the shaft stops where the head starts
	}

	// every pixel in the box around it (plus the width), clamped to the image
	float pad = width + head;
	int min_x = std::max(0, int(std::floor(std::min(sa[0], tip[0]) - pad)));
	int max_x = std::min(image.Width() - 1, int(std::ceil(std::max(sa[0], tip[0]) + pad)));
	int min_y = std::max(0, int(std::floor(std::min(sa[1], tip[1]) - pad)));
	int max_y = std::min(image.Height() - 1, int(std::ceil(std::max(sa[1], tip[1]) + pad)));

	for(int y = min_y;y<=max_y;y++){
		for(int x = min_x;x<=max_x;x++){
			float cx = x + 0.5f, cy = y + 0.5f;
			float coverage = line_coverage(cx, cy, sa[0], sa[1], sb[0], sb[1], width);
			if(l.arrowhead){
				// inside the head triangle: the smallest distance to its three
				// edges (positive inside), plus a half pixel for a smooth edge
				auto edge = [&](const vec3& p,const vec3& q){
					float ex = q[0] - p[0], ey = q[1] - p[1];
					float len = std::sqrt(ex * ex + ey * ey);
					return ((cx - p[0]) * ey - (cy - p[1]) * ex) / len;
				};
				float inside = std::min({edge(tip, right), edge(right, left), edge(left, tip)});
				// the triangle may be wound either way: use whichever sign is inside
				float other = std::min({-edge(tip, right), -edge(right, left), -edge(left, tip)});
				coverage = std::max(coverage, std::clamp(std::max(inside, other) + 0.5f, 0.0f, 1.0f));
			}
			if(coverage <= 0.0f) continue;

			// depth along the line (screen-space depth is linear, docs/08),
			// and only where nothing solid is in front
			float dlx = sb[0] - sa[0], dly = sb[1] - sa[1];
			float l2 = dlx * dlx + dly * dly;
			float s = l2 > 0.0f ? std::clamp(((cx - sa[0]) * dlx + (cy - sa[1]) * dly) / l2, 0.0f, 1.0f) : 1.0f;
			float z = sa[2] + (sb[2] - sa[2]) * s;
			if(z > depth[size_t(y) * image.Width() + x] + 1e-4f) continue;

			px::Pixel c = l.color;
			c.a = uint8_t(std::lround(255.0f * coverage));
			image.Draw(x, y, c);
		}
	}
}

void render::draw_see_through(const mesh& model,const mat4<float>& model_matrix,px::Pixel color,float how_solid){
	// the object's center is where the model matrix moves (0,0,0) to: its last column
	vec3 center(model_matrix(0, 3), model_matrix(1, 3), model_matrix(2, 3));
	vec3 d = center - camera_eye;
	waiting.push_back({&model, model_matrix, color, how_solid, std::sqrt(dot(d, d))});
}

void render::fill(const vec3& v1,const vec3& v2,const vec3& v3,
                  const vec3& s1,const vec3& s2,const vec3& s3,px::Pixel color){
	// Signed area (x2) of the triangle on screen. The .obj faces are
	// counter-clockwise when seen from outside, but screen y points down,
	// which flips that: a triangle facing us has negative area here.
	// Positive means we're looking at its back, so skip it.
	float area = vec3::det(s2 - s1, s3 - s1);
	if(area >= 0.0f) return;

	// Flat shading: the more the face points at the light, the brighter.
	vec3 normal = normalize(cross(v2 - v1, v3 - v1));
	const float ambient = 0.15f;
	float brightness = ambient + (1.0f - ambient) * std::max(0.0f, dot(normal, light_dir));
	// a < 255 makes Image::Draw blend it over what's there (docs/24)
	px::Pixel shaded(uint8_t(color.r * brightness),
	                 uint8_t(color.g * brightness),
	                 uint8_t(color.b * brightness),
	                 uint8_t(std::lround(255.0f * std::clamp(opacity, 0.0f, 1.0f))));

	// Only look at pixels inside the triangle's bounding box.
	int minX = std::max(0,                  int(std::floor(std::min({s1[0], s2[0], s3[0]}))));
	int maxX = std::min(image.Width() - 1,  int(std::ceil (std::max({s1[0], s2[0], s3[0]}))));
	int minY = std::max(0,                  int(std::floor(std::min({s1[1], s2[1], s3[1]}))));
	int maxY = std::min(image.Height() - 1, int(std::ceil (std::max({s1[1], s2[1], s3[1]}))));

	// row by row (y outside, x inside): the image and depth buffer are stored
	// row after row, so this reads memory in order, which is faster
	for(int j = minY;j<=maxY;j++){
		for(int i = minX;i<=maxX;i++){
			pixel p = {i,j};
			vec3 center(p.x + 0.5f, p.y + 0.5f, 0.0f);

			// Barycentric weights: the area of the small triangle made by the
			// pixel and one edge, divided by the full area. Each weight is how
			// much of the opposite corner is "in" this pixel. They add up to 1,
			// and all three are >= 0 only when the pixel is inside.
			float w1 = vec3::det(s3 - s2, center - s2) / area;
			float w2 = vec3::det(s1 - s3, center - s3) / area;
			float w3 = vec3::det(s2 - s1, center - s1) / area;
			// A pixel exactly on an edge shared by two triangles should give 0,
			// but float rounding can make it a tiny negative for BOTH triangles,
			// leaving a gap. Allow a tiny bit of slack so edge pixels get drawn.
			const float edge_slack = -1e-5f;
			if(w1 < edge_slack || w2 < edge_slack || w3 < edge_slack) continue;

			// Depth test: only draw if nothing closer is already there.
			float z = w1 * s1[2] + w2 * s2[2] + w3 * s3[2];
			float& closest = depth[size_t(p.y) * image.Width() + p.x];
			if(z >= closest) continue;
			// Something see-through doesn't claim the pixel: whatever is
			// behind it has to stay visible through it (docs/24).
			if(opacity >= 1.0f) closest = z;

			image.Draw(p.x, p.y, shaded);
     		}
	}

}

void render::draw_bounds(vec3 center,float radius){
	if(show_circles) overlay.push_back({center, radius});
}

float render::ui_scale()const{
	return float(image.Height()) / float(samples) / 480.0f;
}

void render::finish(){
	// See-through things last, FARTHEST FIRST: each one is blended over what's
	// behind it, so what's behind has to be drawn already (docs/24).
	std::stable_sort(waiting.begin(), waiting.end(),
		[](const see_through& a,const see_through& b){ return a.distance > b.distance; });
	for(const see_through& s : waiting){
		opacity = s.opacity;
		draw_mesh(*s.model, s.model_matrix, s.color);
	}
	opacity = 1.0f;
	waiting.clear();

	// lines and arrows, now that everything they could be hidden behind is
	// drawn (docs/26)
	for(const line& l : lines) rasterize_line(l);
	lines.clear();

	// Anti-aliasing (docs/25): everything was drawn samples x samples times
	// bigger. Each final pixel is the average of its block of small ones, so
	// a pixel that an edge cuts through gets a color in between.
	if(samples > 1){
		int n = samples * samples;
		for(int y = 0;y<result.Height();y++){
			for(int x = 0;x<result.Width();x++){
				int r = 0, g = 0, b = 0;
				for(int sy = 0;sy<samples;sy++){
					for(int sx = 0;sx<samples;sx++){
						px::Pixel p = image.Get(x * samples + sx, y * samples + sy);
						r += p.r; g += p.g; b += p.b;
					}
				}
				result.Draw(x, y, px::Pixel(uint8_t((r + n / 2) / n), uint8_t((g + n / 2) / n), uint8_t((b + n / 2) / n)));
			}
		}
	}

	// A sphere seen through a camera looks (almost exactly) like a circle.
	// Its screen radius: project the center, and a point on the sphere's
	// edge straight "up" from the camera's point of view, and measure the
	// distance between the two on screen.
	for(size_t i = 0;i<overlay.size();i++){
		const circle& c = overlay[i];
		// red if this sphere overlaps any other one right now (docs/11).
		// Touching isn't overlapping (a comet stuck to a planet, docs/18), and
		// float rounding can make "exactly touching" come out a millionth
		// short, so they must overlap by more than 0.01% of the distance.
		bool hit = false;
		for(size_t j = 0;j<overlay.size();j++){
			if(j == i) continue;
			vec3 d = c.center - overlay[j].center;
			float touch = c.radius + overlay[j].radius;
			if(std::sqrt(dot(d, d)) < touch * (1.0f - 1e-4f)) hit = true;
		}
		px::Pixel color = hit ? px::Pixel(255, 70, 70) : px::Pixel(150, 150, 160);
		vec3 mid, edge;
		if(!project(c.center, mid) || !project(c.center + camera_up * c.radius, edge)) continue;
		float dx = edge[0] - mid[0];
		float dy = edge[1] - mid[1];
		// (projected into the big image; the circles go on the final one, so
		// divide by samples, otherwise a 1-pixel line would average away)
		int r = int(std::lround(std::sqrt(dx * dx + dy * dy) / samples));
		out().DrawCircle(int(std::lround(mid[0] / samples)), int(std::lround(mid[1] / samples)), r, color);
	}
	overlay.clear();

	// leader lines from labels to their objects (docs/28), on the final picture
	px::Image& picture = out();
	for(const screen_line& l : screen_lines){
		int x0 = std::max(0, int(std::floor(std::min(l.x0, l.x1) - l.width)));
		int x1 = std::min(picture.Width() - 1, int(std::ceil(std::max(l.x0, l.x1) + l.width)));
		int y0 = std::max(0, int(std::floor(std::min(l.y0, l.y1) - l.width)));
		int y1 = std::min(picture.Height() - 1, int(std::ceil(std::max(l.y0, l.y1) + l.width)));
		for(int y = y0;y<=y1;y++){
			for(int x = x0;x<=x1;x++){
				float c = line_coverage(x + 0.5f, y + 0.5f, l.x0, l.y0, l.x1, l.y1, l.width);
				if(c <= 0.0f) continue;
				px::Pixel p = l.color;
				p.a = uint8_t(std::lround(p.a * c));
				picture.Draw(x, y, p);
			}
		}
	}
	screen_lines.clear();

	// text last, on top of everything (docs/27)
	for(const text_item& t : texts){
		if(t.bitmap){
			int scale = int(t.size);
			paint_text(t, scale, scale, px::Pixel(0, 0, 0, 170));   // the shadow
			paint_text(t, 0, 0, t.color);
		}else{
			float offset = std::max(1.0f, t.size / 16.0f);
			paint_outline_text(t, offset, offset, px::Pixel(0, 0, 0, uint8_t(170 * t.color.a / 255)));
			paint_outline_text(t, 0, 0, t.color);
		}
	}
	texts.clear();

	// formulas (docs/30): the same soft shadow, then the glyphs and bars
	for(const math_item& m : maths){
		px::Pixel shadow(0, 0, 0, uint8_t(170 * m.color.a / 255));
		paint_math(m, 1.5f * ui_scale(), 1.5f * ui_scale(), shadow);
		paint_math(m, 0.0f, 0.0f, m.color);
	}
	maths.clear();
}

void render::draw_math(float x,float y,const math_box& formula,px::Pixel color,float opacity){
	if(opacity <= 0.0f) return;
	color.a = uint8_t(std::lround(color.a * std::clamp(opacity, 0.0f, 1.0f)));
	maths.push_back({x, y, formula, color});
}

// Every glyph of the formula at its place (the baseline is `height` below
// the top), and every bar filled with smooth top and bottom edges.
void render::paint_math(const math_item& m,float dx,float dy,px::Pixel color){
	px::Image& picture = out();
	float baseline = m.y + dy + m.formula.height;
	auto blend = [&](int x,int y,float c){
		if(c <= 0.0f) return;
		px::Pixel p = color;
		p.a = uint8_t(std::lround(color.a * std::min(1.0f, c)));
		picture.Draw(x, y, p);
	};
	for(const math_glyph& g : m.formula.glyphs){
		const font::glyph& shape = g.face->get(g.codepoint, g.size);
		int left = int(std::lround(m.x + dx + g.x)) + shape.left;
		int top = int(std::lround(baseline + g.y)) + shape.top;
		for(int y = 0;y<shape.height;y++){
			for(int x = 0;x<shape.width;x++) blend(left + x, top + y, shape.coverage[size_t(y) * shape.width + x]);
		}
	}
	for(const math_rule& r : m.formula.rules){
		float x0 = m.x + dx + r.x, x1 = x0 + r.width;
		float y0 = baseline + r.y, y1 = y0 + r.height;
		for(int y = int(std::floor(y0));y<int(std::ceil(y1));y++){
			// how much of this pixel row the bar covers: its overlap with [y, y+1]
			float rows = std::min(y1, float(y + 1)) - std::max(y0, float(y));
			for(int x = int(std::floor(x0));x<int(std::ceil(x1));x++){
				float cols = std::min(x1, float(x + 1)) - std::max(x0, float(x));
				blend(x, y, rows * cols);
			}
		}
	}
}

// ---------------------------------------------------------------------------
//  Text (docs/27): an 8x8 bitmap font. Each character is 8 bytes, one per
//  row; bit x of a row's byte (lowest bit = leftmost) says whether pixel x
//  of that row is lit. Every lit pixel becomes a scale x scale square.
// ---------------------------------------------------------------------------
bool render::where_on_screen(const vec3& p,float r,float& x,float& y,float& radius)const{
	vec3 mid, edge;
	if(!project(p, mid) || !project(p + camera_up * r, edge)) return false;
	x = mid[0] / samples;
	y = mid[1] / samples;
	float dx = edge[0] - mid[0], dy = edge[1] - mid[1];
	radius = std::sqrt(dx * dx + dy * dy) / samples;
	return true;
}

void render::draw_screen_line(float x0,float y0,float x1,float y1,float width,px::Pixel color){
	screen_lines.push_back({x0, y0, x1, y1, width, color});
}

void render::draw_bitmap_text(int x,int y,const std::string& text,int scale,px::Pixel color){
	texts.push_back({float(x), float(y), text, float(scale), color, true});
}

int render::bitmap_text_width(const std::string& text,int scale){
	return int(text.size()) * 8 * scale;
}

int render::bitmap_text_height(int scale){
	return 8 * scale;
}

void render::draw_text(float x,float y,const std::string& text,float size,px::Pixel color){
	if(!fonts::sans()){
		// no font files: the bitmap font, as close in size as it gets
		int scale = std::max(1, int(std::lround(size / 8.0f)));
		draw_bitmap_text(int(x), int(y), text, scale, color);
		return;
	}
	texts.push_back({x, y, text, size, color, false});
}

float render::text_width(const std::string& text,float size){
	const font* f = fonts::sans();
	return f ? f->width(text, size) : float(bitmap_text_width(text, std::max(1, int(std::lround(size / 8.0f)))));
}

float render::text_height(float size){
	const font* f = fonts::sans();
	return f ? f->ascent(size) + f->descent(size) : float(bitmap_text_height(std::max(1, int(std::lround(size / 8.0f)))));
}

// Each letter's coverage (docs/29) blended at the pen position; the pen
// moves right by the letter's advance plus the kerning to the next one.
void render::paint_outline_text(const text_item& t,float dx,float dy,px::Pixel color){
	const font* f = fonts::sans();
	px::Image& picture = out();
	std::vector<int> codes = utf8_decode(t.text);
	float pen = t.x + dx;
	float baseline = t.y + dy + f->ascent(t.size);
	for(size_t i = 0;i<codes.size();i++){
		const font::glyph& g = f->get(codes[i], t.size);
		int left = int(std::lround(pen)) + g.left;
		int top = int(std::lround(baseline)) + g.top;
		for(int y = 0;y<g.height;y++){
			for(int x = 0;x<g.width;x++){
				float c = g.coverage[size_t(y) * g.width + x];
				if(c <= 0.0f) continue;
				px::Pixel p = color;
				p.a = uint8_t(std::lround(color.a * c));
				picture.Draw(left + x, top + y, p);
			}
		}
		pen += f->advance(codes[i], t.size);
		if(i + 1 < codes.size()) pen += f->kerning(codes[i], codes[i + 1], t.size);
	}
}

void render::paint_text(const text_item& t,int dx,int dy,px::Pixel color){
	px::Image& picture = out();
	for(size_t i = 0;i<t.text.size();i++){
		unsigned char c = (unsigned char)t.text[i];
		if(c >= 128) c = '?';                                   // only plain ASCII in this font
		int scale = int(t.size);
		int left = int(t.x) + int(i) * 8 * scale + dx;
		for(int row = 0;row<8;row++){
			unsigned char bits = font8x8_basic[c][row];
			for(int col = 0;col<8;col++){
				if(((bits >> col) & 1) == 0) continue;
				for(int sy = 0;sy<scale;sy++){
					for(int sx = 0;sx<scale;sx++){
						picture.Draw(left + col * scale + sx, int(t.y) + dy + row * scale + sy, color);
					}
				}
			}
		}
	}
}

bool render::save(const std::string& filename)const{
	return picture().Save(filename);
}

const px::Image& render::picture()const{
	return samples > 1 ? result : image;
}
