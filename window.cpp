#include "window.h"
#include <SDL3/SDL.h>
#include <stdexcept>


window::window(const std::string& title,int width,int height){
	if(!SDL_Init(SDL_INIT_VIDEO)){
		throw std::runtime_error(std::string("could not start SDL: ") + SDL_GetError());
	}
	if(!SDL_CreateWindowAndRenderer(title.c_str(), width, height, 0, &handle, &painter)){
		std::string error = SDL_GetError();
		SDL_Quit();
		throw std::runtime_error("could not open a window: " + error);
	}
	// vsync: wait for the monitor to be ready before showing the next picture,
	// so we draw at the screen's speed (usually 60 per second) and never show
	// half of one picture and half of the next
	SDL_SetRenderVSync(painter, 1);

	// A texture is an image that lives on the graphics card. STREAMING means
	// we'll replace its pixels every frame. RGBA32 is 4 bytes per pixel in the
	// order r, g, b, a: exactly how px::Pixel is laid out, so no converting.
	screen = SDL_CreateTexture(painter, SDL_PIXELFORMAT_RGBA32,
	                           SDL_TEXTUREACCESS_STREAMING, width, height);
	if(!screen){
		std::string error = SDL_GetError();
		SDL_DestroyRenderer(painter);
		SDL_DestroyWindow(handle);
		SDL_Quit();
		throw std::runtime_error("could not make the screen texture: " + error);
	}
	// on a bigger (e.g. Retina) screen, blow pixels up as sharp squares, not blurry
	SDL_SetTextureScaleMode(screen, SDL_SCALEMODE_NEAREST);

	// A program started from the terminal doesn't always come to the front
	// on macOS, and then the keys keep going to the terminal. Ask for it.
	SDL_RaiseWindow(handle);
}

window::~window(){
	SDL_DestroyTexture(screen);
	SDL_DestroyRenderer(painter);
	SDL_DestroyWindow(handle);
	SDL_Quit();
}

bool window::is_open(){
	// The OS keeps a queue of events (mouse, keys, "close" clicked...).
	// We have to empty it every frame or the OS thinks we've frozen.
	SDL_Event event;
	while(SDL_PollEvent(&event)){
		if(event.type == SDL_EVENT_QUIT) open = false;
		if(event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat){
			if(event.key.key == SDLK_TAB)    tab_pressed = true;
			if(event.key.key == SDLK_ESCAPE) escape_pressed = true;
		}
		// in relative mode these are pure movements, not positions: the
		// mouse can move forever without hitting the edge of the screen
		if(event.type == SDL_EVENT_MOUSE_MOTION && ignore_mouse == 0){
			mouse_x += event.motion.xrel;
			mouse_y += event.motion.yrel;
		}
	}
	if(ignore_mouse > 0) ignore_mouse--;
	return open;
}

controls window::read_controls(){
	// a snapshot of every key: held down right now or not
	const bool* keys = SDL_GetKeyboardState(nullptr);
	controls c;
	c.forward = keys[SDL_SCANCODE_W];
	c.back    = keys[SDL_SCANCODE_S];
	c.left    = keys[SDL_SCANCODE_A];
	c.right   = keys[SDL_SCANCODE_D];
	c.up      = keys[SDL_SCANCODE_SPACE];
	c.down    = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
	c.fast    = keys[SDL_SCANCODE_LCTRL]  || keys[SDL_SCANCODE_RCTRL];
	c.turn_x  = mouse_x;
	c.turn_y  = mouse_y;
	mouse_x = mouse_y = 0.0f;
	return c;
}

bool window::toggled(){
	bool now = tab_pressed;
	tab_pressed = false;
	return now;
}

bool window::escaped(){
	bool now = escape_pressed;
	escape_pressed = false;
	return now;
}

void window::capture_mouse(bool on){
	SDL_SetWindowRelativeMouseMode(handle, on);
	mouse_x = mouse_y = 0.0f;
	// capturing moves the pointer to the middle of the window, and that jump
	// arrives as a mouse movement: without this, the view would jerk sideways
	// the moment you press Tab
	ignore_mouse = 6;   // about 50 ms at 120 frames per second: too short to notice
	escape_pressed = false;
}

void window::show(const px::Image& image){
	// pitch = how many bytes one row of the image takes
	int pitch = image.Width() * int(sizeof(px::Pixel));
	SDL_UpdateTexture(screen, nullptr, image.Data(), pitch);  // our pixels -> graphics card
	SDL_RenderTexture(painter, screen, nullptr, nullptr);     // texture -> whole window
	SDL_RenderPresent(painter);                               // show it (waits for vsync)
}
