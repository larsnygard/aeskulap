// Headless test: CPR engine (spline + straightened reformat).
#include "cpr.h"
#include "volume.h"
#include "poolinstance.h"

using Aeskulap::Cpr;

#include <cstdio>
#include <cmath>
#include <vector>

static int failures = 0;
#define CHECK(cond, name) do { \
	if(cond) { printf("PASS %s\n", name); } \
	else { printf("FAIL %s\n", name); failures++; } \
} while(0)

int main()
{
	printf("== CPR tests ==\n");

	// ---- spline: straight line, ~1px spacing ----
	{
		std::vector<Cpr::Point> pts = {{0, 0}, {10, 0}};
		std::vector<Cpr::Point> p = Cpr::spline(pts);
		CHECK(p.size() >= 10 && p.size() <= 12, "spline straight 10px -> ~10 samples");
		CHECK(std::fabs(p.front().x) < 1e-9 && std::fabs(p.back().x - 10) < 1e-6,
		      "spline endpoints exact");
		bool monotone = true;
		for(size_t i = 1; i < p.size(); i++) {
			if(p[i].x < p[i-1].x - 1e-9) monotone = false;
		}
		CHECK(monotone, "spline straight monotone in x");
	}

	// ---- spline: curve passes near control points, length > chord ----
	{
		std::vector<Cpr::Point> pts = {{0,0},{10,10},{20,0},{30,10}};
		std::vector<Cpr::Point> p = Cpr::spline(pts);
		CHECK(p.size() > 30, "spline curve longer than straight chord");
		double len = 0;
		for(size_t i = 1; i < p.size(); i++) {
			len += std::sqrt(std::pow(p[i].x - p[i-1].x, 2) + std::pow(p[i].y - p[i-1].y, 2));
		}
		CHECK(len > 31.0, "curve arc length > 31 (chord 30)");
	}

	// ---- reformat: 3D volume, value = slice index; straight center row ----
	{
		Glib::RefPtr<Volume> vol = Volume::create();
		for(int s = 0; s < 6; s++) {
			std::vector<guint16> sl(100, (guint16)(s * 10));   // 10x10
			vol->append_slice(&sl[0], 10, 10, s * 2.0, Glib::RefPtr<ImagePool::Instance>());
		}
		std::vector<gint16> out;
		int w = 0, h = 0;
		std::vector<Cpr::Point> pts = {{0, 5}, {9, 5}};
		int frames = Cpr::reformat(vol, pts, out, w, h);
		CHECK(frames == 1, "CPR 1 frame");
		CHECK(h == 6, "CPR rows = slices");
		CHECK(w >= 9, "CPR cols ~ path length");
		CHECK(out[0 * w + w/2] == 0 && out[5 * w + w/2] == 50,
		      "CPR rows follow slice index (row0=0, row5=50)");
		// interpolation between rows: row 0..5 exact, mid column same value
		CHECK(out[3 * w + 0] == 30, "CPR row3 = 30 across columns");
	}

	// ---- reformat: curved path tracks value gradient in-plane ----
	{
		Glib::RefPtr<Volume> vol = Volume::create();
		// each slice: value = x (0..9), all slices equal
		for(int s = 0; s < 3; s++) {
			std::vector<guint16> sl;
			for(int y = 0; y < 10; y++)
				for(int x = 0; x < 10; x++)
					sl.push_back((guint16)x);
			vol->append_slice(&sl[0], 10, 10, s * 2.0, Glib::RefPtr<ImagePool::Instance>());
		}
		std::vector<gint16> out;
		int w = 0, h = 0;
		// curve that sweeps left-right-left: x(t) varies
		std::vector<Cpr::Point> pts = {{0,5},{9,5},{0,5}};
		int frames = Cpr::reformat(vol, pts, out, w, h);
		CHECK(frames == 1, "CPR curve 1 frame");
		// every row identical (all slices equal)
		bool same = true;
		for(int c = 0; c < w; c++) {
			if(out[0*w+c] != out[1*w+c]) same = false;
		}
		CHECK(same, "CPR rows identical for z-constant volume");
		// at path endpoints x=0 -> value 0; at curve middle x=9 -> value ~9
		CHECK(std::abs(out[0*w+0]) < 2, "CPR start value ~0");
		CHECK(std::abs(out[w/2] - 9) < 2, "CPR mid value ~9 (path peak x=9)");
		CHECK(std::abs(out[0*w + w-1]) < 2, "CPR end value ~0");
	}

	printf(failures ? "FAILURES: %d\n" : "ALL PASS\n", failures);
	return failures ? 1 : 0;
}
