#pragma once
#include "pixel.h"
#include "timeline.h"

#include <deque>
#include <string>
#include <vector>

// ============================================================================
//  A scene the way the AI describes it: WHAT exists and how things RELATE
//  to each other. Never exact coordinates: picking numbers is the part an AI
//  is bad at, so that's left to the layout solver (layout.h).
//
//  For now scenes are written in C++ like this:
//
//      scene_spec spec;
//      spec.add("cube",   "shapes/cube.obj",   orange, size_word::big, 10);
//      spec.add("sphere", "shapes/sphere.obj", blue).near("cube");
//
//  Later a parser will read the same thing from a text file and fill in
//  exactly these structs.
// ============================================================================


// Sizes as words. The AI says "big", the engine decides what big means.
enum class size_word{ tiny, small, normal, big, huge };

// The scale each size word stands for (1 = the mesh as stored in the file).
inline float size_value(size_word s){
	switch(s){
		case size_word::tiny:   return 0.3f;
		case size_word::small:  return 0.5f;
		case size_word::normal: return 0.8f;
		case size_word::big:    return 1.4f;
		case size_word::huge:   return 2.0f;
	}
	return 1.0f;
}

inline const char* size_name(size_word s){
	switch(s){
		case size_word::tiny:   return "tiny";
		case size_word::small:  return "small";
		case size_word::normal: return "normal";
		case size_word::big:    return "big";
		case size_word::huge:   return "huge";
	}
	return "?";
}

// How one object relates to another.
//   left_of / right_of  : along x
//   above / below       : along y
//   in_front_of / behind: along z (in front = towards +z, where the camera usually is)
//   near                : close to it, any side
enum class relation_kind{ near, left_of, right_of, above, below, in_front_of, behind };

inline const char* relation_name(relation_kind k){
	switch(k){
		case relation_kind::near:        return "near";
		case relation_kind::left_of:     return "left_of";
		case relation_kind::right_of:    return "right_of";
		case relation_kind::above:       return "above";
		case relation_kind::below:       return "below";
		case relation_kind::in_front_of: return "in_front_of";
		case relation_kind::behind:      return "behind";
	}
	return "?";
}

// Where the camera looks from, as a word. The solver works out the exact
// spot (docs/14). None of them is straight above: look_at can't handle a
// camera directly over its target (docs/04).
enum class view_word{ front, front_above, left_above, right_above };

inline const char* view_name(view_word v){
	switch(v){
		case view_word::front:       return "front";
		case view_word::front_above: return "front_above";
		case view_word::left_above:  return "left_above";
		case view_word::right_above: return "right_above";
	}
	return "?";
}

// "sphere near cube" is stored on the sphere as {near, "cube"}.
struct relation{
	relation_kind kind;
	std::string other;   // the name of the object it's relative to
};

// Something that moves (docs/16). The AI only says what kind of motion,
// around or past what, and when; the solver picks the exact path.
//   orbits     : goes round the other object `turns` times
//   flies_past : flies by the other object in a straight line
//   hits       : flies into the other object ON PURPOSE, arriving exactly
//                at `end`, and sticks to it (docs/18)
enum class motion_kind{ orbits, flies_past, hits };

inline const char* motion_name(motion_kind k){
	switch(k){
		case motion_kind::orbits:     return "orbits";
		case motion_kind::flies_past: return "flies_past";
		case motion_kind::hits:       return "hits";
	}
	return "?";
}

struct motion{
	motion_kind kind;
	std::string other;    // what it moves around or past
	float turns = 1.0f;   // orbits only
	float start = 0.0f;   // seconds
	float end   = 0.0f;
	rate how = rate::linear;   // how it eases (docs/23); orbits stay linear
};

// A change of how see-through it is (docs/24): to 1 = fade in, to 0 = fade out.
struct fade_step{
	float to;
	float start, end;
	rate how = rate::linear;
};

// One object in the scene, as described.
struct object_spec{
	std::string name;       // how other objects refer to it
	std::string mesh_file;  // which shape, e.g. "shapes/cube.obj"
	px::Pixel color;
	size_word size = size_word::normal;
	int importance = 1;     // higher = placed first, gets the best spot
	std::vector<relation> relations;
	std::vector<motion> motions;   // empty = it stands still
	std::vector<fade_step> fades;  // fading in or out (docs/24)

	// Each of these adds a relation and returns the object itself, so they
	// can be chained:  spec.add(...).near("cube").above("table");
	object_spec& near(const std::string& other)       { return relate(relation_kind::near, other); }
	object_spec& left_of(const std::string& other)    { return relate(relation_kind::left_of, other); }
	object_spec& right_of(const std::string& other)   { return relate(relation_kind::right_of, other); }
	object_spec& above(const std::string& other)      { return relate(relation_kind::above, other); }
	object_spec& below(const std::string& other)      { return relate(relation_kind::below, other); }
	object_spec& in_front_of(const std::string& other){ return relate(relation_kind::in_front_of, other); }
	object_spec& behind(const std::string& other)     { return relate(relation_kind::behind, other); }

	object_spec& orbits(const std::string& other,float turns,float start,float end){
		motions.push_back({motion_kind::orbits, other, turns, start, end});
		return *this;
	}
	object_spec& flies_past(const std::string& other,float start,float end,rate how = rate::linear){
		motions.push_back({motion_kind::flies_past, other, 0.0f, start, end, how});
		return *this;
	}
	// starts invisible and fades in / fades out to invisible (docs/24)
	object_spec& fades_in(float start,float end,rate how = rate::linear){
		fades.push_back({1.0f, start, end, how});
		return *this;
	}
	object_spec& fades_out(float start,float end,rate how = rate::linear){
		fades.push_back({0.0f, start, end, how});
		return *this;
	}
	// arrives at `time`, after flying for `approach` seconds, and sticks
	object_spec& hits(const std::string& other,float time,float approach = 4.0f,rate how = rate::linear){
		motions.push_back({motion_kind::hits, other, 0.0f, time - approach, time, how});
		return *this;
	}

private:
	object_spec& relate(relation_kind kind,const std::string& other){
		relations.push_back({kind, other});
		return *this;
	}
};

// An arrow from one object to another (docs/26): "this pulls that".
// Its ends sit on the two objects' surfaces and follow them as they move.
struct arrow_spec{
	std::string from, to;
	px::Pixel color = px::Pixel(235, 235, 245);
	float start = 0.0f;
	float end = -1.0f;   // visible from start to end; end < 0 = always
};

// The whole scene, as described.
class scene_spec{
public:
	arrow_spec& add_arrow(const std::string& from,const std::string& to){
		arrows.push_back(arrow_spec{from, to, px::Pixel(235, 235, 245), 0.0f, -1.0f});
		return arrows.back();
	}
	std::deque<arrow_spec> arrows;

	object_spec& add(const std::string& name,const std::string& mesh_file,px::Pixel color,
	                 size_word size = size_word::normal,int importance = 1){
		objects.push_back(object_spec{name, mesh_file, color, size, importance, {}, {}, {}});
		return objects.back();
	}

	// where the camera looks from (the solver picks the distance)
	view_word view = view_word::front_above;

	// A deque, not a vector: adding to a vector can move everything to new
	// memory, which would break the reference add() just handed out. A deque
	// never moves what's already in it.
	std::deque<object_spec> objects;
};
