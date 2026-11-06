/*
    Aeskulap 3x3 image kernel
    Copyright (C) 2026  Lars Christian Nygard

    GPL v2+; see volume.h header.
*/

#include "displaykernel.h"

#include <algorithm>
#include <cstring>

namespace Aeskulap {
namespace Kernel {

namespace {

struct K3 { int k[9]; int div; };

const K3 kernels[] = {
	// IDENTITY
	{ {0,0,0, 0,1,0, 0,0,0}, 1 },
	// SHARPEN
	{ {0,-1,0, -1,5,-1, 0,-1,0}, 1 },
	// EDGE_ENHANCE (gentle; integer div keeps HU domain stable)
	{ {-1,-1,-1, -1,6,-1, -1,-1,-1}, 2 },
	// BLUR
	{ {1,1,1, 1,1,1, 1,1,1}, 9 },
	// SMOOTH (binomial low-pass, classic noise reduction)
	{ {1,2,1, 2,4,2, 1,2,1}, 16 },
	// MEDIAN placeholder (nonlinear; handled separately)
	{ {0,0,0, 0,1,0, 0,0,0}, 1 },
};

inline int clampi(long v) { return v < 0 ? 0 : (v > 65535 ? 65535 : (int)v); }

} // namespace

int id_from_name(const std::string& nm) {
	for(int i = 0; i < ID_COUNT; i++) {
		if(nm == name(i)) return i;
	}
	return -1;
}

const char* name(int id) {
	switch(id) {
		case SHARPEN:      return "sharpen";
		case EDGE_ENHANCE: return "edge enhance";
		case BLUR:         return "blur";
		case SMOOTH:       return "smooth";
		case MEDIAN:       return "median";
		default:           return "none";
	}
}

const char* label(int id) {
	switch(id) {
		case SHARPEN:      return "Sharpen";
		case EDGE_ENHANCE: return "Edge enhance";
		case BLUR:         return "Blur";
		case SMOOTH:       return "Noise reduction";
		case MEDIAN:       return "Median (denoise)";
		default:           return "None";
	}
}

void apply(int id, const guint16* src, guint16* dst, int w, int h, bool is_signed) {
	if(id <= IDENTITY || id >= ID_COUNT) {
		if(src != dst) {
			std::memcpy(dst, src, (size_t)w * h * sizeof(guint16));
		}
		return;
	}

	// signed CT pixels live in two's-complement space; linear kernels
	// computed over the unsigned wrap ring would spike along the air/HU
	// boundary. Shift into an unsigned linear domain, kernel, shift back.
	// (guint16 v, signed: real value = v - 32768)
	const long off = is_signed ? 32768L : 0L;

	if(id == MEDIAN) {
		static const int dx[8] = {-1,0,1,-1,1,-1,0,1};
		static const int dy[8] = {-1,-1,-1,0,0,1,1,1};
		for(int y = 0; y < h; y++) {
			for(int x = 0; x < w; x++) {
				long vals[9];
				vals[0] = (long)src[(size_t)y*w + x] - off;
				int n = 1;
				for(int i = 0; i < 8; i++) {
					int nx = x + dx[i];
					int ny = y + dy[i];
					if(nx < 0) nx = 0;
					if(ny < 0) ny = 0;
					if(nx >= w) nx = w-1;
					if(ny >= h) ny = h-1;
					vals[n++] = (long)src[(size_t)ny*w + nx] - off;
				}
				std::nth_element(vals, vals+4, vals+9);
				dst[(size_t)y*w + x] = (guint16)clampi(vals[4] + off);
			}
		}
		return;
	}

	const K3& k = kernels[id];
	for(int y = 0; y < h; y++) {
		for(int x = 0; x < w; x++) {
			long sum = 0;
			for(int j = -1; j <= 1; j++) {
				int ny = y + j;
				if(ny < 0) ny = 0;
				if(ny >= h) ny = h-1;
				for(int i = -1; i <= 1; i++) {
					int nx = x + i;
					if(nx < 0) nx = 0;
					if(nx >= w) nx = w-1;
					sum += (long)k.k[(j+1)*3 + (i+1)] * ((long)src[(size_t)ny*w + nx] - off);
				}
			}
			long div = k.div;
			if(div <= 0) {
				div = 0;
				for(int t = 0; t < 9; t++) div += k.k[t];
				if(div == 0) div = 1;
			}
			dst[(size_t)y*w + x] = (guint16)clampi(sum / div + off);
		}
	}
}

} // namespace Kernel
} // namespace Aeskulap
