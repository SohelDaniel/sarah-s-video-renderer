#pragma once
#include "scene_spec.h"

#include <string>
#include <vector>

// ============================================================================
//  Scenes for testing the layout solver, written the way a sloppy AI might
//  write them: crowded, contradictory, full of typos, or just odd. Shared by
//  the stress tests (solver_test.cpp) and by `./main stress <name>`.
// ============================================================================

struct test_scene{
	std::string name;
	std::string attacks;      // what it's trying to break
	scene_spec spec;
	int expected_errors = 0;  // mistakes the solver must report (at least this many)
};

// The scene from docs/11-14 (scene 3).
scene_spec lazy_ai_scene();
// The moving scene from docs/16 (scene 4).
scene_spec lazy_motion_scene();
// Things that collide on purpose, from docs/18 (scene 5).
scene_spec impact_scene();

// Every test scene, in a fixed order.
std::vector<test_scene> all_test_scenes();
