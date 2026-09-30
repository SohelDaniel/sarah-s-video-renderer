#include "render.h"
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
	view_projection = cam.projection(aspect) * cam.view();
}

void render::draw_mesh(const mesh& model,const mat4<float>& model_matrix,px::Pixel color){
	// Transform and project every vertex ONCE up front. A vertex is shared by
	// several triangles (about 6 on the sphere), so doing it per triangle
	// would repeat the same matrix math over and over.
	int count = model.get_vertices_count();
	world_verts.resize(count);
	screen_verts.resize(count);
	visible.resize(count);
	for(int i = 0;i<count;i++){
		world_verts[i] = transform_point(model_matrix, model.vertex(i));
		visible[i] = project(world_verts[i], screen_verts[i]);
	}

	for(int i = 0;i<model.get_faces_count();i++){
		triangle t  = model.face(i);
		// .obj indices start at 1
		int index1 = t[0]-1;
		int index2 = t[1]-1;
		int index3 = t[2]-1;
		if(!visible[index1] || !visible[index2] || !visible[index3]) continue;

		fill(world_verts[index1], world_verts[index2], world_verts[index3],
		     screen_verts[index1], screen_verts[index2], screen_verts[index3], color);
	}
}

bool render::project(const vec3& world,vec3& screen)const{
	vec4<float> clip = view_projection * vec4<float>{world[0], world[1], world[2], 1.0f};
	if(clip.w <= 0.0f) return false;

	// perspective divide: this is where far things get small
	vec4<float> ndc{clip.x / clip.w, clip.y / clip.w, clip.z / clip.w, 1.0f};

	vec4<float> pos = to_screen * ndc;
	screen = vec3(pos.x, pos.y, pos.z);
	return true;
}

void render::draw(vec3 v1,vec3 v2,vec3 v3,px::Pixel color){
	//project the vectors to pixels on screen
	vec3 s1, s2, s3;
	if(!project(v1,s1) || !project(v2,s2) || !project(v3,s3)) return;
	fill(v1,v2,v3,s1,s2,s3,color);
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
	px::Pixel shaded(uint8_t(color.r * brightness),
	                 uint8_t(color.g * brightness),
	                 uint8_t(color.b * brightness));

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
			closest = z;

			image.Draw(p.x, p.y, shaded);
     		}
	}

}

bool render::save(const std::string& filename)const{
	return image.Save(filename);
}

const px::Image& render::picture()const{
	return image;
}
