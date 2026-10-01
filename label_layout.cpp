#include "label_layout.h"

#include <algorithm>
#include <cmath>
#include <numeric>


bool overlap(const box2& p,const box2& q){
	return p.x0 < q.x1 && p.x1 > q.x0 && p.y0 < q.y1 && p.y1 > q.y0;
}

label_layout::label_layout(int width,int height)
	: width(width), height(height){}

int label_layout::side_changes()const{
	return changes;
}

// Step 3. The offset from the object's center to the label's edge is its
// screen radius plus the gap (r + g in the notes):
//   right: [x + r + g, x + r + g + w] x [y − h/2, y + h/2]     left: the mirror image
//   above: centered on x, ending r + g above the center         below: the mirror image
box2 label_layout::candidate(const label_request& r,int side,float distance){
	float off = r.radius + distance;
	float x = r.anchor.x, y = r.anchor.y, w = r.width, h = r.height;
	switch(side){
		case 0:  return {x + off, x + off + w, y - h / 2.0f, y + h / 2.0f};   // right
		case 1:  return {x - off - w, x - off, y - h / 2.0f, y + h / 2.0f};   // left
		case 2:  return {x - w / 2.0f, x + w / 2.0f, y - off - h, y - off};   // above
		default: return {x - w / 2.0f, x + w / 2.0f, y + off, y + off + h};   // below
	}
}

// Step 6. The gradient of E (worked out in the notes, and in docs/28):
//   ∇E = 2·beta·(p − home) − 2·gamma·(R − d)·(p − obstacle)/d
point2 label_layout::gradient_step(point2 p,point2 home,point2 obstacle,float R,
                                   float beta,float gamma,float eta){
	float gx = 2.0f * beta * (p.x - home.x);
	float gy = 2.0f * beta * (p.y - home.y);
	float ox = p.x - obstacle.x, oy = p.y - obstacle.y;
	float d = std::sqrt(ox * ox + oy * oy);
	if(d < R && d > 1e-6f){
		gx -= 2.0f * gamma * (R - d) * ox / d;   // (p − o)/d: the unit arrow away from the obstacle
		gy -= 2.0f * gamma * (R - d) * oy / d;
	}
	return {p.x - eta * gx, p.y - eta * gy};
}

// Step 4 for one box: inside the picture, not on any label placed so far,
// and not on any object's circle (as its square box, like the dot boxes in
// the notes), except its own.
bool label_layout::free_at(const box2& b,size_t self,const std::vector<label_request>& requests,
                           const std::vector<placed_label>& placed)const{
	if(b.x0 < 0 || b.y0 < 0 || b.x1 > width || b.y1 > height) return false;
	for(size_t j = 0;j<placed.size();j++){
		if(j != self && placed[j].shown && overlap(b, placed[j].where)) return false;
	}
	for(size_t j = 0;j<requests.size();j++){
		if(j == self || !requests[j].visible) continue;
		const label_request& o = requests[j];
		box2 dot{o.anchor.x - o.radius, o.anchor.x + o.radius, o.anchor.y - o.radius, o.anchor.y + o.radius};
		if(overlap(b, dot)) return false;
	}
	return true;
}

// How many things a box overlaps, and whether it leaves the picture (that
// counts as 2, so labels stay on screen if they possibly can).
int label_layout::crowding(const box2& b,size_t self,const std::vector<label_request>& requests,
                           const std::vector<placed_label>& placed)const{
	int count = 0;
	if(b.x0 < 0 || b.y0 < 0 || b.x1 > width || b.y1 > height) count += 2;
	for(size_t j = 0;j<placed.size();j++){
		if(j != self && placed[j].shown && overlap(b, placed[j].where)) count++;
	}
	for(size_t j = 0;j<requests.size();j++){
		if(j == self || !requests[j].visible) continue;
		const label_request& o = requests[j];
		box2 dot{o.anchor.x - o.radius, o.anchor.x + o.radius, o.anchor.y - o.radius, o.anchor.y + o.radius};
		if(overlap(b, dot)) count++;
	}
	return count;
}

const std::vector<placed_label>& label_layout::place(const std::vector<label_request>& requests){
	size_t n = requests.size();
	previous = current;
	previous.resize(n);
	current.assign(n, placed_label{});

	// Step 2: priority from how close to the middle of the screen it is
	//   w = 1 / (1 + d / D)        D = a twentieth of the screen width
	// (the notes used D = 10 on a 200-wide screen)
	float D = width / 20.0f;
	std::vector<float> priority(n, 0.0f);
	for(size_t i = 0;i<n;i++){
		float dx = requests[i].anchor.x - width / 2.0f, dy = requests[i].anchor.y - height / 2.0f;
		priority[i] = 1.0f / (1.0f + std::sqrt(dx * dx + dy * dy) / D);
	}
	std::vector<size_t> order(n);
	std::iota(order.begin(), order.end(), 0);
	// always-on labels first (docs/28), then by priority
	std::stable_sort(order.begin(), order.end(), [&](size_t a,size_t b){
		if(requests[a].always != requests[b].always) return requests[a].always;
		return priority[a] > priority[b];
	});

	for(size_t i : order){
		const label_request& r = requests[i];
		if(!r.visible) continue;                            // step 1

		// Step 5, greedy, with step 8's hysteresis: try last frame's side
		// first, then right, left, above, below. Then the same 3x further
		// out, with a leader line.
		std::vector<int> sides;
		if(hysteresis && previous[i].shown && previous[i].side >= 0) sides.push_back(previous[i].side);
		for(int s = 0;s<4;s++) if(std::find(sides.begin(), sides.end(), s) == sides.end()) sides.push_back(s);

		for(float distance : {gap, 3.0f * gap + r.height}){
			for(int side : sides){
				box2 b = candidate(r, side, distance);
				if(!free_at(b, i, requests, current)) continue;
				current[i] = {true, b, side, distance > gap};
				break;
			}
			if(current[i].shown) break;
		}
		if(!current[i].shown && r.always){
			// an always-on label is never hidden: take the side (next to the
			// object) that overlaps the fewest things
			int best = -1, least = 1 << 30;
			for(int side : sides){
				int c = crowding(candidate(r, side, gap), i, requests, current);
				if(c < least){ least = c; best = side; }
			}
			current[i] = {true, candidate(r, best, gap), best, false};
		}
		if(!current[i].shown) continue;                     // nowhere to go: hidden this frame

		// Step 6: a couple of gradient steps away from the nearest other
		// object, if it's within reach (R = its radius + 10 pixels of
		// clearance), with a spring back to the candidate spot ("home").
		// Step 8: if it had the same side last frame, start from where it
		// ended then, so it moves smoothly instead of jumping.
		box2 home = current[i].where;
		point2 s = home.center();
		point2 p = s;
		if(hysteresis && previous[i].shown && previous[i].side == current[i].side){
			point2 last = previous[i].where.center();
			// only if last frame's spot is still close (the object didn't jump)
			if(std::fabs(last.x - s.x) < r.width && std::fabs(last.y - s.y) < r.height) p = last;
		}
		for(int step = 0;step<gradient_steps;step++){
			float best = 1e9f;
			point2 nearest{0, 0};
			float reach = 0.0f;
			for(size_t j = 0;j<n;j++){
				if(j == i || !requests[j].visible) continue;
				float dx = p.x - requests[j].anchor.x, dy = p.y - requests[j].anchor.y;
				float d = std::sqrt(dx * dx + dy * dy);
				if(d < best){ best = d; nearest = requests[j].anchor; reach = requests[j].radius + 10.0f; }
			}
			p = gradient_step(p, s, nearest, best < 1e9f ? reach : 0.0f);
		}
		box2 moved{p.x - r.width / 2.0f, p.x + r.width / 2.0f, p.y - r.height / 2.0f, p.y + r.height / 2.0f};

		// Step 7, the stuck check: if the nudged box overlaps anything, keep
		// the plain candidate spot instead
		if(free_at(moved, i, requests, current)) current[i].where = moved;

		if(previous[i].shown && previous[i].side != current[i].side) changes++;
	}
	return current;
}
