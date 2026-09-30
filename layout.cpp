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
		case method::naive: place_naive(); method_name = "naive"; break;
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

	if(!errors.empty()){
		out << "  problems in the description:\n";
		for(const std::string& e : errors) out << "    " << e << "\n";
	}
	return out.str();
}
