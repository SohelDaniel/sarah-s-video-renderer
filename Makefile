CXX      ?= clang++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2

# SDL3 only opens the window and shows our finished picture.
# pkg-config knows where brew put it: brew install sdl3
SDL_CFLAGS = $(shell pkg-config --cflags sdl3)
SDL_LIBS   = $(shell pkg-config --libs sdl3)

TARGET  = main
SOURCES = main.cpp camera.cpp layout.cpp mesh.cpp object.cpp player.cpp render.cpp vec3.cpp window.cpp world.cpp
HEADERS = $(wildcard *.h)

.PHONY: all run stills clean

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) $(SOURCES) -o $(TARGET) $(SDL_LIBS)

# Build and play a video in a window: make run, or make run SCENE=2
SCENE ?= 1
run: $(TARGET)
	./$(TARGET) $(SCENE)

# The pictures in docs/images: scene 3 after each step of the layout solver.
STEPS = naive greedy
stills: $(TARGET)
	@mkdir -p docs/images
	@for s in $(STEPS); do ./$(TARGET) 3 $$s docs/images/scene3-$$s.png > docs/images/scene3-$$s.txt; done

clean:
	rm -f $(TARGET)
	rm -rf renders out
