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
	}
	return open;
}

void window::show(const px::Image& image){
	// pitch = how many bytes one row of the image takes
	int pitch = image.Width() * int(sizeof(px::Pixel));
	SDL_UpdateTexture(screen, nullptr, image.Data(), pitch);  // our pixels -> graphics card
	SDL_RenderTexture(painter, screen, nullptr, nullptr);     // texture -> whole window
	SDL_RenderPresent(painter);                               // show it (waits for vsync)
}
