// Headless test: Volume slab/projection + Kernel + Palette LUT math.
// Build: g++ test_render.cpp widgets/*.o imagepool/*.o ... (see scripts/test_render.sh)
#include "volume.h"
#include "displaykernel.h"
#include "displaypalette.h"
#include "poolinstance.h"

#include <cstdio>
#include <cmath>
#include <vector>
#include <string>

static int failures = 0;
#define CHECK(cond, msg) do { if(!(cond)) { printf("FAIL: %s\n", msg); failures++; } else { printf("ok: %s\n", msg); } } while(0)

int main() {
	using namespace Aeskulap;

	// ---- Volume: 4 slices 4x4 ----
	// slice s: all pixels = (s+1)*10  => 10,20,30,40
	// center slice 1 (value 20):
	//  slab 1 PASS       => 20
	//  slab 3 (0..2) MAX => 30, MIN => 10, AVG => 20
	//  slab 4 (0..3) MAX => 40, AVG => 25
	auto vol = Volume::create();
	for(int s = 0; s < 4; s++) {
		std::vector<guint16> sl(16, (guint16)((s+1)*10));
		bool ok = vol->append_slice(&sl[0], 4, 4, s * 2.0, Glib::RefPtr<ImagePool::Instance>());
		CHECK(ok, "append_slice");
	}
	CHECK(vol->slices() == 4, "4 slices");

	std::vector<guint16> out;
	vol->project(1, out);
	CHECK(out.size() == 16 && out[0] == 20, "PASS center=20");

	vol->set_slab(3);
	vol->set_projection(Volume::MAXIP);
	vol->project(1, out);
	CHECK(out[0] == 30, "MAXIP slab3=30");
	vol->set_projection(Volume::MINIP);
	vol->project(1, out);
	CHECK(out[0] == 10, "MINIP slab3=10");
	vol->set_projection(Volume::AVGIP);
	vol->project(1, out);
	CHECK(out[0] == 20, "AVGIP slab3=20");

	vol->set_slab(4);
	vol->set_projection(Volume::MAXIP);
	vol->project(1, out);
	CHECK(out[0] == 40, "MAXIP slab4=40");
	vol->set_projection(Volume::AVGIP);
	vol->project(1, out);
	CHECK(out[0] == 25, "AVGIP slab4=25");

	// slab clamp
	vol->set_slab(99);
	CHECK(vol->slab() == 4, "slab clamped to slice count");

	// signed slice: HU 100/101 checkerboard (stored == HU for positive HU).
	// signed smoothing of a +-1 HU checker must stay within 100..101 HU.
	{
		int w = 8, h = 8;
		std::vector<guint16> src((size_t)w*h);
		for(int y = 0; y < h; y++)
			for(int x = 0; x < w; x++)
				src[(size_t)y*w+x] = (guint16)(100 + ((x + y) & 1)); // HU 100/101 checker
		std::vector<guint16> dst((size_t)w*h);
		Kernel::apply(Kernel::SMOOTH, &src[0], &dst[0], w, h, true);
		bool in_range = true;
		for(int y = 2; y < h-2; y++)
			for(int x = 2; x < w-2; x++) {
				int hu = (int)(int16_t)dst[(size_t)y*w+x];
				if(hu < 100 || hu > 101) in_range = false;
			}
		CHECK(in_range, "signed SMOOTH stays in HU range (no wrap spike)");

		// unsigned interpretation of same data: values 32868/32869, smooth
		// kernel in unsigned domain stays in range too (no boundary here) —
		// the wrap bug shows at the 0x7FFF/0x8000 seam:
		std::vector<guint16> seam((size_t)w*h);
		// realistic clinical edge: air (-1000 HU -> stored 0xFC18) left,
		// tissue (+40 HU -> stored 0x0028) right. The old unsigned-domain
		// code smoothed 64536->40 and spiked to mid-gray (~32000).
		for(int y = 0; y < h; y++)
			for(int x = 0; x < w; x++)
				seam[(size_t)y*w+x] = (x < 4) ? 0xFC18 : 0x0028;
		Kernel::apply(Kernel::SMOOTH, &seam[0], &dst[0], w, h, true);
		int hu_mid = (int)(int16_t)dst[(size_t)4*w + 3]; // straddling the seam
		CHECK(hu_mid >= -1000 && hu_mid <= 40, "signed smooth interpolates air<->tissue HU (no mid-gray spike)");
		CHECK(hu_mid < -100, "seam midpoint is HU-interpolated, not raw-unsigned spike");
	}

	// median denoise: single salt spike
	{
		int w = 8, h = 8;
		std::vector<guint16> src((size_t)w*h, 500);
		src[(size_t)4*w+4] = 60000;
		std::vector<guint16> dst((size_t)w*h);
		Kernel::apply(Kernel::MEDIAN, &src[0], &dst[0], w, h, false);
		CHECK(dst[(size_t)4*w+4] == 500, "median removes salt spike");
	}

	// ---- Palette LUT ----
	{
		std::vector<guint8> lut(256*3);
		// GRAY ramp: center 128, width 256 -> 0->0, 255->255 monotone
		Palette::build_lut(&lut[0], 256, Palette::GRAY, false, 128, 256);
		CHECK(lut[0] == 0 && lut[3*3] >= lut[2*3] && lut[255*3] >= 254, "GRAY LUT monotone 0..255");
		CHECK(lut[0] == lut[1] && lut[1] == lut[2], "GRAY LUT r=g=b");

		Palette::build_lut(&lut[0], 256, Palette::GRAY, true, 128, 256);
		CHECK(lut[0] == 255 && lut[255*3] <= 1, "GRAY invert swaps ends");

		// HOT_IRON: starts black, ends bright, red leads
		Palette::build_lut(&lut[0], 256, Palette::HOT_IRON, false, 128, 256);
		CHECK(lut[0] <= 16 && lut[0] >= 0 && lut[1] <= 16 && lut[2] <= 16, "HOT_IRON starts dark");
		CHECK(lut[255*3] >= 200 && lut[255*3+1] >= 200 && lut[255*3+2] >= 100, "HOT_IRON ends bright warm");
		CHECK(lut[64*3] > lut[64*3+2], "HOT_IRON red leads blue at low-mid");

		// 16-bit LUT size runs
		std::vector<guint8> lut64(65536*3, 0xFF);
		Palette::build_lut(&lut64[0], 65536, Palette::GRAY, false, 32768, 65536);
		CHECK(lut64[0] == 0 && lut64[65535*3] >= 254, "16-bit GRAY ramp fills 0..255");

		// window narrower than full range: [center=100,width=50] -> below = 0, above = 255
		Palette::build_lut(&lut[0], 256, Palette::GRAY, false, 100, 50);
		CHECK(lut[74*3] == 0, "window floor: below ramp black");
		CHECK(lut[126*3] == 255, "window ceiling: above ramp white");
	}

	printf(failures ? "FAILED: %d\n" : "ALL PASS: %d\n", failures);
	return failures ? 1 : 0;
}
