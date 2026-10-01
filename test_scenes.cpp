#include "test_scenes.h"


// A few shapes and colors to cycle through when a scene needs many objects.
static const char* const shape_files[] = {
	"shapes/sphere.obj", "shapes/cone.obj", "shapes/cylinder.obj", "shapes/icosahedron.obj",
	"shapes/octahedron.obj", "shapes/pyramid.obj", "shapes/tetrahedron.obj", "shapes/torus.obj",
};
static const px::Pixel colors[] = {
	px::Pixel( 80, 160, 230), px::Pixel(120, 200,  90), px::Pixel(200, 120, 220), px::Pixel(240, 220,  80),
	px::Pixel(160, 160, 170), px::Pixel(235, 235, 245), px::Pixel(220,  80,  70), px::Pixel( 90, 210, 200),
};
static const px::Pixel orange(230, 130, 60);

scene_spec lazy_ai_scene(){
	scene_spec spec;
	spec.add("cube",        "shapes/cube.obj",        orange, size_word::big, 10);
	spec.add("sphere",      "shapes/sphere.obj",      colors[0]).near("cube");
	spec.add("cone",        "shapes/cone.obj",        colors[1]).near("cube");
	spec.add("cylinder",    "shapes/cylinder.obj",    colors[2]).near("cube");
	spec.add("icosahedron", "shapes/icosahedron.obj", colors[3], size_word::small).near("cube");
	spec.add("pyramid",     "shapes/pyramid.obj",     colors[5]).above("cube");
	spec.add("torus",       "shapes/torus.obj",       colors[7]).left_of("sphere");
	spec.add("tetrahedron", "shapes/tetrahedron.obj", colors[6], size_word::small).behind("cube");
	spec.add("octahedron",  "shapes/octahedron.obj",  colors[4], size_word::small).near("moon");
	return spec;
}

// Scene 4, written the way a lazy AI would write an animation: two planets
// told to orbit the sun without saying how far out, a rock sitting right
// where they'd go, a comet that "flies past" the sun, and a moon orbiting a
// planet (which isn't supported yet).
scene_spec lazy_motion_scene(){
	scene_spec spec;
	spec.add("sun",     "shapes/sphere.obj",      px::Pixel(250, 200,  60), size_word::big, 10);
	spec.add("rock",    "shapes/icosahedron.obj", colors[4]).near("sun");
	spec.add("planet1", "shapes/octahedron.obj",  colors[6]).orbits("sun", 1.0f, 0.0f, 20.0f);
	spec.add("planet2", "shapes/cube.obj",        colors[0], size_word::small).orbits("sun", 2.0f, 0.0f, 20.0f);
	spec.add("comet",   "shapes/pyramid.obj",     colors[5], size_word::small).flies_past("sun", 4.0f, 10.0f);
	spec.add("moon",    "shapes/tetrahedron.obj", colors[5], size_word::tiny).orbits("planet1", 3.0f, 0.0f, 20.0f);
	return spec;
}

// Scene 5: collisions that are MEANT to happen. A comet hits a planet that
// is moving round the sun, at exactly 12 s, and sticks; a meteor hits a rock
// that stands still, at 6 s.
scene_spec impact_scene(){
	scene_spec spec;
	spec.add("sun",    "shapes/sphere.obj",      px::Pixel(250, 200,  60), size_word::big, 10);
	spec.add("rock",   "shapes/icosahedron.obj", colors[4]).near("sun");
	spec.add("planet", "shapes/octahedron.obj",  colors[6]).orbits("sun", 1.0f, 0.0f, 20.0f);
	spec.add("comet",  "shapes/pyramid.obj",     colors[5], size_word::small).hits("planet", 12.0f, 4.0f);
	spec.add("meteor", "shapes/tetrahedron.obj", colors[3], size_word::small).hits("rock", 6.0f, 3.0f);
	return spec;
}

// Scene 5 with easing (docs/23): the comet and meteor ease in and out, and a
// probe flies past the sun, rushing in. Easing makes them faster at their
// fastest, so the collision checks have to look more often.
static scene_spec eased_scene(){
	scene_spec spec;
	spec.add("sun",    "shapes/sphere.obj",      px::Pixel(250, 200,  60), size_word::big, 10);
	spec.add("rock",   "shapes/icosahedron.obj", colors[4]).near("sun");
	spec.add("planet", "shapes/octahedron.obj",  colors[6]).orbits("sun", 1.0f, 0.0f, 20.0f);
	spec.add("comet",  "shapes/pyramid.obj",     colors[5], size_word::small).hits("planet", 12.0f, 4.0f, rate::smooth);
	spec.add("meteor", "shapes/tetrahedron.obj", colors[3], size_word::small).hits("rock", 6.0f, 3.0f, rate::rush_into);
	spec.add("probe",  "shapes/cube.obj",        colors[0], size_word::small).flies_past("sun", 2.0f, 9.0f, rate::rush_into);
	return spec;
}

// Every way a motion can't work.
static scene_spec motion_mistakes(){
	scene_spec spec;
	spec.add("sun",   "shapes/sphere.obj",      px::Pixel(250, 200, 60), size_word::big, 10);
	spec.add("a",     "shapes/cube.obj",        colors[0]).orbits("b", 1.0f, 0.0f, 10.0f);     // a round b ...
	spec.add("b",     "shapes/cone.obj",        colors[1]).orbits("a", 1.0f, 0.0f, 10.0f);     // ... and b round a
	spec.add("c",     "shapes/octahedron.obj",  colors[2]).orbits("sunn", 1.0f, 0.0f, 10.0f);  // misspelled
	spec.add("d",     "shapes/pyramid.obj",     colors[3]).orbits("d", 1.0f, 0.0f, 10.0f);     // itself
	spec.add("e",     "shapes/torus.obj",       colors[4]).orbits("sun", 1.0f, 8.0f, 2.0f);    // ends before it starts
	spec.add("f",     "shapes/sphere.obj",      colors[5], size_word::small).orbits("sun", 1.0f, 0.0f, 10.0f);
	spec.add("g",     "shapes/tetrahedron.obj", colors[6], size_word::small).flies_past("f", 2.0f, 6.0f);  // past a mover
	return spec;
}

// 20 things all "near" one cube: far more than fit around it.
static scene_spec crowd(){
	scene_spec spec;
	spec.add("cube", "shapes/cube.obj", orange, size_word::big, 10);
	for(int k = 0;k<20;k++){
		spec.add("thing" + std::to_string(k + 1), shape_files[k % 8], colors[k % 8],
		         k % 3 == 0 ? size_word::small : size_word::normal).near("cube");
	}
	return spec;
}

// 15 objects, each relative to the one before: the scene gets long, and the
// camera has to back away a lot.
static scene_spec chain(){
	scene_spec spec;
	spec.add("link1", "shapes/cube.obj", orange, size_word::normal, 10);
	for(int k = 2;k<=15;k++){
		object_spec& o = spec.add("link" + std::to_string(k), shape_files[k % 8], colors[k % 8]);
		std::string before = "link" + std::to_string(k - 1);
		switch(k % 3){
			case 0:  o.right_of(before); break;
			case 1:  o.above(before);    break;
			default: o.near(before);     break;
		}
	}
	return spec;
}

// a waits for b, b waits for c, c waits for a: nobody can go first.
static scene_spec cycle(){
	scene_spec spec;
	spec.add("a", "shapes/cube.obj",   orange,    size_word::normal, 3).left_of("b");
	spec.add("b", "shapes/sphere.obj", colors[0], size_word::normal, 2).left_of("c");
	spec.add("c", "shapes/cone.obj",   colors[1], size_word::normal, 1).left_of("a");
	return spec;
}

// Relations that can't all be true at once.
static scene_spec contradiction(){
	scene_spec spec;
	spec.add("a", "shapes/cube.obj",     orange,    size_word::normal, 5).left_of("b");
	spec.add("b", "shapes/sphere.obj",   colors[0]).left_of("a");
	spec.add("c", "shapes/cone.obj",     colors[1]).above("d").below("d");
	spec.add("d", "shapes/cylinder.obj", colors[2]);
	return spec;
}

// Huge and tiny things side by side.
static scene_spec sizes(){
	scene_spec spec;
	spec.add("giant", "shapes/cube.obj",        orange,    size_word::huge, 10);
	spec.add("dust1", "shapes/tetrahedron.obj", colors[6], size_word::tiny).near("giant");
	spec.add("dust2", "shapes/octahedron.obj",  colors[4], size_word::tiny).above("giant");
	spec.add("ball",  "shapes/sphere.obj",      colors[0], size_word::huge).near("dust1");
	spec.add("dust3", "shapes/icosahedron.obj", colors[3], size_word::tiny).left_of("ball");
	return spec;
}

// Every kind of mistake in the names.
static scene_spec typos(){
	scene_spec spec;
	spec.add("cube",   "shapes/cube.obj",   orange, size_word::big, 10);
	spec.add("sphere", "shapes/sphere.obj", colors[0]).near("cueb");     // misspelled
	spec.add("cone",   "shapes/cone.obj",   colors[1]).near("cone");     // itself
	spec.add("cube",   "shapes/cube.obj",   colors[2]).near("sphere");   // a second "cube"
	spec.add("torus",  "shapes/torus.obj",  colors[7]).left_of("");      // empty name
	return spec;
}

static scene_spec single(){
	scene_spec spec;
	spec.add("alone", "shapes/torus.obj", colors[7]);
	return spec;
}

static scene_spec seen_from(view_word v){
	scene_spec spec = lazy_ai_scene();
	spec.view = v;
	return spec;
}

std::vector<test_scene> all_test_scenes(){
	return {
		{"lazy",          "the scene from docs/11-14",                        lazy_ai_scene(),                     1},
		{"crowd",         "20 objects all near one cube",                     crowd(),                             0},
		{"chain",         "15 objects in a row, each relative to the last",   chain(),                             0},
		{"cycle",         "a needs b needs c needs a",                        cycle(),                             1},
		{"contradiction", "relations that can't all be true",                 contradiction(),                     2},
		{"sizes",         "huge and tiny objects side by side",               sizes(),                             0},
		{"typos",         "misspelled, self and duplicate names",             typos(),                             4},
		{"empty",         "no objects at all",                                scene_spec{},                        0},
		{"single",        "just one object",                                  single(),                            0},
		{"view_front",    "the lazy scene seen from the front",               seen_from(view_word::front),         1},
		{"view_left",     "the lazy scene seen from the left, above",         seen_from(view_word::left_above),    1},
		{"view_right",    "the lazy scene seen from the right, above",        seen_from(view_word::right_above),   1},
		{"motion",        "orbits, a moon, and a fly-by crossing a crowded spot", lazy_motion_scene(),             0},
		{"motion_typos",  "every way a motion can't work",                    motion_mistakes(),                   5},
		{"impact",        "collisions that are meant to happen, on time",     impact_scene(),                      0},
		{"eased",         "eased hits and fly-bys: faster at their fastest",  eased_scene(),                       0},
	};
}
