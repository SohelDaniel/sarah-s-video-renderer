#include "render.h"
#include "timeline.h"
#include "transform.h"

#include <algorithm>
#include <cmath>
#include <limits>


// the color behind everything
static const px::Pixel background(20, 20, 28);

render::render(int width,int height)
	:image(width, height, background),
	 depth(size_t(width) * size_t(height), std::numeric_limits<float>::infinity()),
	 to_screen(viewport(width, height)),
	 light_dir(normalize(vec3(0.4f, 0.8f, 0.6f))){}

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
	overlay.push_back({center, radius});
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

	// A sphere seen through a camera looks (almost exactly) like a circle.
	// Its screen radius: project the center, and a point on the sphere's
	// edge straight "up" from the camera's point of view, and measure the
	// distance between the two on screen.
	for(size_t i = 0;i<overlay.size();i++){
		const circle& c = overlay[i];
		// red if this sphere overlaps any other one right now (docs/11)
		bool hit = false;
		for(size_t j = 0;j<overlay.size();j++){
			if(j == i) continue;
			vec3 d = c.center - overlay[j].center;
			if(std::sqrt(dot(d, d)) < c.radius + overlay[j].radius) hit = true;
		}
		px::Pixel color = hit ? px::Pixel(255, 70, 70) : px::Pixel(150, 150, 160);
		vec3 mid, edge;
		if(!project(c.center, mid) || !project(c.center + camera_up * c.radius, edge)) continue;
		float dx = edge[0] - mid[0];
		float dy = edge[1] - mid[1];
		int r = int(std::lround(std::sqrt(dx * dx + dy * dy)));
		image.DrawCircle(int(std::lround(mid[0])), int(std::lround(mid[1])), r, color);
	}
	overlay.clear();
}

bool render::save(const std::string& filename)const{
	return image.Save(filename);
}

const px::Image& render::picture()const{
	return image;
}
