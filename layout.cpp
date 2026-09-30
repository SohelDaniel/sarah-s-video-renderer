#include "layout.h"

#include <cmath>
#include <iomanip>
#include <sstream>


static float distance(const vec3& p,const vec3& q){
	vec3 d = p - q;
	return std::sqrt(dot(d, d));
}

layout::layout(const scene_spec& spec,const std::vector<float>& mesh_radii)
	: spec(spec){
	for(size_t i = 0;i<spec.objects.size();i++){
		const object_spec& o = spec.objects[i];
		placement p;
		p.name   = o.name;
		p.size   = size_value(o.size);
		p.radius = mesh_radii[i] * p.size;
		placed.push_back(p);
	}
	resolve_names();
	sort_by_dependencies();
}

// Turn every "near cube" into "near object #0". A name that doesn't exist is
// reported and the relation is dropped: the rest of the scene still works.
void layout::resolve_names(){
	links.assign(spec.objects.size(), {});
	for(size_t i = 0;i<spec.objects.size();i++){
		for(const relation& r : spec.objects[i].relations){
			int found = -1;
			for(size_t j = 0;j<spec.objects.size();j++){
				if(spec.objects[j].name == r.other) found = int(j);
			}
			if(found < 0){
				errors.push_back(spec.objects[i].name + " " + relation_name(r.kind) + " " + r.other
				                 + ": there is no object called \"" + r.other + "\" (relation ignored)");
			}else if(found == int(i)){
				errors.push_back(spec.objects[i].name + " " + relation_name(r.kind)
				                 + " itself (relation ignored)");
			}else{
				links[i].push_back({r.kind, found});
			}
		}
	}
}

// "sphere near cube" means the cube has to be placed before the sphere.
// Topological sort (Kahn's algorithm): repeatedly take an object whose
// references are all placed already. When several are ready, the most
// important one goes first.
void layout::sort_by_dependencies(){
	int n = int(spec.objects.size());
	std::vector<int> waiting_for(n, 0);   // how many of its references aren't placed yet
	for(int i = 0;i<n;i++) waiting_for[i] = int(links[i].size());

	std::vector<bool> done(n, false);
	while(int(order.size()) < n){
		// pick the most important object that isn't waiting for anything
		int best = -1;
		for(int i = 0;i<n;i++){
			if(done[i] || waiting_for[i] > 0) continue;
			if(best < 0 || spec.objects[i].importance > spec.objects[best].importance) best = i;
		}
		if(best < 0){
			// everyone left is waiting for someone else: a cycle, like
			// "a left_of b" + "b left_of a". Break it at the most important one.
			for(int i = 0;i<n;i++){
				if(done[i]) continue;
				if(best < 0 || spec.objects[i].importance > spec.objects[best].importance) best = i;
			}
			errors.push_back(spec.objects[best].name + " is part of a cycle of relations"
			                 " (placed before the objects it refers to)");
		}
		done[best] = true;
		order.push_back(best);
		// everyone who refers to `best` now has one less thing to wait for
		for(int i = 0;i<n;i++){
			for(const link& l : links[i]){
				if(l.other == best) waiting_for[i]--;
			}
		}
	}
}

void layout::solve(method how){
	switch(how){
		case method::naive:  place_naive();  method_name = "naive";  break;
		case method::greedy: place_greedy(); method_name = "greedy"; break;
	}
}

// Step A, the baseline: put every object exactly where its first relation
// points (the other object's center), without looking at sizes or at anyone
// else. Objects with no relation go to the origin. This is roughly what you
// get when an AI guesses coordinates: everything piled up.
void layout::place_naive(){
	for(int i : order){
		placed[i].position = links[i].empty() ? vec3(0.0f, 0.0f, 0.0f)
		                                      : placed[links[i][0].other].position;
	}
}

// Step B: the direction a relation points in, from the other object.
static vec3 direction_of(relation_kind kind){
	switch(kind){
		case relation_kind::left_of:     return vec3(-1.0f, 0.0f, 0.0f);
		case relation_kind::right_of:    return vec3( 1.0f, 0.0f, 0.0f);
		case relation_kind::above:       return vec3( 0.0f, 1.0f, 0.0f);
		case relation_kind::below:       return vec3( 0.0f,-1.0f, 0.0f);
		case relation_kind::in_front_of: return vec3( 0.0f, 0.0f, 1.0f);
		case relation_kind::behind:      return vec3( 0.0f, 0.0f,-1.0f);
		case relation_kind::near:        return vec3( 1.0f, 0.0f, 0.0f); // not used
	}
	return vec3(1.0f, 0.0f, 0.0f);
}

// Candidate spots for object i, from one relation, best first.
//
// Every spot is at distance  D = r_i + r_other + gap  from the other
// object's center (the spheres then have exactly `gap` of empty space
// between them), times 1, 1.5, 2 or 3 if the close ones are all taken.
//   near      : 8 directions around it, sides and front first, then the
//               same 8 tilted a bit upwards
//   left_of...: straight in that direction first, then fanned out 30° and
//               then 60° in the four directions around it (to get around
//               whatever is in the way)
std::vector<vec3> layout::candidates(int i,const link& l)const{
	const placement& other = placed[l.other];
	float D = placed[i].radius + other.radius + gap;

	std::vector<vec3> directions;
	if(l.kind == relation_kind::near){
		const vec3 flat[8] = {
			vec3( 1, 0, 0), vec3(-1, 0, 0),           // right, left
			vec3( 1, 0, 1), vec3(-1, 0, 1),           // front-right, front-left
			vec3( 1, 0,-1), vec3(-1, 0,-1),           // back-right, back-left
			vec3( 0, 0, 1), vec3( 0, 0,-1),           // front, back
		};
		for(const vec3& d : flat) directions.push_back(normalize(d));
		for(const vec3& d : flat) directions.push_back(normalize(d + vec3(0, 0.6f, 0)));
	}else{
		vec3 d = direction_of(l.kind);
		// two directions at right angles to d
		vec3 side = std::fabs(d[1]) > 0.5f ? vec3(1, 0, 0) : vec3(0, 1, 0);
		vec3 across = normalize(cross(d, side));
		side = cross(across, d);
		directions.push_back(d);
		// tilt d towards each of the four: by 30° (tan 30° = 0.577),
		// then by 60° (tan 60° = 1.732)
		for(float tilt : {0.577f, 1.732f}){
			for(const vec3& t : {side, side * -1.0f, across, across * -1.0f}){
				directions.push_back(normalize(d + t * tilt));
			}
		}
	}

	std::vector<vec3> spots;
	for(float k : {1.0f, 1.5f, 2.0f, 3.0f, 4.0f}){
		for(const vec3& d : directions){
			spots.push_back(other.position + d * (D * k));
		}
	}
	return spots;
}

// How much object i, at `spot`, would dig into the objects already placed
// (counting the wanted `gap` too): 0 means the spot is free.
float layout::crowding(int i,const vec3& spot,const std::vector<bool>& done)const{
	float total = 0.0f;
	for(size_t j = 0;j<placed.size();j++){
		if(!done[j] || int(j) == i) continue;
		float need = placed[i].radius + placed[j].radius + gap;
		float d = distance(spot, placed[j].position);
		if(d < need - 1e-4f) total += need - d;
	}
	return total;
}

// How many of object i's relations are fully satisfied where it is now.
int layout::satisfied(int i)const{
	int count = 0;
	for(const link& l : links[i]){
		if(check(i, l) == verdict::ok) count++;
	}
	return count;
}

// Step B: place objects one at a time, in dependency order (most important
// first). Each one looks at the candidate spots of its first relation, and
// takes the free spot that satisfies the most of its relations (the first
// such spot on a tie, which is the closest and most natural one). Objects
// that were placed earlier never move again: that's what makes it greedy.
void layout::place_greedy(){
	std::vector<bool> done(placed.size(), false);
	int anchor = order.empty() ? -1 : order[0];   // the most important object

	for(int i : order){
		if(i == anchor){
			placed[i].position = vec3(0.0f, 0.0f, 0.0f);   // the main object goes in the middle
			done[i] = true;
			continue;
		}
		// no relation (or only broken ones): treat it as "near" the main object
		link first = links[i].empty() ? link{relation_kind::near, anchor} : links[i][0];

		std::vector<vec3> spots = candidates(i, first);
		int best_score = -1;
		vec3 best;
		for(const vec3& spot : spots){
			if(crowding(i, spot, done) > 0.0f) continue;
			placed[i].position = spot;
			int score = satisfied(i);
			if(score > best_score){
				best_score = score;
				best = spot;
			}
		}
		if(best_score < 0){
			// every spot is taken: use the least crowded one and say so
			float least = -1.0f;
			for(const vec3& spot : spots){
				float c = crowding(i, spot, done);
				if(least < 0.0f || c < least){
					least = c;
					best = spot;
				}
			}
			warnings.push_back("no free spot for " + placed[i].name + " (placed where it overlaps least)");
		}
		placed[i].position = best;
		done[i] = true;
	}
}

const std::vector<placement>& layout::result()const{
	return placed;
}

bool layout::overlapping(int i)const{
	for(size_t j = 0;j<placed.size();j++){
		if(int(j) == i) continue;
		if(distance(placed[i].position, placed[j].position) < placed[i].radius + placed[j].radius) return true;
	}
	return false;
}

int layout::count_overlaps()const{
	int count = 0;
	for(size_t i = 0;i<placed.size();i++){
		for(size_t j = i + 1;j<placed.size();j++){
			if(distance(placed[i].position, placed[j].position) < placed[i].radius + placed[j].radius) count++;
		}
	}
	return count;
}

// Is object i's relation l true right now?
//   ok     : fully true
//   weak   : the right idea but not quite (e.g. left, but the spheres still
//            overlap sideways; or near-ish but too far)
//   failed : wrong (e.g. on the right when it should be on the left)
layout::verdict layout::check(int i,const link& l)const{
	const placement& a = placed[i];
	const placement& b = placed[l.other];
	vec3 d = a.position - b.position;       // from b to a
	float reach = a.radius + b.radius;       // center distance where they'd just touch

	// For the direction relations: how far a is from b along that axis.
	// "a left_of b" wants a.x to be at least `reach` less than b.x.
	float along = 0.0f;
	switch(l.kind){
		case relation_kind::near:{
			float surface_gap = distance(a.position, b.position) - reach;
			if(surface_gap < 0.0f) return verdict::weak;        // touching/overlapping
			if(surface_gap <= near_limit) return verdict::ok;
			if(surface_gap <= 2.0f * near_limit) return verdict::weak;
			return verdict::failed;
		}
		case relation_kind::left_of:     along = -d[0]; break;
		case relation_kind::right_of:    along =  d[0]; break;
		case relation_kind::above:       along =  d[1]; break;
		case relation_kind::below:       along = -d[1]; break;
		case relation_kind::in_front_of: along =  d[2]; break;
		case relation_kind::behind:      along = -d[2]; break;
	}
	if(along >= reach) return verdict::ok;       // fully on that side
	if(along > 0.0f)   return verdict::weak;     // right side, but not clear of it
	return verdict::failed;                      // wrong side (or level with it)
}

std::string layout::report()const{
	std::ostringstream out;
	out << std::fixed << std::setprecision(2);

	out << "layout (" << method_name << "): " << placed.size() << " objects, "
	    << count_overlaps() << " overlapping pairs\n";

	out << "  objects (in placement order):\n";
	for(int i : order){
		const placement& p = placed[i];
		out << "    " << std::left << std::setw(12) << p.name
		    << std::setw(7) << size_name(spec.objects[i].size) << std::right
		    << " r=" << std::setw(5) << p.radius
		    << "  at (" << std::setw(6) << p.position[0] << ", " << std::setw(6) << p.position[1]
		    << ", " << std::setw(6) << p.position[2] << ")"
		    << (overlapping(i) ? "  OVERLAPS" : "") << "\n";
	}

	int ok = 0, total = 0;
	out << "  relations:\n";
	for(int i : order){
		for(const link& l : links[i]){
			verdict v = check(i, l);
			total++;
			if(v == verdict::ok) ok++;
			const char* word = v == verdict::ok ? "ok    " : v == verdict::weak ? "weak  " : "FAILED";
			out << "    " << word << "  " << placed[i].name << " " << relation_name(l.kind)
			    << " " << placed[l.other].name << "\n";
		}
	}
	out << "  " << ok << " of " << total << " relations satisfied\n";

	if(!warnings.empty()){
		out << "  problems while solving:\n";
		for(const std::string& w : warnings) out << "    " << w << "\n";
	}
	if(!errors.empty()){
		out << "  problems in the description:\n";
		for(const std::string& e : errors) out << "    " << e << "\n";
	}
	return out.str();
}
