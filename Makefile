CXX      ?= clang++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2

# SDL3 only opens the window and shows our finished picture.
# pkg-config knows where brew put it: brew install sdl3
SDL_CFLAGS = $(shell pkg-config --cflags sdl3)
SDL_LIBS   = $(shell pkg-config --libs sdl3)

TARGET  = main
SOURCES = main.cpp camera.cpp layout.cpp mesh.cpp motion.cpp object.cpp player.cpp render.cpp solver.cpp test_scenes.cpp vec3.cpp window.cpp world.cpp

# The solver's stress tests: no window, so no SDL (docs/15).
TEST_TARGET  = solver_test
TEST_SOURCES = solver_test.cpp layout.cpp mesh.cpp motion.cpp solver.cpp test_scenes.cpp vec3.cpp
HEADERS = $(wildcard *.h)

.PHONY: all run stills test clean

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) $(SOURCES) -o $(TARGET) $(SDL_LIBS)

# Build and play a video in a window: make run, or make run SCENE=2
SCENE ?= 1
run: $(TARGET)
	./$(TARGET) $(SCENE)

# The pictures in docs/images: scene 3 after each step of the layout solver.
STEPS = naive greedy refined framed
# scene 4 (docs/16): each motion step, pictured at these seconds
MOTION_STEPS = naive orbits flights framed
MOTION_TIMES = 0 3 7 12
stills: $(TARGET) $(TEST_TARGET)
	@mkdir -p docs/images
	@for s in $(STEPS); do ./$(TARGET) 3 $$s docs/images/scene3-$$s.png > docs/images/scene3-$$s.txt; done
	@for s in greedy framed; do ./$(TARGET) stress crowd $$s docs/images/crowd-$$s.png > docs/images/crowd-$$s.txt; done
	@for s in refined framed; do ./$(TARGET) stress chain $$s docs/images/chain-$$s.png > docs/images/chain-$$s.txt; done
	@./$(TARGET) stress cycle framed docs/images/cycle-framed.png > docs/images/cycle-framed.txt
	@for s in $(MOTION_STEPS); do ./$(TARGET) 4 $$s docs/images/scene4-$$s.png > docs/images/scene4-$$s.txt; \
	  for t in $(MOTION_TIMES); do ./$(TARGET) 4 $$s docs/images/scene4-$$s-$$t.png $$t > /dev/null; done; done
	@./solver_test > docs/images/stress-results.txt || true
	@# our PNG writer doesn't compress (docs/pixel.h); macOS's sips can, ~50x smaller
	@if command -v sips > /dev/null; then for f in docs/images/*.png; do sips -s format png $$f --out $$f > /dev/null; done; fi

$(TEST_TARGET): $(TEST_SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(TEST_SOURCES) -o $(TEST_TARGET)

# Solve every test scene with every step, print the numbers, check the rules.
test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -f $(TARGET) $(TEST_TARGET)
	rm -rf renders out
