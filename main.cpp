#include "camera.h"
#include "mesh.h"
#include "object.h"

#include <iostream>
#include <exception>
#include <string>
#include <vector>

// usage: ./main [shape.obj] [folder]
// Writes folder/shot_1.png .. shot_3.png (default folder: out).
// The shape you pass is object 1, in the middle. Objects 2 and 3 are fixed.
void example_scene(){

    std::string input  = "shapes/cube.obj";
    std::string folder = "out";

    try {
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

        // Everything the camera should photograph.
        std::vector<const object*> scene = {&one, &two, &three};

        // One camera, looking at object 1, saving into `folder`.
        camera cam;
        cam.folder = folder;
        cam.eye    = vec3(0.0f, 1.5f, 7.0f);
        cam.target = one.get_position();

        // ---- Picture 1: the starting scene ----
        cam.take(scene);

        // ---- Picture 2: things move, camera stays put ----
        one.rotate(1.6f, 0.35f);              // cube spins further on y
        two.move(vec3(-1.8f, 1.4f, 0.5f));    // sphere moves up and toward the middle
        two.scale(1.0f);                      // ...and grows back to normal size
        cam.take(scene);

        // ---- Picture 3: camera turns and flies over to look at object 3 ----
        cam.target = three.get_position();
        cam.eye    = three.get_position() + vec3(0.5f, 1.5f, 4.0f);
        cam.take(scene);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        throw std::invalid_argument("error");
    }
}
void example_scene2(){


    std::string input  =  "shapes/cube.obj";
    std::string folder =  "out";

    try {
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

        // Everything the camera should photograph.
        std::vector<const object*> scene = {&one, &two, &three};

        // One camera, looking at object 1, saving into `folder`.
        camera cam;
        cam.folder = folder;
        cam.eye    = vec3(0.0f, 1.5f, 7.0f);
        cam.target = one.get_position();

        // ---- Picture 1: the starting scene ----
        cam.take(scene);

        // ---- Picture 2: things move, camera stays put ----
        one.rotate(1.6f, 0.35f);              // cube spins further on y
        two.move(vec3(-1.8f, 1.4f, 0.5f));    // sphere moves up and toward the middle
        two.scale(1.0f);                      // ...and grows back to normal size
        cam.take(scene);

        // ---- Picture 3: camera turns and flies over to look at object 3 ----
        cam.target = three.get_position();
        cam.eye    = three.get_position() + vec3(0.5f, 1.5f, 4.0f);
        cam.take(scene);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        throw std::invalid_argument("error");
    }



}

int main(int argc, char** argv) {
    example_scene();
}
