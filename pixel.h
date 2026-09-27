#pragma once
// ============================================================================
//  pixel.h  -  a tiny single-header library for drawing pixels
//
//  Just put this file next to your .cpp and #include "pixel.h". No setup,
//  no other libraries, nothing to install.
//
//      px::Image img(320, 240);                 // canvas, black by default
//      img.Draw(10, 20, px::Pixel(255, 0, 0));  // red pixel at x=10, y=20
//      img.Save("out.png");                     // .png or .ppm
//
//  (0,0) is the TOP-LEFT corner. x goes right, y goes down.
//  Drawing outside the image is safe; those pixels are simply ignored.
// ============================================================================

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace px {

// ---------------------------------------------------------------------------
// Pixel: a color. r, g, b are 0-255. a is opacity: 255 = solid, 0 = invisible.
// ---------------------------------------------------------------------------
struct Pixel {
    uint8_t r = 0, g = 0, b = 0, a = 255;
    constexpr Pixel() = default;
    constexpr Pixel(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255)
        : r(r_), g(g_), b(b_), a(a_) {}
};
static_assert(sizeof(Pixel) == 4, "Pixel must be exactly 4 bytes");

constexpr Pixel BLACK(0, 0, 0);
constexpr Pixel WHITE(255, 255, 255);
constexpr Pixel RED(255, 0, 0);
constexpr Pixel GREEN(0, 255, 0);
constexpr Pixel BLUE(0, 0, 255);
constexpr Pixel YELLOW(255, 255, 0);
constexpr Pixel CYAN(0, 255, 255);
constexpr Pixel MAGENTA(255, 0, 255);
constexpr Pixel GREY(128, 128, 128);
constexpr Pixel TRANSPARENT(0, 0, 0, 0);

namespace detail {
    inline bool WritePNG(const std::string& filename, int w, int h, const std::vector<Pixel>& px);
    inline bool WritePPM(const std::string& filename, int w, int h, const std::vector<Pixel>& px);
}

// ---------------------------------------------------------------------------
// Image: your canvas.
// ---------------------------------------------------------------------------
class Image {
public:
    Image(int width, int height, Pixel background = BLACK)
        : width_(std::max(1, width)), height_(std::max(1, height)),
          pixels_(size_t(width_) * size_t(height_), background) {}

    int Width() const { return width_; }
    int Height() const { return height_; }

    // Set one pixel. Semi-transparent colors (a < 255) blend with what's there.
    void Draw(int x, int y, Pixel p) {
        if (x < 0 || y < 0 || x >= width_ || y >= height_) return;
        Pixel& dst = pixels_[size_t(y) * width_ + x];
        if (p.a == 255) { dst = p; return; }
        if (p.a == 0) return;
        float sa = p.a / 255.0f, da = dst.a / 255.0f;
        float oa = sa + da * (1.0f - sa);
        auto mix = [&](uint8_t s, uint8_t d) {
            return uint8_t(std::lround((s * sa + d * da * (1.0f - sa)) / oa));
        };
        dst = Pixel(mix(p.r, dst.r), mix(p.g, dst.g), mix(p.b, dst.b),
                    uint8_t(std::lround(oa * 255.0f)));
    }

    // Read one pixel (returns TRANSPARENT if outside the image).
    Pixel Get(int x, int y) const {
        if (x < 0 || y < 0 || x >= width_ || y >= height_) return TRANSPARENT;
        return pixels_[size_t(y) * width_ + x];
    }

    // Fill the whole image with one color.
    void Clear(Pixel p = BLACK) { std::fill(pixels_.begin(), pixels_.end(), p); }

    // Straight line between two points.
    void DrawLine(int x0, int y0, int x1, int y1, Pixel p) {
        int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;
        while (true) {
            Draw(x0, y0, p);
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }

    // Rectangle outline / filled rectangle. (x, y) is the top-left corner.
    void DrawRect(int x, int y, int w, int h, Pixel p) {
        if (w <= 0 || h <= 0) return;
        DrawLine(x, y, x + w - 1, y, p);
        DrawLine(x, y + h - 1, x + w - 1, y + h - 1, p);
        for (int j = y + 1; j < y + h - 1; ++j) { Draw(x, j, p); Draw(x + w - 1, j, p); }
    }
    void FillRect(int x, int y, int w, int h, Pixel p) {
        for (int j = y; j < y + h; ++j)
            for (int i = x; i < x + w; ++i) Draw(i, j, p);
    }

    // Circle outline / filled circle, centered at (cx, cy).
    void DrawCircle(int cx, int cy, int radius, Pixel p) {
        if (radius < 0) return;
        int x = radius, y = 0, err = 1 - radius;
        while (x >= y) {
            Draw(cx + x, cy + y, p); Draw(cx + y, cy + x, p);
            Draw(cx - y, cy + x, p); Draw(cx - x, cy + y, p);
            Draw(cx - x, cy - y, p); Draw(cx - y, cy - x, p);
            Draw(cx + y, cy - x, p); Draw(cx + x, cy - y, p);
            ++y;
            if (err < 0) err += 2 * y + 1;
            else { --x; err += 2 * (y - x) + 1; }
        }
    }
    void FillCircle(int cx, int cy, int radius, Pixel p) {
        if (radius < 0) return;
        for (int dy = -radius; dy <= radius; ++dy) {
            int dx = int(std::sqrt(double(radius) * radius - double(dy) * dy));
            for (int x = cx - dx; x <= cx + dx; ++x) Draw(x, cy + dy, p);
        }
    }

    // Save to a file. Format is picked from the extension: .png or .ppm
    // Returns true on success.
    bool Save(const std::string& filename) const {
        std::string lower = filename;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });
        if (lower.size() >= 4 && lower.compare(lower.size() - 4, 4, ".ppm") == 0)
            return detail::WritePPM(filename, width_, height_, pixels_);
        return detail::WritePNG(filename, width_, height_, pixels_);
    }

    // Raw access: width*height pixels, row by row, 4 bytes each (RGBA).
    const Pixel* Data() const { return pixels_.data(); }
    Pixel* Data() { return pixels_.data(); }

private:
    int width_, height_;
    std::vector<Pixel> pixels_;
};

// ============================================================================
//  Implementation details below - you never need to touch these.
// ============================================================================
namespace detail {

inline uint32_t Crc32(const uint8_t* data, size_t len, uint32_t crc) {
    static const std::array<uint32_t, 256> table = [] {
        std::array<uint32_t, 256> t{};
        for (uint32_t n = 0; n < 256; ++n) {
            uint32_t c = n;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[n] = c;
        }
        return t;
    }();
    for (size_t i = 0; i < len; ++i) crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc;
}

inline void PutU32BE(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back(uint8_t(x >> 24)); v.push_back(uint8_t(x >> 16));
    v.push_back(uint8_t(x >> 8));  v.push_back(uint8_t(x));
}

inline void WriteChunk(std::ofstream& out, const char* type, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> buf;
    PutU32BE(buf, uint32_t(data.size()));
    buf.insert(buf.end(), type, type + 4);
    buf.insert(buf.end(), data.begin(), data.end());
    uint32_t crc = Crc32(buf.data() + 4, buf.size() - 4, 0xFFFFFFFFu) ^ 0xFFFFFFFFu;
    PutU32BE(buf, crc);
    out.write(reinterpret_cast<const char*>(buf.data()), std::streamsize(buf.size()));
}

// Writes a valid RGBA PNG. For simplicity the data is stored without
// compression, so files are larger than usual but open everywhere.
inline bool WritePNG(const std::string& filename, int w, int h, const std::vector<Pixel>& px) {
    std::ofstream out(filename, std::ios::binary);
    if (!out) return false;

    const uint8_t signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    out.write(reinterpret_cast<const char*>(signature), 8);

    std::vector<uint8_t> ihdr;
    PutU32BE(ihdr, uint32_t(w));
    PutU32BE(ihdr, uint32_t(h));
    ihdr.push_back(8);  // 8 bits per channel
    ihdr.push_back(6);  // color type: RGBA
    ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0);
    WriteChunk(out, "IHDR", ihdr);

    // Raw scanlines: each row starts with a filter byte (0 = none).
    std::vector<uint8_t> raw;
    raw.reserve(size_t(h) * (size_t(w) * 4 + 1));
    for (int y = 0; y < h; ++y) {
        raw.push_back(0);
        const uint8_t* row = reinterpret_cast<const uint8_t*>(&px[size_t(y) * w]);
        raw.insert(raw.end(), row, row + size_t(w) * 4);
    }

    // zlib stream made of uncompressed ("stored") deflate blocks.
    std::vector<uint8_t> z = {0x78, 0x01};
    size_t pos = 0;
    do {
        uint16_t len = uint16_t(std::min<size_t>(65535, raw.size() - pos));
        bool last = pos + len == raw.size();
        uint16_t nlen = uint16_t(~len);
        z.push_back(last ? 1 : 0);
        z.push_back(uint8_t(len & 0xFF));  z.push_back(uint8_t(len >> 8));
        z.push_back(uint8_t(nlen & 0xFF)); z.push_back(uint8_t(nlen >> 8));
        z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + len);
        pos += len;
    } while (pos < raw.size());

    uint32_t a = 1, b = 0;
    for (uint8_t byte : raw) { a = (a + byte) % 65521; b = (b + a) % 65521; }
    PutU32BE(z, (b << 16) | a);

    WriteChunk(out, "IDAT", z);
    WriteChunk(out, "IEND", {});
    return bool(out);
}

// PPM: the simplest image format there is (no transparency).
inline bool WritePPM(const std::string& filename, int w, int h, const std::vector<Pixel>& px) {
    std::ofstream out(filename, std::ios::binary);
    if (!out) return false;
    out << "P6\n" << w << " " << h << "\n255\n";
    for (const Pixel& p : px) { out.put(char(p.r)); out.put(char(p.g)); out.put(char(p.b)); }
    return bool(out);
}

} // namespace detail
} // namespace px
