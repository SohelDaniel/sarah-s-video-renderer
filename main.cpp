#include "camera.h"
#include "mesh.h"
#include "object.h"
#include "render.h"

#include <iostream>
#include <string>

// usage: ./main [shape.obj] [output.png]
// The shape you pass is object 1, in the middle. Objects 2 and 3 are fixed.
int main(int argc, char** argv) {
    std::string input  = argc > 1 ? argv[1] : "shapes/cube.obj";
    std::string output = argc > 2 ? argv[2] : "out.png";

    // Load each .obj ONCE. Objects only point at these, so two objects
    // using the same mesh don't load or copy it twice.
    mesh main_shape(input);
    mesh sphere("shapes/sphere.obj");
    mesh torus("shapes/torus.obj");

    // Object 1: in the middle, spun 45° on y and tipped 20° on x.
    object one(main_shape, px::Pixel(230, 130, 60));
    one.rotate(0.785f, 0.35f);

    // Object 2: moved to the left and a bit back, a bit smaller.
    object two(sphere, px::Pixel(80, 160, 230));
    two.move(vec3(-3.0f, 0.0f, -1.0f));
    two.scale(0.8f);

    // Object 3: starts on the right at (3,0,0), tipped on x, then swung 45°
    // around object 1 -> ends up at about (2.12, 0, -2.12), behind and right.
    object three(torus, px::Pixel(120, 200, 90));
    three.move(vec3(3.0f, 0.0f, 0.0f));
    three.rotate(0.0f, 1.0f);
    three.rotate_around(one.get_position(), 0.785f, 0.0f);

    // One camera for the whole picture, looking at object 1.
    // To look at object 3 instead: cam.target = three.get_position();
    camera cam;
    cam.eye    = vec3(0.0f, 1.5f, 7.0f);
    cam.target = one.get_position();

    // One render = one image + one depth buffer, shared by all objects,
    // so they hide behind each other correctly.
    render renderer(640, 480, cam);
    one.draw(renderer);
    two.draw(renderer);
    three.draw(renderer);

    if (!renderer.save(output)) {
        std::cerr << "could not write " << output << "\n";
        return 1;
    }
}
