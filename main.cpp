#include "pixel.h"

int main() {
    px::Image img(320, 240);
    img.Draw(10, 20, px::Pixel(255, 0, 0));
    img.Save("out.png");
}
