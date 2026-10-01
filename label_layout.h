#pragma once
#include "point2.h"
#include <string>
#include <vector>

// ============================================================================
//  Placing text labels next to objects on the screen (docs/28).
//
//  This is the problem this whole project started from: labels on a map
//  must sit next to their point without covering each other or anything
//  else (Christensen, Marks & Shieber 1995). It works in screen pixels,
//  every frame, in the steps of the original notes:
//    1. skip what isn't visible
//    2. priority: labels near the middle of the screen pick first
//    3. candidate boxes: right, left, above, below the object
//    4. the box overlap test
//    5. greedy: each label takes the first candidate that's free
//    6. a couple of gradient steps: a spring home, a push from obstacles
//    7. the stuck check: still overlapping? back to the plain spot
//    8. next frame starts from where this one ended, and keeps its side
//       if it still works, so labels don't flicker (Vaaraniemi et al. 2012)
//
//  Pure 2D math, no drawing, so it can be tested on its own.
// ============================================================================

// a box on the screen: [x0, x1] x [y0, y1], in pixels (y goes down)
struct box2{
	float x0, x1, y0, y1;
	point2 center()const{ return {(x0 + x1) / 2.0f, (y0 + y1) / 2.0f}; }
};

// The test from the notes: P and Q overlap only if all four are true.
// One false and they don't touch.
bool overlap(const box2& p,const box2& q);

// One label to place: its size, and the object it belongs to, as the
// circle the object covers on screen.
struct label_request{
	float width, height;      // the label's size in pixels
	point2 anchor;            // the object's center on screen
	float radius;             // the object's circle on screen
	bool visible = true;      // in front of the camera and in the picture
	bool always = false;      // never hidden: placed first, and shown even if crowded
};

struct placed_label{
	bool shown = false;
	box2 where{0, 0, 0, 0};
	int side = -1;            // 0 right, 1 left, 2 above, 3 below
	bool leader = false;      // pushed away from its object: draw a thin line to it
};

class label_layout{
public:
	label_layout(int width,int height);

	// One frame. requests[i] is the same object's label every frame (the
	// order must not change), so last frame can be remembered.
	const std::vector<placed_label>& place(const std::vector<label_request>& requests);

	// how many times a label switched sides, over all frames so far (tests)
	int side_changes()const;

	// Step 3: a candidate box, `side` of the anchor's circle, `distance`
	// pixels of empty space between the circle and the box.
	static box2 candidate(const label_request& r,int side,float distance);

	// Step 6, one step of gradient descent on the energy from the notes:
	//   E(p) = beta·|p − home|² + gamma·(R − d)²     (the push only when d < R)
	//   d = |p − obstacle|
	// Returns the new p. Public so the tests can replay the notes' example.
	static point2 gradient_step(point2 p,point2 home,point2 obstacle,float R,
	                            float beta = 1.0f,float gamma = 1.0f,float eta = 0.1f);

	// keep last frame's side when it still works (step 8); off only for the
	// test that shows why it's there
	bool hysteresis = true;
	// places no label may go, like the band where the titles are (docs/31)
	std::vector<box2> keep_out;

	static constexpr float gap = 3.0f;           // between an object's circle and its label
	static constexpr int gradient_steps = 2;     // per frame (step 9: it's already close)

private:
	bool free_at(const box2& b,size_t self,const std::vector<label_request>& requests,
	             const std::vector<placed_label>& placed)const;
	int crowding(const box2& b,size_t self,const std::vector<label_request>& requests,
	             const std::vector<placed_label>& placed)const;

	int width, height;
	std::vector<placed_label> current;
	std::vector<placed_label> previous;
	int changes = 0;
};
