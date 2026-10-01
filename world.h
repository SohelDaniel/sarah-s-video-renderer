#pragma once
#include "camera.h"
#include "frame_source.h"
#include "label_layout.h"
#include "math_layout.h"
#include "mesh.h"
#include "object.h"
#include "scene_spec.h"
#include "solver.h"

#include <map>
#include <string>
#include <vector>

// A described scene turned into real things you can draw:
//   1. loads every mesh the description mentions (each file only once)
//   2. runs the solver to get positions and paths
//   3. makes one object per described object (moving ones get timed
//      animations along their paths), plus a camera
//
// It owns everything, so the meshes live exactly as long as the objects
// pointing at them.
class world{
public:
	// width x height: the picture's size in pixels (docs/34)
	world(const scene_spec& spec,layout::method still_how,
	      motion_plan::method moving_how = motion_plan::method::naive,
	      int width = 640,int height = 480);

	camera cam;

	// the objects, in the form player::play wants
	std::vector<object*> scene();
	// only the objects that stand still
	std::vector<object*> still_objects();
	// how long the video should play: until the last motion ends, at least 20 s
	float duration()const;
	std::string report()const;
	// titles, arrows and labels, at time t (docs/26-28). Call after drawing
	// the objects. (Not const: labels remember last frame, docs/28.)
	void draw_overlays(render& renderer,float t);

private:
	// loads the meshes and returns each object's mesh radius (for the solver)
	std::vector<float> load_meshes(const scene_spec& spec);

	// declared in this order on purpose: members are built top to bottom,
	// and the solver needs the meshes loaded first
	std::map<std::string, mesh> meshes;   // file name -> mesh
	std::map<std::string, flat_shape> flat_shapes;   // "circle", "square filled", ... (docs/40)
	scene_solver solved;
	std::vector<object> objects;
	std::vector<bool> moves;              // moves[i]: does object i follow a path?

	struct world_arrow{
		int from, to;                     // object indices
		px::Pixel color;
		float start, end;
	};
	std::vector<world_arrow> arrows;
	// titles and formulas as vector paths (docs/37), built once
	struct world_title{
		title_spec when;
		float size;                       // 30 px (times ui), or less if it wouldn't fit across
		vgroup paths;                     // empty if fonts/ is missing: then plain draw_text
	};
	std::vector<world_title> titles;
	struct world_math{
		title_spec when;
		std::vector<vgroup> stages;              // the formula, then each one it becomes (docs/39)
		std::vector<std::vector<int>> matches;   // matches[k]: where stage k's pieces go in stage k + 1
	};
	std::vector<world_math> maths;
	struct world_label{
		std::string text;                 // "" = no label
		bool math = false, always = false;
		float start = 0.0f, end = -1.0f;
		math_box formula;                 // if math: laid out once
	};
	std::vector<world_label> labels;      // labels[i] = object i's label
	label_layout placer;
	std::vector<std::string> arrow_errors;
	// every size on the picture (text, gaps) is chosen for 480 pixels tall,
	// and multiplied by this: height / 480 (docs/34)
	float ui = 1.0f;
};

// A world as a frame source, for the player (docs/22).
class world_source : public frame_source{
public:
	explicit world_source(world& w) : w(w), all(w.scene()){}
	camera& cam() override{ return w.cam; }
	const std::vector<object*>& objects() override{ return all; }
	float seconds() override{ return w.duration(); }
	void draw_overlays(render& renderer,float t) override{ w.draw_overlays(renderer, t); }

private:
	world& w;
	std::vector<object*> all;
};
