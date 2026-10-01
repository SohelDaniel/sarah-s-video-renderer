#pragma once
#include "pixel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// ============================================================================
//  sRGB and linear light (docs/35).
//
//  A pixel's bytes are NOT amounts of light. Screens follow the sRGB curve:
//  byte 128 gives only about 22% of the light of byte 255, not 50%. So
//  mixing two colors by averaging their bytes (half an edge, a see-through
//  pane, an anti-aliased pixel) comes out too dark.
//
//  The fix: turn the bytes into light (decode), mix the light, and turn the
//  result back into bytes (encode).
//
//      decode(c) = c / 12.92                          if c <= 0.04045
//                = ((c + 0.055) / 1.055) ^ 2.4        otherwise        (c = byte / 255)
//      encode(l) = 12.92 · l                          if l <= 0.0031308
//                = 1.055 · l ^ (1 / 2.4) − 0.055      otherwise
//
//  The short straight piece near black keeps the curve from being infinitely
//  steep at 0.
// ============================================================================
namespace srgb{
	// how much light a byte gives, 0..1 (a table: there are only 256 bytes)
	inline float to_linear(uint8_t byte){
		static const std::array<float, 256> table = []{
			std::array<float, 256> t{};
			for(int b = 0;b<256;b++){
				float c = b / 255.0f;
				t[size_t(b)] = c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
			}
			return t;
		}();
		return table[byte];
	}

	// the byte that gives this much light
	inline uint8_t to_byte(float light){
		float l = std::clamp(light, 0.0f, 1.0f);
		float c = l <= 0.0031308f ? 12.92f * l : 1.055f * std::pow(l, 1.0f / 2.4f) - 0.055f;
		return uint8_t(std::lround(c * 255.0f));
	}

	// Like px::Image::Draw (p.a = how much of p covers the pixel), but the
	// colors are mixed as light, not as bytes.
	inline void blend(px::Image& image,int x,int y,px::Pixel p){
		if(x < 0 || y < 0 || x >= image.Width() || y >= image.Height()) return;
		if(p.a == 0) return;
		if(p.a == 255){ image.Draw(x, y, p); return; }
		px::Pixel d = image.Get(x, y);
		float sa = p.a / 255.0f, da = d.a / 255.0f;
		float oa = sa + da * (1.0f - sa);
		auto mix = [&](uint8_t s,uint8_t t){
			return to_byte((to_linear(s) * sa + to_linear(t) * da * (1.0f - sa)) / oa);
		};
		// written straight in (Draw would mix it a second time if oa < 1)
		image.Data()[size_t(y) * size_t(image.Width()) + size_t(x)] =
			px::Pixel(mix(p.r, d.r), mix(p.g, d.g), mix(p.b, d.b), uint8_t(std::lround(oa * 255.0f)));
	}
}
