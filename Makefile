CXX      ?= clang++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2

TARGET  = main
SOURCES = main.cpp mesh.cpp render.cpp vec3.cpp
HEADERS = $(wildcard *.h)

# Override on the command line: make run SHAPE=shapes/torus.obj OUT=torus.png
SHAPE ?= shapes/cube.obj
OUT   ?= out.png

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET)

run: $(TARGET)
	./$(TARGET) $(SHAPE) $(OUT)

clean:
	rm -f $(TARGET)
