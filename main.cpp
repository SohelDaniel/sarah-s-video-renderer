#include "camera.h"
#include "mesh.h"
#include "render.h"
#include "transform.h"

#include <iostream>
#include <string>

// usage: ./main [shape.obj] [output.png]
int main(int argc, char** argv) {
    std::string input  = argc > 1 ? argv[1] : "shapes/cube.obj";
    std::string output = argc > 2 ? argv[2] : "out.png";

    mesh model(input);

    camera cam;
    render renderer(640, 480, cam);

    // Tilt the model a bit so we see more than one side.
    mat4<float> model_matrix = rotate_y(0.6f) * rotate_x(0.4f);
    renderer.draw_mesh(model, model_matrix, px::Pixel(230, 130, 60));

    if (!renderer.save(output)) {
        std::cerr << "could not write " << output << "\n";
        return 1;
    }
}
