#pragma once
#include "fly_camera.h"
#include "pixel.h"
#include <string>

// Only mentions of SDL's types, so files that include window.h don't need
// SDL at all. window.cpp is the ONLY file that talks to SDL.
struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

// A window on screen that shows a px::Image. That's all it does: it never
// draws a triangle or a pixel itself. Every pixel comes from our own
// rasterizer; the window just copies the finished picture onto the screen.
class window{
public:
	// Opens the window. Throws if the OS won't give us one.
	window(const std::string& title,int width,int height);
	// Closes it again (RAII: the window lives exactly as long as this object).
	~window();

	// One window object = one real window. A copy would close it twice.
	window(const window&) = delete;
	window& operator=(const window&) = delete;

	// Handle what the OS sent us since last frame (like the close button).
	// Returns false once the user has closed the window.
	bool is_open();

	// The keys held right now, and how far the mouse moved since last time
	// (docs/20). Only means something while the mouse is captured.
	controls read_controls();
	// Was Tab pressed since last time? (switches between the scripted and
	// the free camera)
	bool toggled();
	// Was Esc pressed since last time? (leaves the free camera, gives the mouse back)
	bool escaped();
	// Capture the mouse (hidden, unlimited movement) or let it go.
	void capture_mouse(bool on);

	// Put this picture on screen. Must be the same size as the window.
	void show(const px::Image& image);

private:
	SDL_Window*   handle  = nullptr;  // the window itself
	SDL_Renderer* painter = nullptr;  // copies images onto the window
	SDL_Texture*  screen  = nullptr;  // the picture, uploaded to the graphics card
	bool open = true;
	bool tab_pressed = false;
	bool escape_pressed = false;
	float mouse_x = 0.0f, mouse_y = 0.0f;   // mouse movement collected since last read
	int ignore_mouse = 0;                   // frames of mouse movement to throw away (see capture_mouse)
};
