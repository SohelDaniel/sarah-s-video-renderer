// ============================================================================
//  Tests for the engine itself (docs/19-21): the parts that aren't the
//  layout solver. No window and no SDL, so they run anywhere.
//
//  Exits with 1 if anything failed, so `make test` stops.
// ============================================================================
#include "camera.h"
#include "render.h"

#include <cstdio>
#include <string>


static int failures = 0;

static void check(bool ok,const std::string& what){
	std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
	if(!ok) failures++;
}

// How many pixels aren't the background color.
static int drawn_pixels(const render& r){
	const px::Image& img = r.picture();
	px::Pixel background = img.Get(0, 0);   // the corner is never drawn on in these tests
	int count = 0;
	for(int y = 0;y<img.Height();y++){
		for(int x = 0;x<img.Width();x++){
			px::Pixel p = img.Get(x, y);
			if(p.r != background.r || p.g != background.g || p.b != background.b) count++;
		}
	}
	return count;
}

// ---- near-plane clipping (docs/19) ----
static void test_clipping(){
	std::printf("near-plane clipping:\n");
	camera cam;
	cam.width = 160;
	cam.height = 120;
	cam.move(vec3(0.0f, 0.0f, 4.0f));
	cam.point_at(vec3(0.0f, 0.0f, 0.0f));

	// a floor-like triangle: two corners in front of the camera, one far
	// behind it (z = 10 is 6 units behind the camera at z = 4). Corners in
	// the order a, b, c are counter-clockwise seen from ABOVE, so it faces up,
	// towards the camera (otherwise back-face culling would skip it, docs/07).
	vec3 a(-2.0f, -1.0f, -2.0f), b(0.0f, -1.0f, 10.0f), c(2.0f, -1.0f, -2.0f);

	render with(cam.width, cam.height);
	with.begin(cam);
	with.draw(a, b, c, px::Pixel(200, 200, 200));
	int on = drawn_pixels(with);

	render without(cam.width, cam.height);
	without.clipping = false;
	without.begin(cam);
	without.draw(a, b, c, px::Pixel(200, 200, 200));
	int off = drawn_pixels(without);

	check(on > 500, "a triangle reaching behind the camera is still drawn (" + std::to_string(on) + " pixels)");
	check(off == 0, "without clipping, the same triangle disappears (" + std::to_string(off) + " pixels)");

	// a triangle completely behind the camera draws nothing
	render behind(cam.width, cam.height);
	behind.begin(cam);
	behind.draw(vec3(-1, 0, 6), vec3(1, 0, 6), vec3(0, 1, 6), px::Pixel(200, 200, 200));
	check(drawn_pixels(behind) == 0, "a triangle completely behind the camera draws nothing");
}

int main(){
	test_clipping();
	std::printf("\n%s: %d check%s failed\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
	return failures == 0 ? 0 : 1;
}
