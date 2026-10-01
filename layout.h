#pragma once
#include "scene_spec.h"
#include "vec3.h"

#include <string>
#include <vector>

// ============================================================================
//  The layout solver: turns a scene_spec (names + relations) into positions.
//
//  Every object is treated as a sphere (its bounding sphere, see mesh.h), so
//  "do these two overlap?" is one test:  distance < r1 + r2.
//
//  The steps (docs/11 .. 14):
//    naive  : everything goes where its relation points, no checking (the mess)
//    greedy : one at a time, most important first; try candidate spots that
//             fit the relation and take the best one that's free
//    refined: greedy, then everything moves a little at a time downhill on
//             an "energy" (gradient descent) until it settles
//    framed : like refined, but the camera is placed automatically so the
//             whole scene fits in the picture
// ============================================================================

// Where one object ended up.
struct placement{
	std::string name;
	vec3 position{0.0f, 0.0f, 0.0f};
	float size   = 1.0f;   // scale to draw the mesh at
	float radius = 0.0f;   // bounding sphere radius at that size
};

class layout{
public:
	enum class method{ naive, greedy, refined, framed };

	// Where the camera will look from. Only used to judge what's hidden
	// behind what on screen (the refinement tries to fix that).
	void set_camera(vec3 eye,vec3 target);
	// The lens the camera will use, for the automatic framing.
	void set_lens(float fov_y,float aspect);
	vec3 camera_eye()const;
	vec3 camera_target()const;
	// Is a sphere at p with radius r completely inside the picture (docs/15)?
	// edge_scale < 1 shrinks the picture's edges, to leave a margin;
	// extra_px is room it needs around it on screen (its label, docs/31).
	// Nothing may be in the band kept for titles at the top.
	bool in_picture(const vec3& p,float r,float edge_scale = 1.0f,float extra_px = 0.0f)const;

	// Room for words (docs/31): a band at the top for titles and formulas,
	// and how wide each object's label is (0 = none), all in pixels of a
	// picture `picture_height` tall. Call before solve().
	void set_words(float top_band_px,const std::vector<float>& label_widths_px,int picture_height);
	// How many labels wouldn't find a free spot next to their object, with
	// the camera where it is now (it runs the label layout, docs/28)
	int labels_without_room()const;
	// Make the automatic camera also fit this sphere (a motion path, docs/16),
	// then place the camera again. Only does anything after method::framed.
	void include_in_frame(const vec3& center,float radius);
	void reframe();

	// mesh_radii[i] = bounding radius of object i's mesh at size 1
	// (same order as spec.objects). The layout never loads files itself.
	layout(const scene_spec& spec,const std::vector<float>& mesh_radii);

	void solve(method how);

	const std::vector<placement>& result()const;
	// true if object i overlaps any other object
	bool overlapping(int i)const;
	// Everything an AI (or a person) needs to know about how it went.
	std::string report()const;

	// The numbers that say how good a layout is (docs/15). The tests check
	// these; the report prints them.
	struct metrics{
		int objects         = 0;
		int overlaps        = 0;   // pairs overlapping in 3D
		int hidden          = 0;   // pairs overlapping on screen
		int off_screen      = 0;   // objects not completely inside the picture
		int relations_ok    = 0;
		int relations_total = 0;
		int errors          = 0;   // mistakes in the description
		int warnings        = 0;   // problems while solving
		int steps_halved    = 0;   // refinement steps that overshot (docs/13)
		int labels_without_room = 0;   // labels that couldn't be placed (docs/31)
	};
	metrics measure()const;

	// wanted empty space between two objects' spheres (world units)
	static constexpr float gap = 0.4f;
	// "near" is satisfied if the two surfaces are at most this far apart
	static constexpr float near_limit = 2.0f;

	// refinement settings (docs/13)
	static constexpr float spring_weight   = 1.0f;    // beta : stay close to the greedy spot
	static constexpr float push_weight     = 10.0f;   // gamma: keep `gap` between objects
	static constexpr float relation_weight = 10.0f;   // rho  : keep relations true
	static constexpr float screen_weight   = 8000.0f;  // nu   : don't hide each other on screen
	static constexpr float screen_margin   = 0.02f;   // wanted space between circles on screen
	static constexpr float step_size       = 0.01f;   // eta
	static constexpr int   steps           = 500;

private:
	// A relation with the other object's name looked up to its index.
	struct link{
		relation_kind kind;
		int other;
	};

	void resolve_names();
	void drop_contradictions();
	void sort_by_dependencies();
	void place_naive();
	void place_greedy();
	std::vector<vec3> candidates(int i,const link& l)const;
	float crowding(int i,const vec3& spot,const std::vector<bool>& done)const;
	int satisfied(int i)const;

	void refine();
	void separate();
	// the energy of a layout, split into its four parts (docs/13)
	struct energy_parts{
		float spring = 0.0f, push = 0.0f, relations = 0.0f, screen = 0.0f;
		float total()const{ return spring + push + relations + screen; }
	};
	energy_parts energy(const std::vector<vec3>& home)const;
	std::vector<vec3> gradient(const std::vector<vec3>& home)const;

	// what the camera sees (docs/13): where an object's center lands on
	// screen, and how big its circle looks there
	struct seen{
		float x, y;     // screen position (tan-angle units, like NDC / focal)
		float radius;   // circle radius, same units
		float depth;    // distance in front of the camera (<= 0: behind it)
	};
	// with_label: add half the label's width to the radius (the room the
	// refinement makes for words, docs/31); without: just the object
	seen look(int i,bool with_label = false)const;

	void frame();
	float fov_y  = 50.0f * 3.14159265f / 180.0f;
	float aspect = 640.0f / 480.0f;
	std::vector<std::pair<vec3, float>> extra_bounds;   // more spheres the camera has to fit
	struct sphere{
		vec3 center;
		float radius;
		float extra_px;                     // room on screen for its label (docs/31)
	};
	vec3 scene_center{0.0f, 0.0f, 0.0f};   // found by frame()
	float scene_radius = 0.0f;
	float camera_distance = 0.0f;
	float sphere_distance = 0.0f;           // the safe (loose) distance from the sphere fit
	bool fits_at(float distance,const std::vector<sphere>& spheres);
	int count_hidden()const;
	int count_off_screen()const;
	float px_to_tan()const;                     // one pixel, in look()'s units
	float top_band_px = 0.0f;
	int picture_height = 480;
	std::vector<float> label_px;                // per object: its label's width in pixels

	enum class verdict{ ok, weak, failed };
	verdict check(int i,const link& l)const;
	int count_overlaps()const;

	scene_spec spec;                        // its own copy, so it can't dangle
	std::vector<placement> placed;          // one per object, same order as spec.objects
	std::vector<std::vector<link>> links;   // links[i] = object i's relations
	std::vector<int> order;                 // the order to place objects in
	std::vector<std::string> errors;        // problems found in the description
	std::vector<std::string> warnings;      // problems found while solving
	std::vector<std::string> energy_log;    // how the refinement went
	vec3 eye{0.0f, 6.0f, 16.0f};
	vec3 target{0.0f, 0.0f, 0.0f};
	std::string method_name = "none";
	bool framed = false;
	int halved = 0;                         // refinement steps that had to be halved
};
