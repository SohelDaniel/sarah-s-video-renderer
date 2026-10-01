#include "font.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <stdexcept>

// stb_truetype is someone else's code: only here, and without our strict
// warnings (they'd be about its code, not ours)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wall"
#pragma clang diagnostic ignored "-Wextra"
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wunused-function"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#pragma clang diagnostic pop


// ---------------------------------------------------------------------------
//  UTF-8 (docs/29): one character is 1 to 4 bytes. The first byte's top
//  bits say how many:
//    0xxxxxxx                              1 byte  (plain ASCII)
//    110xxxxx 10xxxxxx                     2 bytes (Greek, ...)
//    1110xxxx 10xxxxxx 10xxxxxx            3 bytes (most symbols)
//    11110xxx 10xxxxxx 10xxxxxx 10xxxxxx   4 bytes
//  The x bits, put together, are the code point.
// ---------------------------------------------------------------------------
std::vector<int> utf8_decode(const std::string& text){
	std::vector<int> out;
	for(size_t i = 0;i<text.size();){
		unsigned char c = (unsigned char)text[i];
		int length = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 0;
		if(length == 0 || i + length > text.size()){ out.push_back('?'); i++; continue; }
		int code = length == 1 ? c : c & (0x7F >> length);   // the first byte's x bits
		bool ok = true;
		for(int k = 1;k<length;k++){
			unsigned char more = (unsigned char)text[i + k];
			if((more >> 6) != 0x2){ ok = false; break; }
			code = (code << 6) | (more & 0x3F);              // 6 more x bits from each next byte
		}
		out.push_back(ok ? code : '?');
		i += ok ? length : 1;
	}
	return out;
}

// ---------------------------------------------------------------------------
//  Quadratic Bezier curves (docs/29). de Casteljau: go t of the way along
//  each of the two control lines, then t of the way between those points.
//      a = lerp(p0, p1, t),  b = lerp(p1, p2, t),  point = lerp(a, b, t)
// ---------------------------------------------------------------------------
static point2 mix(point2 a,point2 b,float t){
	return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

point2 bezier_point(point2 p0,point2 p1,point2 p2,float t){
	return mix(mix(p0, p1, t), mix(p1, p2, t), t);
}

// How far the curve bulges from the straight line p0-p2: at most
// |p0 − 2·p1 + p2| / 4 (the bulge at t = ½). If that's small enough, a
// straight line will do; otherwise cut the curve in half at t = ½ (each half
// is a quadratic Bezier again, with the de Casteljau points as controls) and
// do the same to each half.
void flatten_quadratic(point2 p0,point2 p1,point2 p2,float tolerance,std::vector<point2>& out){
	float bx = p0.x - 2.0f * p1.x + p2.x, by = p0.y - 2.0f * p1.y + p2.y;
	float bulge = std::sqrt(bx * bx + by * by) / 4.0f;
	if(bulge <= tolerance){
		out.push_back(p2);
		return;
	}
	point2 a = mix(p0, p1, 0.5f), b = mix(p1, p2, 0.5f), middle = mix(a, b, 0.5f);
	flatten_quadratic(p0, a, middle, tolerance, out);
	flatten_quadratic(middle, b, p2, tolerance, out);
}

// ---------------------------------------------------------------------------
//  Filling (docs/29), the nonzero winding rule. Look along a horizontal line
//  through the shape: every edge it crosses going DOWN adds 1, going UP takes
//  1 away. Wherever the running total is not 0, you're inside. (An 'o' is two
//  loops going opposite ways, so inside its hole the total is back to 0.)
//  Each pixel is looked at along `samples` lines, at `samples` points each,
//  and its coverage is the share of those points that are inside.
// ---------------------------------------------------------------------------
std::vector<float> fill_loops(const std::vector<std::vector<point2>>& loops,int width,int height,int samples){
	std::vector<float> coverage(size_t(width) * size_t(height), 0.0f);
	std::vector<int> count(width);
	struct crossing{ float x; int direction; };
	std::vector<crossing> crossings;
	float per_point = 1.0f / float(samples * samples);

	for(int y = 0;y<height;y++){
		std::fill(count.begin(), count.end(), 0);
		for(int k = 0;k<samples;k++){
			float sy = y + (k + 0.5f) / samples;
			crossings.clear();
			for(const std::vector<point2>& loop : loops){
				for(size_t i = 0;i<loop.size();i++){
					point2 a = loop[i], b = loop[(i + 1) % loop.size()];
					// half-open, so a line through a corner counts it once
					bool down = a.y <= sy && sy < b.y, up = b.y <= sy && sy < a.y;
					if(!down && !up) continue;
					float x = a.x + (sy - a.y) * (b.x - a.x) / (b.y - a.y);
					crossings.push_back({x, down ? 1 : -1});
				}
			}
			std::sort(crossings.begin(), crossings.end(), [](const crossing& p,const crossing& q){ return p.x < q.x; });
			int winding = 0;
			for(size_t c = 0;c + 1<crossings.size();c++){
				winding += crossings[c].direction;
				if(winding == 0) continue;
				// inside from this crossing to the next: count the sample columns there
				float from = crossings[c].x, to = crossings[c + 1].x;
				int first = std::max(0, int(std::floor(from)));
				int last = std::min(width - 1, int(std::ceil(to)));
				for(int x = first;x<=last;x++){
					for(int m = 0;m<samples;m++){
						float sx = x + (m + 0.5f) / samples;
						if(sx >= from && sx < to) count[x]++;
					}
				}
			}
		}
		for(int x = 0;x<width;x++) coverage[size_t(y) * width + x] = std::min(1.0f, count[x] * per_point);
	}
	return coverage;
}

// ---------------------------------------------------------------------------
//  The font file
// ---------------------------------------------------------------------------
struct font::data{
	std::vector<unsigned char> bytes;
	stbtt_fontinfo info;
};

font::font(const std::string& filename) : d(std::make_unique<data>()){
	std::ifstream in(filename, std::ios::binary);
	if(!in) throw std::runtime_error("could not open font " + filename);
	d->bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	if(!stbtt_InitFont(&d->info, d->bytes.data(), stbtt_GetFontOffsetForIndex(d->bytes.data(), 0))){
		throw std::runtime_error("not a font file: " + filename);
	}
}

font::~font() = default;

// Font files measure everything in their own "units per em"; this turns
// those into pixels for a letter size (em) of `size` pixels.
static float to_pixels(const stbtt_fontinfo& info,float size){
	return stbtt_ScaleForMappingEmToPixels(&info, size);
}

float font::ascent(float size)const{
	int a, b, gap;
	stbtt_GetFontVMetrics(&d->info, &a, &b, &gap);
	return a * to_pixels(d->info, size);
}

float font::descent(float size)const{
	int a, b, gap;
	stbtt_GetFontVMetrics(&d->info, &a, &b, &gap);
	return -b * to_pixels(d->info, size);
}

float font::advance(int codepoint,float size)const{
	int advance_width, left_bearing;
	stbtt_GetCodepointHMetrics(&d->info, codepoint, &advance_width, &left_bearing);
	return advance_width * to_pixels(d->info, size);
}

float font::kerning(int a,int b,float size)const{
	return stbtt_GetCodepointKernAdvance(&d->info, a, b) * to_pixels(d->info, size);
}

float font::width(const std::string& text,float size)const{
	std::vector<int> codes = utf8_decode(text);
	float w = 0.0f;
	for(size_t i = 0;i<codes.size();i++){
		w += advance(codes[i], size);
		if(i + 1 < codes.size()) w += kerning(codes[i], codes[i + 1], size);
	}
	return w;
}

std::vector<std::vector<point2>> font::outline(int codepoint,float size)const{
	stbtt_vertex* v = nullptr;
	int n = stbtt_GetCodepointShape(&d->info, codepoint, &v);
	float scale = to_pixels(d->info, size);
	std::vector<std::vector<point2>> loops;
	point2 pen{0, 0};
	// font units have y going UP; the screen's y goes DOWN, so flip it
	auto at = [&](int x,int y){ return point2{x * scale, -y * scale}; };
	for(int i = 0;i<n;i++){
		point2 p = at(v[i].x, v[i].y);
		if(v[i].type == STBTT_vmove){
			loops.push_back({p});
		}else if(v[i].type == STBTT_vline && !loops.empty()){
			loops.back().push_back(p);
		}else if(v[i].type == STBTT_vcurve && !loops.empty()){
			flatten_quadratic(pen, at(v[i].cx, v[i].cy), p, 0.25f, loops.back());
		}
		pen = p;
	}
	stbtt_FreeShape(&d->info, v);
	return loops;
}

const font::glyph& font::get(int codepoint,float size)const{
	auto key = std::make_pair(codepoint, int(std::lround(size * 4.0f)));
	auto found = cache.find(key);
	if(found != cache.end()) return found->second;

	glyph g;
	std::vector<std::vector<point2>> loops = outline(codepoint, size);
	if(!loops.empty()){
		float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
		for(const auto& loop : loops) for(const point2& p : loop){
			x0 = std::min(x0, p.x); y0 = std::min(y0, p.y); x1 = std::max(x1, p.x); y1 = std::max(y1, p.y);
		}
		g.left = int(std::floor(x0)) - 1;
		g.top = int(std::floor(y0)) - 1;
		g.width = int(std::ceil(x1)) - g.left + 2;
		g.height = int(std::ceil(y1)) - g.top + 2;
		for(auto& loop : loops) for(point2& p : loop){ p.x -= g.left; p.y -= g.top; }
		g.coverage = fill_loops(loops, g.width, g.height);
	}
	return cache.emplace(key, std::move(g)).first->second;
}

namespace fonts{
	static const font* load(const char* file){
		try{
			static std::map<std::string, std::unique_ptr<font>> loaded;
			auto it = loaded.find(file);
			if(it == loaded.end()) it = loaded.emplace(file, std::make_unique<font>(file)).first;
			return it->second.get();
		}catch(const std::exception&){
			return nullptr;
		}
	}
	const font* sans(){   static const font* f = load("fonts/DejaVuSans.ttf");          return f; }
	const font* serif(){  static const font* f = load("fonts/DejaVuSerif.ttf");         return f; }
	const font* italic(){ static const font* f = load("fonts/DejaVuSerif-Italic.ttf");  return f; }
}
