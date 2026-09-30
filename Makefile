CXX      ?= clang++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2

# SDL3 only opens the window and shows our finished picture.
# pkg-config knows where brew put it: brew install sdl3
SDL_CFLAGS = $(shell pkg-config --cflags sdl3)
SDL_LIBS   = $(shell pkg-config --libs sdl3)

TARGET  = main
SOURCES = main.cpp camera.cpp mesh.cpp object.cpp player.cpp render.cpp vec3.cpp window.cpp
HEADERS = $(wildcard *.h)

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) $(SOURCES) -o $(TARGET) $(SDL_LIBS)

# Build and play a video in a window: make run, or make run SCENE=2
SCENE ?= 1
run: $(TARGET)
	./$(TARGET) $(SCENE)

clean:
	rm -f $(TARGET)
	rm -rf renders out
