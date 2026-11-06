// aeskulap 0.2.9 — CPR engine implementation.
#include "cpr.h"

#include <cmath>
#include <algorithm>

namespace Aeskulap {

std::vector<Cpr::Point> Cpr::spline(const std::vector<Point>& pts)
{
	std::vector<Point> out;
	size_t n = pts.size();
	if(n < 2) {
		return out;
	}

	bool closed = (std::fabs(pts.front().x - pts.back().x) < 1e-9 &&
	               std::fabs(pts.front().y - pts.back().y) < 1e-9);

	// P(i) with endpoint clamping (open) / wrapping (closed)
	auto P = [&](long i) -> Point {
		if(closed) {
			long m = (long)(n - 1);           // pts[n-1] == pts[0]
			long k = ((i % m) + m) % m;
			return pts[(size_t)k];
		}
		if(i < 0) i = 0;
		if((size_t)i >= n) i = (long)n - 1;
		return pts[(size_t)i];
	};

	long segs = closed ? (long)(n - 1) : (long)(n - 1);
	// dense pre-sampling, then decimate by arc length
	const int SUB = 32;
	std::vector<Point> dense;
	for(long s = 0; s < segs; s++) {
		Point p0 = P(s - 1), p1 = P(s), p2 = P(s + 1), p3 = P(s + 2);
		for(int j = 0; j < SUB; j++) {
			double t = (double)j / SUB;
			double t2 = t * t, t3 = t2 * t;
			Point q;
			q.x = 0.5 * ((2 * p1.x) + (-p0.x + p2.x) * t +
			             (2 * p0.x - 5 * p1.x + 4 * p2.x - p3.x) * t2 +
			             (-p0.x + 3 * p1.x - 3 * p2.x + p3.x) * t3);
			q.y = 0.5 * ((2 * p1.y) + (-p0.y + p2.y) * t +
			             (2 * p0.y - 5 * p1.y + 4 * p2.y - p3.y) * t2 +
			             (-p0.y + 3 * p1.y - 3 * p2.y + p3.y) * t3);
			dense.push_back(q);
		}
	}
	dense.push_back(P(segs));   // endpoint (open: pts[n-1]; closed: pts[0])

	// arc-length decimation to ~1 px spacing (threshold walker, no drift)
	out.push_back(dense.front());
	double cum = 0.0, next = 1.0;
	for(size_t i = 1; i < dense.size(); i++) {
		double dx = dense[i].x - dense[i - 1].x;
		double dy = dense[i].y - dense[i - 1].y;
		cum += std::sqrt(dx * dx + dy * dy);
		if(cum >= next) {
			out.push_back(dense[i]);
			next += 1.0;
		}
	}
	if(out.back().x != dense.back().x || out.back().y != dense.back().y) {
		out.push_back(dense.back());
	}
	if(out.size() < 2) {
		out.push_back(dense.back());
	}
	return out;
}

int Cpr::reformat(const Glib::RefPtr<Volume>& vol,
                  const std::vector<Point>& control_points,
                  std::vector<gint16>& out, int& w, int& h)
{
	w = h = 0;
	out.clear();
	if(!vol || vol->slices() < 2) {
		return 0;
	}
	std::vector<Point> path = spline(control_points);
	if(path.size() < 2) {
		return 0;
	}

	int vw = vol->width(), vh = vol->height(), vs = vol->slices();
	w = (int)path.size();
	h = vs;
	out.assign((size_t)w * h, 0);

	// z range from the volume's slice positions
	double z0 = vol->z(0);
	double z1 = vol->z(vs - 1);
	double dz = (vs > 1) ? (z1 - z0) / (vs - 1) : 1.0;
	if(dz < 0) { z0 = z1; dz = -dz; }

	auto fetch = [&](double x, double y, int si) -> double {
		if(si < 0) si = 0;
		if(si >= vs) si = vs - 1;
		const guint16* s = vol->raw_slice(si);
		if(!s) {
			return 0.0;
		}
		// bilinear in (x,y), clamped
		double fx = x - 0.5, fy = y - 0.5;
		int x0 = (int)std::floor(fx), y0 = (int)std::floor(fy);
		double tx = fx - x0, ty = fy - y0;
		if(x0 < 0) { x0 = 0; tx = 0.0; }
		if(y0 < 0) { y0 = 0; ty = 0.0; }
		if(x0 > vw - 1) { x0 = vw - 1; tx = 1.0; }
		if(y0 > vh - 1) { y0 = vh - 1; ty = 1.0; }
		int x1 = std::min(x0 + 1, vw - 1);
		int y1 = std::min(y0 + 1, vh - 1);
		double v00 = (gint16)s[(size_t)y0 * vw + x0];
		double v01 = (gint16)s[(size_t)y0 * vw + x1];
		double v10 = (gint16)s[(size_t)y1 * vw + x0];
		double v11 = (gint16)s[(size_t)y1 * vw + x1];
		return (v00 * (1 - tx) + v01 * tx) * (1 - ty) +
		       (v10 * (1 - tx) + v11 * tx) * ty;
	};

	for(int col = 0; col < w; col++) {
		double x = path[col].x, y = path[col].y;
		// row 0 = lowest z; linear interpolation between slices
		for(int row = 0; row < h; row++) {
			double z = z0 + dz * row;
			double fs = (z - vol->z(0)) / dz;
			int s0 = (int)std::floor(fs);
			double tz = fs - s0;
			if(s0 < 0) { s0 = 0; tz = 0.0; }
			if(s0 > vs - 1) { s0 = vs - 1; tz = 0.0; }
			int s1 = std::min(s0 + 1, vs - 1);
			double v = fetch(x, y, s0) * (1 - tz) + fetch(x, y, s1) * tz;
			long iv = (long)std::lround(v);
			if(iv > 32767) iv = 32767;
			if(iv < -32768) iv = -32768;
			out[(size_t)row * w + col] = (gint16)iv;
		}
	}
	return 1;
}

} // namespace Aeskulap
