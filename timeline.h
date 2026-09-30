#pragma once
#include <algorithm>
#include <functional>
#include <vector>
#include "vec3.h"

// ============================================================================
//  Things that happen over time.
//
//  main() schedules changes like "rotate to 1.6 between second 2 and 5".
//  Every frame we ask: at time t, how far through each change are we?
//  That fraction (0..1) is used to blend from the old value to the new one.
// ============================================================================


// Linear interpolation ("lerp"): f = 0 gives `from`, f = 1 gives `to`,
// f = 0.5 is exactly halfway.
inline float lerp(float from,float to,float f){
	return from + (to - from) * f;
}
inline vec3 lerp(const vec3& from,const vec3& to,float f){
	return from + (to - from) * f;
}

// A stretch of time in seconds, e.g. from second 3 to second 6.
struct time_span{
	float start = 0.0f;
	float end   = 0.0f;

	// How far through the span we are at time t: 0 = not started, 1 = done.
	//   start = 3, end = 6
	//   t = 2   ->  0     (hasn't started)
	//   t = 4.5 ->  0.5   (halfway)
	//   t = 9   ->  1     (finished, stays at the end value)
	float progress(float t)const{
		if(end <= start) return t >= start ? 1.0f : 0.0f; // no length: jump
		float f = (t - start) / (end - start);
		return std::clamp(f, 0.0f, 1.0f);
	}
};

// Everything that happens to one State (an object's pose, a camera's
// viewpoint) over the whole video.
//
// To get the state at time t we don't remember anything from last frame.
// We start again from `initial` and replay every change in the order they
// begin, each one only as far as it has gotten by time t:
//   - not started yet -> progress 0 -> changes nothing
//   - finished        -> progress 1 -> fully applied
//   - running         -> partly applied
// Because each change blends from whatever the state is when its turn comes,
// a rotation at second 3 automatically starts from where an earlier one ended.
template<typename State>
class timeline{
public:
	// One change: given the state so far and the progress (0..1), update it.
	using change = std::function<void(State& state,float progress)>;

	State initial;  // how things are at second 0, before anything happens

	void add(time_span when,change what){
		steps.push_back({when, std::move(what)});
		// keep them in the order they begin, so a move at 3s is replayed
		// before a move at 10s even if main() wrote them the other way round
		std::stable_sort(steps.begin(), steps.end(),
			[](const step& a,const step& b){ return a.when.start < b.when.start; });
	}

	State at(float t)const{
		State state = initial;
		for(const step& s : steps){
			s.what(state, s.when.progress(t));
		}
		return state;
	}

private:
	struct step{
		time_span when;
		change what;
	};
	std::vector<step> steps;
};
