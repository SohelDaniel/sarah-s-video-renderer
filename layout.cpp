#include "layout.h"
#include "label_layout.h"

#include <algorithm>
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
	drop_contradictions();
	sort_by_dependencies();
}

// Turn every "near cube" into "near object #0". A name that doesn't exist is
// reported and the relation is dropped: the rest of the scene still works.
void layout::resolve_names(){
	links.assign(spec.objects.size(), {});

	// Two objects with the same name: "near cube" would be ambiguous.
	for(size_t i = 0;i<spec.objects.size();i++){
		for(size_t j = 0;j<i;j++){
			if(spec.objects[i].name == spec.objects[j].name){
				errors.push_back("two objects are called \"" + spec.objects[i].name
				                 + "\" (relations to it use the first one)");
				break;
			}
		}
	}

	for(size_t i = 0;i<spec.objects.size();i++){
		for(const relation& r : spec.objects[i].relations){
			int found = -1;
			for(size_t j = 0;j<spec.objects.size() && found < 0;j++){
				if(spec.objects[j].name == r.other) found = int(j);   // the first match
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

// The opposite of a direction relation (near has none).
static bool opposite(relation_kind a,relation_kind b){
	auto pair = [&](relation_kind x,relation_kind y){ return (a == x && b == y) || (a == y && b == x); };
	return pair(relation_kind::left_of, relation_kind::right_of)
	    || pair(relation_kind::above, relation_kind::below)
	    || pair(relation_kind::in_front_of, relation_kind::behind);
}

// Relations that can't both be true:
//   "c above d" and "c below d"        (the same object, opposite directions)
//   "a left_of b" and "b left_of a"    (two objects, each on the same side of the other)
// The one written first wins; the later one is reported and ignored, so the
// solver isn't stuck pulling in two directions at once.
void layout::drop_contradictions(){
	for(size_t i = 0;i<links.size();i++){
		std::vector<link> kept;
		for(const link& l : links[i]){
			std::string clash;
			for(const link& k : kept){
				if(k.other == l.other && opposite(k.kind, l.kind)){
					clash = placed[i].name + " " + relation_name(k.kind) + " " + placed[k.other].name;
				}
			}
			if(l.kind != relation_kind::near && size_t(l.other) < i){
				// the other object came first: does it say the same about us?
				for(const link& k : links[l.other]){
					if(k.other == int(i) && k.kind == l.kind){
						clash = placed[l.other].name + " " + relation_name(k.kind) + " " + placed[i].name;
					}
				}
			}
			if(clash.empty()){
				kept.push_back(l);
			}else{
				errors.push_back(placed[i].name + " " + relation_name(l.kind) + " " + placed[l.other].name
				                 + " contradicts " + clash + " (relation ignored)");
			}
		}
		links[i] = kept;
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
		case method::refined: place_greedy(); refine(); method_name = "refined"; break;
		case method::framed:
			// frame first, so the screen term works in the real camera; then
			// again, because refining moved things. The tight framing reacts to
			// small moves, so refine once more for the new camera and frame a
			// last time (docs/14).
			place_greedy(); frame(); refine(); frame(); refine(); frame(); method_name = "framed"; break;
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

void layout::set_camera(vec3 new_eye,vec3 new_target){
	eye = new_eye;
	target = new_target;
}

void layout::set_lens(float new_fov_y,float new_aspect){
	fov_y = new_fov_y;
	aspect = new_aspect;
}

vec3 layout::camera_eye()const{
	return eye;
}

vec3 layout::camera_target()const{
	return target;
}

// The direction a view word looks FROM (from the scene towards the camera).
static vec3 view_direction(view_word v){
	switch(v){
		case view_word::front:       return normalize(vec3( 0.0f, 0.1f,   1.0f));
		case view_word::front_above: return normalize(vec3( 0.0f, 0.375f, 1.0f));
		case view_word::left_above:  return normalize(vec3(-0.7f, 0.5f,   0.7f));
		case view_word::right_above: return normalize(vec3( 0.7f, 0.5f,   0.7f));
	}
	return vec3(0.0f, 0.0f, 1.0f);
}

// ---------------------------------------------------------------------------
//  Step D: place the camera so the whole scene fits (docs/14).
//
//  1. One sphere around the whole scene:
//       center = middle of the box around all the objects' spheres
//       radius = farthest any object's sphere reaches from that center
//  2. The narrower of the two fields of view:
//       fov_x = 2·atan(tan(fov_y / 2) · aspect)
//       half  = min(fov_x, fov_y) / 2
//  3. A sphere of radius R just fits inside a cone of half-angle `half`
//     when its center is  R / sin(half)  from the tip:
//       distance = R / sin(half)   (plus 5% for breathing room)
//  4. eye = center + direction · distance, looking at the center.
// ---------------------------------------------------------------------------
void layout::frame(){
	// every sphere the camera has to fit: the objects, plus any extras
	// (motion paths, docs/16)
	std::vector<sphere> spheres;
	for(const auto& e : extra_bounds) spheres.push_back({e.first, e.second, 0.0f});
	// (labels aren't added here: a label needs room on ONE side, and the label
	// layout picks a side that has it. Asking for room on every side made
	// the camera back off without end, docs/31.)
	for(const placement& p : placed) spheres.push_back({p.position, p.radius, 0.0f});
	if(spheres.empty()) return;

	vec3 low  = spheres[0].center;
	vec3 high = spheres[0].center;
	for(const sphere& s : spheres){
		for(int k = 0;k<3;k++){
			low[k]  = std::min(low[k],  s.center[k] - s.radius);
			high[k] = std::max(high[k], s.center[k] + s.radius);
		}
	}
	scene_center = (low + high) * 0.5f;

	scene_radius = 0.0f;
	for(const sphere& s : spheres){
		scene_radius = std::max(scene_radius, distance(s.center, scene_center) + s.radius);
	}

	float fov_x = 2.0f * std::atan(std::tan(fov_y / 2.0f) * aspect);
	float half  = std::min(fov_x, fov_y) / 2.0f;
	sphere_distance = 1.05f * scene_radius / std::sin(half);

	// The sphere fit is safe but loose: a wide, flat scene gets lots of empty
	// space above and below. So: keep the direction, and binary-search the
	// smallest distance at which every sphere is still inside the picture,
	// with 5% of the picture kept free at the edges.
	//   too close -> something sticks out -> move the bottom of the range up
	//   fits      -> try closer           -> move the top of the range down
	// 30 halvings shrink the range by 2^30, about a billion times.
	float low_d = 0.0f, high_d = sphere_distance;
	for(int grow = 0;grow<20 && !fits_at(high_d, spheres);grow++) high_d *= 1.5f;
	for(int k = 0;k<30;k++){
		float mid = (low_d + high_d) / 2.0f;
		if(fits_at(mid, spheres)) high_d = mid;
		else                      low_d  = mid;
	}
	camera_distance = high_d;
	eye    = scene_center + view_direction(spec.view) * camera_distance;
	target = scene_center;
	framed = true;
}

// Does every sphere fit in the picture, with a 5% margin, when the camera
// stands `distance` from the scene's center? (Moves the camera to check.)
bool layout::fits_at(float distance,const std::vector<sphere>& spheres){
	eye    = scene_center + view_direction(spec.view) * distance;
	target = scene_center;
	for(const sphere& s : spheres){
		if(!in_picture(s.center, s.radius, 0.95f, s.extra_px)) return false;
	}
	return true;
}

// ---------------------------------------------------------------------------
//  What the camera sees.
//
//  Build the camera's axes (like look_at, docs/04): forward, right, up.
//  For an object at p, with v = p - eye:
//      depth  = dot(v, forward)            how far in front of the camera
//      x      = dot(v, right) / depth      where on screen (similar triangles, docs/05)
//      y      = dot(v, up)    / depth
//      radius = r / depth                  how big its circle looks
//  These are "tan-angle" units: multiply by focal and you get NDC.
// ---------------------------------------------------------------------------
struct camera_axes{
	vec3 forward, right, up;
};
static camera_axes axes_of(const vec3& eye,const vec3& target){
	camera_axes a;
	a.forward = normalize(target - eye);
	a.right   = normalize(cross(a.forward, vec3(0.0f, 1.0f, 0.0f)));
	a.up      = cross(a.right, a.forward);
	return a;
}

layout::seen layout::look(int i,bool with_label)const{
	camera_axes a = axes_of(eye, target);
	vec3 v = placed[i].position - eye;
	seen s;
	s.depth  = dot(v, a.forward);
	float z  = std::max(s.depth, 0.001f);
	s.x      = dot(v, a.right) / z;
	s.y      = dot(v, a.up) / z;
	// its footprint on screen: its circle, plus half its label's width, so
	// two labelled objects keep room between them for their words (docs/31)
	float label = with_label && i < int(label_px.size()) ? label_px[i] : 0.0f;
	s.radius = placed[i].radius / z + 0.5f * label * px_to_tan();
	return s;
}

// look() measures in "tan units" (docs/13): the picture's top edge is at
// tan(fov_y / 2), and the picture is picture_height pixels tall, so one
// pixel is 2 · tan(fov_y / 2) / picture_height of them.
float layout::px_to_tan()const{
	return 2.0f * std::tan(fov_y / 2.0f) / float(picture_height);
}

void layout::set_words(float band,const std::vector<float>& widths,int height){
	top_band_px = band;
	label_px = widths;
	label_px.resize(placed.size(), 0.0f);
	picture_height = height;
}

// Project every labelled object the way the renderer will, and let the real
// label layout (docs/28) try to place their words, with the title band kept
// out. Count the labels it can't place.
int layout::labels_without_room()const{
	int width = int(std::lround(picture_height * aspect));
	float px = px_to_tan();
	std::vector<label_request> requests;
	for(size_t i = 0;i<placed.size() && i<label_px.size();i++){
		if(label_px[i] <= 0.0f) continue;
		camera_axes a = axes_of(eye, target);
		vec3 v = placed[i].position - eye;
		float depth = dot(v, a.forward);
		label_request r{label_px[i], 20.0f, {0, 0}, 0, false};
		if(depth > 0.0f){
			r.anchor = {width / 2.0f + dot(v, a.right) / depth / px, picture_height / 2.0f - dot(v, a.up) / depth / px};
			r.radius = placed[i].radius / depth / px;
			r.visible = true;
		}
		requests.push_back(r);
	}
	if(requests.empty()) return 0;
	label_layout labels(width, picture_height);
	if(top_band_px > 0.0f) labels.keep_out.push_back({0.0f, float(width), 0.0f, top_band_px});
	int missing = 0;
	for(const placed_label& p : labels.place(requests)) if(!p.shown) missing++;
	return missing;
}

// How many pairs of objects overlap on screen (one partly hides the other).
int layout::count_hidden()const{
	int count = 0;
	for(size_t i = 0;i<placed.size();i++){
		for(size_t j = i + 1;j<placed.size();j++){
			seen a = look(int(i)), b = look(int(j));
			if(a.depth <= 0.0f || b.depth <= 0.0f) continue;
			float dx = a.x - b.x, dy = a.y - b.y;
			if(std::sqrt(dx * dx + dy * dy) < a.radius + b.radius) count++;
		}
	}
	return count;
}

// How many objects aren't completely inside the picture.
// The screen edges are at x = ±tan(fov_x / 2) and y = ±tan(fov_y / 2) in
// look()'s units, so an object's circle is inside when its center plus its
// radius stays within them, on both axes (docs/15).
bool layout::in_picture(const vec3& p,float r,float edge_scale,float extra_px)const{
	camera_axes a = axes_of(eye, target);
	vec3 v = p - eye;
	float depth = dot(v, a.forward);
	if(depth <= 0.0f) return false;
	float x = dot(v, a.right) / depth;
	float y = dot(v, a.up) / depth;
	float edge_y = std::tan(fov_y / 2.0f) * edge_scale;
	float edge_x = std::tan(fov_y / 2.0f) * aspect * edge_scale;
	float rr = r / depth + extra_px * px_to_tan();
	float top = edge_y - top_band_px * px_to_tan();         // nothing under the titles (docs/31)
	return std::fabs(x) + rr <= edge_x && y + rr <= top && -y + rr <= edge_y;
}

int layout::count_off_screen()const{
	int count = 0;
	for(const placement& p : placed){
		if(!in_picture(p.position, p.radius)) count++;
	}
	return count;
}

void layout::include_in_frame(const vec3& center,float radius){
	extra_bounds.push_back({center, radius});
}

void layout::reframe(){
	if(framed) frame();
}

// ---------------------------------------------------------------------------
//  Step C: refinement by gradient descent.
//
//  The energy E is a number that's big when the layout is bad and 0 when
//  it's perfect. It has four parts (docs/13 works through each one):
//
//    spring   : beta  · |p - home|²                  (stay near the greedy spot)
//    push     : gamma · max(0, r_i + r_j + gap - d)² (every pair of objects)
//    relations: rho   · max(0, how far it's broken)² (every relation)
//    screen   : nu    · max(0, a_i + a_j + margin - s)²
//               (every pair, s = distance between their circles on screen)
//
//  The gradient of E says, for every object, which way makes E grow
//  fastest. Stepping the other way (p -= eta · gradient) makes E smaller.
//  Repeat, and the layout settles where all the pushes and pulls balance.
// ---------------------------------------------------------------------------

layout::energy_parts layout::energy(const std::vector<vec3>& home)const{
	energy_parts e;
	int n = int(placed.size());
	for(int i = 0;i<n;i++){
		vec3 d = placed[i].position - home[i];
		e.spring += spring_weight * dot(d, d);
	}
	for(int i = 0;i<n;i++){
		for(int j = i + 1;j<n;j++){
			float need = placed[i].radius + placed[j].radius + gap;
			float p = std::max(0.0f, need - distance(placed[i].position, placed[j].position));
			e.push += push_weight * p * p;

			seen a = look(i, true), b = look(j, true);
			if(a.depth > 0.0f && b.depth > 0.0f){
				float dx = a.x - b.x, dy = a.y - b.y;
				float s = std::sqrt(dx * dx + dy * dy);
				float q = std::max(0.0f, a.radius + b.radius + screen_margin - s);
				e.screen += screen_weight * q * q;
			}
		}
	}
	for(int i = 0;i<n;i++){
		for(const link& l : links[i]){
			const placement& a = placed[i];
			const placement& b = placed[l.other];
			float broken = 0.0f;
			if(l.kind == relation_kind::near){
				// too far: surfaces more than near_limit apart
				broken = std::max(0.0f, distance(a.position, b.position) - a.radius - b.radius - near_limit);
			}else{
				// not far enough that way: along should be at least reach
				float along = dot(a.position - b.position, direction_of(l.kind));
				broken = std::max(0.0f, a.radius + b.radius - along);
			}
			e.relations += relation_weight * broken * broken;
		}
	}
	return e;
}

std::vector<vec3> layout::gradient(const std::vector<vec3>& home)const{
	int n = int(placed.size());
	std::vector<vec3> g(n, vec3(0.0f, 0.0f, 0.0f));
	camera_axes cam = axes_of(eye, target);

	// spring: d/dp of beta·|p - home|² = 2·beta·(p - home)
	for(int i = 0;i<n;i++){
		g[i] = g[i] + (placed[i].position - home[i]) * (2.0f * spring_weight);
	}
	for(int i = 0;i<n;i++){
		for(int j = i + 1;j<n;j++){
			// push: E = gamma·(need - d)², d = |p_i - p_j|
			// dE/dp_i = -2·gamma·(need - d)·(p_i - p_j)/d   (and the opposite for j)
			vec3 diff = placed[i].position - placed[j].position;
			float d = std::sqrt(dot(diff, diff));
			float need = placed[i].radius + placed[j].radius + gap;
			if(d < need && d > 1e-6f){
				vec3 away = diff * (1.0f / d);   // unit arrow from j to i
				vec3 push = away * (-2.0f * push_weight * (need - d));
				g[i] = g[i] + push;
				g[j] = g[j] - push;
			}

			// screen: same shape as push, but on the screen. Moving p_i by a
			// small step changes its screen spot by (right·dx + up·dy)/depth,
			// so a screen direction (qx, qy) is the world direction
			// (right·qx + up·qy), scaled by 1/depth. (Depth is treated as
			// fixed here: objects mostly slide sideways.)
			seen a = look(i, true), b = look(j, true);
			if(a.depth > 0.0f && b.depth > 0.0f){
				float qx = a.x - b.x, qy = a.y - b.y;
				float s = std::sqrt(qx * qx + qy * qy);
				float need_s = a.radius + b.radius + screen_margin;
				if(s < need_s && s > 1e-6f){
					vec3 sideways = (cam.right * (qx / s) + cam.up * (qy / s));  // screen arrow j -> i, in world
					float amount = -2.0f * screen_weight * (need_s - s);
					g[i] = g[i] + sideways * (amount / a.depth);
					g[j] = g[j] - sideways * (amount / b.depth);
				}
			}
		}
	}
	for(int i = 0;i<n;i++){
		for(const link& l : links[i]){
			const placement& a = placed[i];
			const placement& b = placed[l.other];
			if(l.kind == relation_kind::near){
				// E = rho·(d - r_a - r_b - limit)² when too far: pulls them together
				vec3 diff = a.position - b.position;
				float d = std::sqrt(dot(diff, diff));
				float broken = d - a.radius - b.radius - near_limit;
				if(broken > 0.0f && d > 1e-6f){
					vec3 pull = diff * (2.0f * relation_weight * broken / d);
					g[i] = g[i] + pull;
					g[l.other] = g[l.other] - pull;
				}
			}else{
				// E = rho·(reach - along)², along = dot(p_a - p_b, dir)
				// dE/dp_a = -2·rho·(reach - along)·dir   (and the opposite for b)
				vec3 dir = direction_of(l.kind);
				float broken = a.radius + b.radius - dot(a.position - b.position, dir);
				if(broken > 0.0f){
					vec3 push = dir * (-2.0f * relation_weight * broken);
					g[i] = g[i] + push;
					g[l.other] = g[l.other] - push;
				}
			}
		}
	}
	return g;
}

void layout::refine(){
	std::vector<vec3> home;   // where greedy put everything: the springs' rest spots
	for(const placement& p : placed) home.push_back(p.position);
	int anchor = order.empty() ? -1 : order[0];   // the main object stays in the middle

	auto log = [&](int step){
		energy_parts e = energy(home);
		std::ostringstream line;
		line << std::fixed << std::setprecision(4)
		     << "step " << std::setw(3) << step << "   E = " << std::setw(8) << e.total()
		     << "   (spring " << e.spring << ", push " << e.push
		     << ", relations " << e.relations << ", screen " << e.screen << ")";
		energy_log.push_back(line.str());
	};

	log(0);
	// Backtracking: try a step of size eta. If the energy went UP, the step
	// jumped over the bottom of the valley, so undo it and try half the
	// size. After a step that worked, let eta grow a little again. This way
	// the energy can never go up, however steep the landscape is (a camera
	// close to the scene makes the screen term much steeper, docs/14).
	float eta = step_size;
	float before = energy(home).total();
	int shortened = 0;   // how many times a step had to be halved
	for(int step = 1;step<=steps;step++){
		std::vector<vec3> g = gradient(home);
		std::vector<vec3> old;
		for(const placement& p : placed) old.push_back(p.position);

		for(int attempt = 0;attempt<20;attempt++){
			for(size_t i = 0;i<placed.size();i++){
				if(int(i) == anchor) continue;
				placed[i].position = old[i] - g[i] * eta;   // downhill
			}
			float after = energy(home).total();
			// Only ever accept a step that didn't make things worse. (An
			// earlier version allowed a tiny rise for float rounding, but
			// those tiny rises could add up over hundreds of steps; see
			// docs/15. Near the bottom, rounding noise now just makes the
			// step shrink, which does no harm.)
			if(after <= before){
				before = after;
				eta = std::min(eta * 1.25f, 4.0f * step_size);
				break;
			}
			eta *= 0.5f;
			shortened++;
			if(attempt == 19){
				// no step helps at all: we're at the bottom, stay put
				for(size_t i = 0;i<placed.size();i++) placed[i].position = old[i];
			}
		}
		if(step == 1 || step == 2 || step % 50 == 0) log(step);
	}
	halved += shortened;
	energy_log.push_back("steps halved because they went uphill: " + std::to_string(shortened));
	separate();
}

// The energy's terms are all soft: they trade off against each other. When
// relations can't all be true (a cycle like "a left_of b left_of c left_of
// a"), their pull can win against the overlap push and leave objects inside
// each other. Overlapping is never acceptable, though, so after refining,
// every pair that's still too close is pushed straight apart, along the
// line between their centers, until it has `gap` between them. The main
// object stays put; anything else moves. Relations may get weaker, and the
// report says so.
void layout::separate(){
	int anchor = order.empty() ? -1 : order[0];
	int moved = 0;
	for(int round = 0;round<100;round++){
		bool clear = true;
		for(size_t i = 0;i<placed.size();i++){
			for(size_t j = i + 1;j<placed.size();j++){
				vec3 diff = placed[j].position - placed[i].position;
				float d = std::sqrt(dot(diff, diff));
				float need = placed[i].radius + placed[j].radius + gap;
				if(d >= need - 1e-4f) continue;
				clear = false;
				moved++;
				// exactly on top of each other: pick a direction, the same every time
				vec3 away = d > 1e-6f ? diff * (1.0f / d) : vec3(1.0f, 0.0f, 0.0f);
				float short_by = need - d;
				if(int(i) == anchor)      placed[j].position = placed[j].position + away * short_by;
				else if(int(j) == anchor) placed[i].position = placed[i].position - away * short_by;
				else{
					placed[i].position = placed[i].position - away * (short_by / 2.0f);
					placed[j].position = placed[j].position + away * (short_by / 2.0f);
				}
			}
		}
		if(clear) break;
	}
	if(moved > 0){
		warnings.push_back("objects were still overlapping after refining; " + std::to_string(moved)
		                   + " pushes to separate them (the relations couldn't all be true)");
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

layout::metrics layout::measure()const{
	metrics m;
	m.objects    = int(placed.size());
	m.overlaps   = count_overlaps();
	m.hidden     = count_hidden();
	m.off_screen = count_off_screen();
	m.labels_without_room = labels_without_room();
	for(size_t i = 0;i<links.size();i++){
		for(const link& l : links[i]){
			m.relations_total++;
			if(check(int(i), l) == verdict::ok) m.relations_ok++;
		}
	}
	m.errors       = int(errors.size());
	m.warnings     = int(warnings.size());
	m.steps_halved = halved;
	return m;
}

std::string layout::report()const{
	std::ostringstream out;
	out << std::fixed << std::setprecision(2);

	out << "layout (" << method_name << "): " << placed.size() << " objects, "
	    << count_overlaps() << " overlapping pairs, "
	    << count_hidden() << " pairs overlapping on screen\n";
	out << "  objects not fully in the picture: " << count_off_screen() << "\n";
	if(std::any_of(label_px.begin(), label_px.end(), [](float w){ return w > 0.0f; })){
		out << "  labels without room next to their object: " << labels_without_room() << "\n";
	}

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

	if(framed){
		out << "  camera: from " << view_name(spec.view) << ", looking at (" << scene_center[0] << ", "
		    << scene_center[1] << ", " << scene_center[2] << "), scene radius " << scene_radius
		    << ", distance " << camera_distance << " (sphere fit: " << sphere_distance << ")\n";
		out << "          eye at (" << eye[0] << ", " << eye[1] << ", " << eye[2] << ")\n";
	}
	if(!energy_log.empty()){
		out << "  refinement (energy should go down):\n";
		for(const std::string& line : energy_log) out << "    " << line << "\n";
	}
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
