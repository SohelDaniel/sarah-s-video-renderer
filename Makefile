CXX      ?= clang++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2

TARGET  = main
SOURCES = main.cpp camera.cpp mesh.cpp object.cpp render.cpp vec3.cpp
HEADERS = $(wildcard *.h)

# Every shape in shapes/ becomes a target: make cube, make torus, ...
SHAPES  = $(basename $(notdir $(wildcard shapes/*.obj)))
RENDERS = renders

# Override on the command line: make run SHAPE=shapes/torus.obj OUT=torus
# OUT is a folder: writes $(OUT)/shot_1.png, shot_2.png, ...
SHAPE ?= shapes/cube.obj
OUT   ?= out

# Image viewer: macOS has `open`, Linux has `xdg-open`.
OPEN ?= $(if $(shell command -v open),open,xdg-open)

.PHONY: all run clean $(SHAPES)

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET)

# Build, render, and open the images.
run: $(TARGET)
	./$(TARGET) $(SHAPE) $(OUT)
	$(OPEN) $(OUT)/shot_*.png

# make torus -> renders/torus/shot_1.png, shot_2.png, ..., then opens them.
$(SHAPES): $(TARGET)
	./$(TARGET) shapes/$@.obj $(RENDERS)/$@
	$(OPEN) $(RENDERS)/$@/shot_*.png

clean:
	rm -f $(TARGET)
	rm -rf $(RENDERS) out
