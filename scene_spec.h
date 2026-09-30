#pragma once
#include "pixel.h"

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

// "sphere near cube" is stored on the sphere as {near, "cube"}.
struct relation{
	relation_kind kind;
	std::string other;   // the name of the object it's relative to
};

// One object in the scene, as described.
struct object_spec{
	std::string name;       // how other objects refer to it
	std::string mesh_file;  // which shape, e.g. "shapes/cube.obj"
	px::Pixel color;
	size_word size = size_word::normal;
	int importance = 1;     // higher = placed first, gets the best spot
	std::vector<relation> relations;

	// Each of these adds a relation and returns the object itself, so they
	// can be chained:  spec.add(...).near("cube").above("table");
	object_spec& near(const std::string& other)       { return relate(relation_kind::near, other); }
	object_spec& left_of(const std::string& other)    { return relate(relation_kind::left_of, other); }
	object_spec& right_of(const std::string& other)   { return relate(relation_kind::right_of, other); }
	object_spec& above(const std::string& other)      { return relate(relation_kind::above, other); }
	object_spec& below(const std::string& other)      { return relate(relation_kind::below, other); }
	object_spec& in_front_of(const std::string& other){ return relate(relation_kind::in_front_of, other); }
	object_spec& behind(const std::string& other)     { return relate(relation_kind::behind, other); }

private:
	object_spec& relate(relation_kind kind,const std::string& other){
		relations.push_back({kind, other});
		return *this;
	}
};

// The whole scene, as described.
class scene_spec{
public:
	object_spec& add(const std::string& name,const std::string& mesh_file,px::Pixel color,
	                 size_word size = size_word::normal,int importance = 1){
		objects.push_back(object_spec{name, mesh_file, color, size, importance, {}});
		return objects.back();
	}

	// A deque, not a vector: adding to a vector can move everything to new
	// memory, which would break the reference add() just handed out. A deque
	// never moves what's already in it.
	std::deque<object_spec> objects;
};
