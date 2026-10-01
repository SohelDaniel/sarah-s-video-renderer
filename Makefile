CXX      ?= clang++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2

# SDL3 only opens the window and shows our finished picture.
# pkg-config knows where brew put it: brew install sdl3
SDL_CFLAGS = $(shell pkg-config --cflags sdl3)
SDL_LIBS   = $(shell pkg-config --libs sdl3)

TARGET  = main
SOURCES = main.cpp camera.cpp fly_camera.cpp font.cpp label_layout.cpp math_layout.cpp layout.cpp live_scene.cpp mesh.cpp motion.cpp object.cpp player.cpp render.cpp scene_parser.cpp solver.cpp test_scenes.cpp vec3.cpp window.cpp world.cpp

# The solver's stress tests: no window, so no SDL (docs/15).
TEST_TARGET  = solver_test
TEST_SOURCES = solver_test.cpp label_layout.cpp layout.cpp mesh.cpp motion.cpp solver.cpp test_scenes.cpp vec3.cpp
# The engine's own tests (docs/19-21): also no SDL
ENGINE_TEST         = engine_test
ENGINE_TEST_SOURCES = engine_test.cpp camera.cpp fly_camera.cpp font.cpp label_layout.cpp math_layout.cpp layout.cpp live_scene.cpp mesh.cpp motion.cpp object.cpp render.cpp scene_parser.cpp solver.cpp test_scenes.cpp vec3.cpp world.cpp
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
# scene 5 (docs/18): before, at and after the planned hits
IMPACT_TIMES = 5 6 10 12 16
stills: $(TARGET) $(TEST_TARGET)
	@mkdir -p docs/images
	@for s in $(STEPS); do ./$(TARGET) 3 $$s docs/images/scene3-$$s.png > docs/images/scene3-$$s.txt; done
	@for s in greedy framed; do ./$(TARGET) stress crowd $$s docs/images/crowd-$$s.png > docs/images/crowd-$$s.txt; done
	@for s in refined framed; do ./$(TARGET) stress chain $$s docs/images/chain-$$s.png > docs/images/chain-$$s.txt; done
	@./$(TARGET) stress cycle framed docs/images/cycle-framed.png > docs/images/cycle-framed.txt
	@for s in naive framed; do ./$(TARGET) 5 $$s docs/images/scene5-$$s.png > docs/images/scene5-$$s.txt; \
	  for t in $(IMPACT_TIMES); do ./$(TARGET) 5 $$s docs/images/scene5-$$s-$$t.png $$t > /dev/null; done; done
	@for s in $(MOTION_STEPS); do ./$(TARGET) 4 $$s docs/images/scene4-$$s.png > docs/images/scene4-$$s.txt; \
	  for t in $(MOTION_TIMES); do ./$(TARGET) 4 $$s docs/images/scene4-$$s-$$t.png $$t > /dev/null; done; done
	@for c in on off; do ./$(TARGET) clip $$c docs/images/clip-$$c.png > /dev/null; done
	@./$(TARGET) walk docs/images/walk- > docs/images/walk.txt
	@./$(TARGET) ease docs/images/easing.png > /dev/null
	@./$(TARGET) aa docs/images/aa- > /dev/null
	@./$(TARGET) fonts docs/images/font- > /dev/null
	@for t in 0 2 5 11; do ./$(TARGET) scenes/fades.dan framed docs/images/fades-$$t.png $$t > /dev/null; done
	@for t in 7 10; do ./$(TARGET) scenes/arrows.dan framed docs/images/arrows-$$t.png $$t --aa > /dev/null; done
	@./$(TARGET) scenes/arrows.dan framed docs/images/text-title.png 7 --aa > /dev/null
	@for t in 2 8 14; do ./$(TARGET) scenes/labels.dan framed docs/images/labels-$$t.png $$t --aa > /dev/null; done
	@for t in 3 13; do ./$(TARGET) scenes/math.dan framed docs/images/math-$$t.png $$t --aa > /dev/null; done
	@./$(TARGET) scenes/labelled.dan framed docs/images/labelled-after.png 0 --aa > /dev/null
	@rm -f scenes/*.report
	@./solver_test > docs/images/stress-results.txt || true
	@# our PNG writer doesn't compress (docs/pixel.h); macOS's sips can, ~50x smaller
	@if command -v sips > /dev/null; then for f in docs/images/*.png; do sips -s format png $$f --out $$f > /dev/null; done; fi

$(TEST_TARGET): $(TEST_SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(TEST_SOURCES) -o $(TEST_TARGET)

$(ENGINE_TEST): $(ENGINE_TEST_SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(ENGINE_TEST_SOURCES) -o $(ENGINE_TEST)

# Solve every test scene with every step, print the numbers, check the rules;
# then test the engine (clipping, controls, the scene language).
test: $(TEST_TARGET) $(ENGINE_TEST)
	./$(TEST_TARGET)
	./$(ENGINE_TEST)

clean:
	rm -f $(TARGET) $(TEST_TARGET) $(ENGINE_TEST)
	rm -rf renders out
