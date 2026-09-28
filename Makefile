CXX      ?= clang++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2

TARGET  = main
SOURCES = main.cpp mesh.cpp object.cpp render.cpp vec3.cpp
HEADERS = $(wildcard *.h)

# Every shape in shapes/ becomes a target: make cube, make torus, ...
SHAPES  = $(basename $(notdir $(wildcard shapes/*.obj)))
RENDERS = renders

# Override on the command line: make run SHAPE=shapes/torus.obj OUT=torus.png
SHAPE ?= shapes/cube.obj
OUT   ?= out.png

# Image viewer: macOS has `open`, Linux has `xdg-open`.
OPEN ?= $(if $(shell command -v open),open,xdg-open)

.PHONY: all run clean $(SHAPES)

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET)

# Build, render, and open the image.
run: $(TARGET)
	./$(TARGET) $(SHAPE) $(OUT)
	$(OPEN) $(OUT)

# make torus -> renders/torus.png, then opens it.
$(SHAPES): $(TARGET)
	@mkdir -p $(RENDERS)
	./$(TARGET) shapes/$@.obj $(RENDERS)/$@.png
	$(OPEN) $(RENDERS)/$@.png

clean:
	rm -f $(TARGET)
	rm -rf $(RENDERS)
