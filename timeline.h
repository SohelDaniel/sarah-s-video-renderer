#pragma once
#include <algorithm>
#include <cmath>
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

// Looping (docs/20): real time t, a video `duration` seconds long, played
// over and over. fmod is the remainder of a division for floats:
//   loop_time(25, 20) = 5      loop_time(40, 20) = 0      loop_time(7, 20) = 7
inline float loop_time(float t,float duration){
	if(duration <= 0.0f) return 0.0f;
	return std::fmod(t, duration);
}

// ---------------------------------------------------------------------------
//  Easing (docs/23): how a change speeds up and slows down.
//
//  progress f goes from 0 to 1 at a steady pace. A rate reshapes it:
//  "smooth" starts slowly, speeds up in the middle and settles gently at the
//  end. Always f(0) = 0; every rate except there_and_back ends at f(1) = 1.
//  These are Manim's rate functions (manim/utils/rate_functions.py).
// ---------------------------------------------------------------------------
enum class rate{ linear, smooth, sine, rush_into, rush_from, there_and_back };

inline const char* rate_name(rate r){
	switch(r){
		case rate::linear:         return "linear";
		case rate::smooth:         return "smooth";
		case rate::sine:           return "sine";
		case rate::rush_into:      return "rush_into";
		case rate::rush_from:      return "rush_from";
		case rate::there_and_back: return "there_and_back";
	}
	return "?";
}

inline float sigmoid(float x){
	return 1.0f / (1.0f + std::exp(-x));
}

// Manim's smooth: an S-shaped sigmoid, squeezed so it goes exactly from
// (0, 0) to (1, 1). 10 sets how sharp the S is.
//   smooth(t) = ( σ(10·(t − ½)) − σ(−5) ) / ( 1 − 2·σ(−5) )
inline float smooth_curve(float t){
	float lowest = sigmoid(-5.0f);
	return std::clamp((sigmoid(10.0f * (t - 0.5f)) - lowest) / (1.0f - 2.0f * lowest), 0.0f, 1.0f);
}

inline float shape(rate r,float f){
	const float pi = 3.14159265f;
	switch(r){
		case rate::linear:         return f;
		case rate::smooth:         return smooth_curve(f);
		case rate::sine:           return -(std::cos(pi * f) - 1.0f) / 2.0f;          // ease in and out, gentler
		case rate::rush_into:      return 2.0f * smooth_curve(f / 2.0f);              // slow start, fast end
		case rate::rush_from:      return 2.0f * smooth_curve(f / 2.0f + 0.5f) - 1.0f; // fast start, slow end
		case rate::there_and_back: return smooth_curve(f < 0.5f ? 2.0f * f : 2.0f * (1.0f - f));  // out and back again
	}
	return f;
}

// The steepest the curve ever gets: how much faster than steady it moves at
// its fastest moment (linear = 1, smooth ≈ 2.53). Measured by looking at
// 1000 small steps. The collision checks need it (docs/23).
inline float steepest(rate r){
	if(r == rate::linear) return 1.0f;   // steady by definition (measuring gives 1.00004)
	float most = 0.0f;
	const int n = 1000;
	for(int k = 1;k<=n;k++){
		float slope = (shape(r, float(k) / n) - shape(r, float(k - 1) / n)) * n;
		most = std::max(most, std::fabs(slope));
	}
	return most;
}

// A stretch of time in seconds, e.g. from second 3 to second 6.
struct time_span{
	float start = 0.0f;
	float end   = 0.0f;
	rate how = rate::linear;   // how it eases (docs/23)

	// How far through the span we are at time t: 0 = not started, 1 = done.
	//   start = 3, end = 6
	//   t = 2   ->  0     (hasn't started)
	//   t = 4.5 ->  0.5   (halfway)
	//   t = 9   ->  1     (finished, stays at the end value)
	float progress(float t)const{
		if(end <= start) return t >= start ? shape(how, 1.0f) : 0.0f; // no length: jump
		float f = (t - start) / (end - start);
		return shape(how, std::clamp(f, 0.0f, 1.0f));
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
