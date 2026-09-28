#include "render.h"
#include "transform.h"

#include <algorithm>
#include <cmath>
#include <limits>


render::render(int width,int height,const camera& cam)
	:image(width, height, px::Pixel(20, 20, 28)),
	 depth(size_t(width) * size_t(height), std::numeric_limits<float>::infinity()),
	 view_projection(cam.projection(float(width) / float(height)) * cam.view()),
	 to_screen(viewport(width, height)),
	 light_dir(normalize(vec3(0.4f, 0.8f, 0.6f))){}

void render::draw_mesh(const mesh& model,const mat4<float>& model_matrix,px::Pixel color){
	for(int i = 0;i<model.get_faces_count();i++){
		triangle t  = model.face(i);
		// .obj indices start at 1
		vec3 v1 = transform_point(model_matrix, model.vertex(t[0]-1));
		vec3 v2 = transform_point(model_matrix, model.vertex(t[1]-1));
		vec3 v3 = transform_point(model_matrix, model.vertex(t[2]-1));

		draw(v1,v2,v3,color);
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

	for(int i = minX;i<=maxX;i++){
		for(int j = minY;j<=maxY;j++){
			pixel p = {i,j};
			vec3 center(p.x + 0.5f, p.y + 0.5f, 0.0f);

			// Barycentric weights: the area of the small triangle made by the
			// pixel and one edge, divided by the full area. Each weight is how
			// much of the opposite corner is "in" this pixel. They add up to 1,
			// and all three are >= 0 only when the pixel is inside.
			float w1 = vec3::det(s3 - s2, center - s2) / area;
			float w2 = vec3::det(s1 - s3, center - s3) / area;
			float w3 = vec3::det(s2 - s1, center - s1) / area;
			if(w1 < 0.0f || w2 < 0.0f || w3 < 0.0f) continue;

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
