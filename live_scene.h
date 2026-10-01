#pragma once
#include "frame_source.h"
#include "motion.h"
#include "world.h"

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// A .dan file, kept up to date while it plays (docs/22).
//
// Every half second it looks at the file's "last changed" time. When that
// changes (the AI, or you, saved a new version), it reads the file again,
// solves it, and swaps the new scene in. If the new version has mistakes,
// the last good scene stays on screen.
//
// Every time it reads the file, it writes   <file>.report   next to it:
// the parser's errors and notes, then the solver's report. That's what the
// AI reads to see how its scene turned out.
class live_scene : public frame_source{
public:
	// look = the picture every loaded scene makes: its size, and whether the
	// bounding circles are drawn (docs/34)
	live_scene(const std::string& path,motion_plan::method how,const camera& look = camera());

	camera& cam() override;
	const std::vector<object*>& objects() override;
	float seconds() override;
	void poll() override;            // checks the file, at most every 0.5 s
	void draw_overlays(render& renderer,float t) override;

	// Check the file right now (the tests use this, so they don't have to wait).
	// Returns true if a new scene was swapped in.
	bool check_now();
	bool has_scene()const;            // false until the file has been read without errors once
	const std::string& last_report()const;
	std::string report_path()const;   // <file>.report

private:
	bool load();                      // read, solve, swap; writes the report

	std::string path;
	motion_plan::method how;
	std::filesystem::file_time_type stamp{};   // the file's time when we last read it
	std::chrono::steady_clock::time_point last_check;

	std::unique_ptr<world> current;   // the scene on screen (owned: replaced on reload)
	std::vector<object*> pointers;
	camera empty_camera;              // shown until there's a scene; also the size and circles for every scene
	std::string report;
};
