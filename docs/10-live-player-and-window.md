# 10 · Live player and window

How a scene becomes a video playing in a window, about 120 times a second.

Code: `player.h/.cpp` (the loop), `window.h/.cpp` (the only file that uses SDL).

---

## 1. The frame loop

Every game and real-time graphics program has this loop:

```cpp
while(screen.is_open()){
	t = seconds since start;
	if(t > seconds) break;

	cam.update(t);                         // 1. where is everything at time t? (09)
	for(object* o : scene) o->update(t);

	renderer.begin(cam);                   // 2. wipe image + depth buffer (08)
	for(const object* o : scene) o->draw(renderer);   // 3. rasterize (03-08)

	screen.show(renderer.picture());       // 4. put it on screen
}
```

Every frame is drawn **from scratch**. The motion comes from t being a
little bigger each time.

## 2. Real time, not a frame count

If you instead added 1/60 s per frame, then:

- a computer that only manages 30 frames per second plays in **slow motion**
- a 120 Hz screen plays at **double speed**

Asking the clock how much time has really passed keeps the video exactly 12
seconds long everywhere. A slow machine just shows fewer frames. This is
called being **frame-rate independent**.

`std::chrono::steady_clock` is used because it only ever goes forward. The
wall clock (`system_clock`) can jump when the computer syncs its time.

## 3. Numbers from this Mac

| Scene | Triangles | Time to draw one frame |
|---|---|---|
| 1 | 12 + 960 + 1024 = 1996 | ~1.15 ms |
| 2 | 960 + 20 + 8 + 1024 + 6 = 2018 | ~0.55 ms |

A frame at 120 Hz has 8.3 ms, so drawing uses about an eighth of it.
Scene 2 is faster with the same triangle count because its objects cover
fewer pixels: a rasterizer's cost follows **pixels filled**, not only
triangles.

The loop runs at 120 fps because of vsync (below), not because of the
drawing speed.

## 4. The window (SDL)

C++ can't open a window by itself; only the operating system can. SDL is a
library that asks it for us the same way on Mac, Linux and Windows. **It
draws nothing.** It does three jobs:

1. open a window
2. take our finished pixel array and put it on screen
3. tell us when the close button is clicked

| SDL thing | Our name | What it is |
|---|---|---|
| `SDL_Window` | `handle` | the window |
| `SDL_Renderer` | `painter` | copies images onto the window |
| `SDL_Texture` | `screen` | an image stored on the graphics card |

Every frame:

```cpp
SDL_UpdateTexture(screen, nullptr, image.Data(), pitch);  // our pixels → graphics card
SDL_RenderTexture(painter, screen, nullptr, nullptr);     // stretch over the window
SDL_RenderPresent(painter);                               // show it (waits for vsync)
```

The graphics card is only used as a **display**. Every pixel's color was
decided by our code in `render.cpp`.

### Pixel format and pitch

`px::Pixel` is 4 bytes: r, g, b, a (the `static_assert` in `pixel.h` checks
this). `SDL_PIXELFORMAT_RGBA32` is the same layout, so the image is copied
as one block with no converting.

**pitch** = bytes per row = 640 · 4 = **2560**.

### The event queue

The OS sends a stream of events (mouse, keys, close button).
`is_open()` empties it every frame. A program that stops reading its events
looks frozen to macOS (the spinning beach ball).

### vsync and double buffering

There are two pictures: the one on screen and the one being prepared.
`SDL_RenderPresent` swaps them at the moment the screen refreshes
(**vsync**), so you never see half of one frame and half of the next
(**tearing**). It also paces the loop to the screen's refresh rate.

### RAII

The window opens in the constructor and closes in the destructor. Copying is
deleted (`= delete`), because two copies would close the same window twice.
See [C++ notes](cpp-notes.md).

---

## Try it on paper

1. The screen refreshes 60 times a second. How long does each frame have?
2. What's the pitch of a 1280×720 image?

Answers: 1. 1000 / 60 = 16.7 ms · 2. 1280 · 4 = 5120 bytes
