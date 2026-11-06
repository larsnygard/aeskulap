/*
    Aeskulap palette LUT builder
    Copyright (C) 2026  Lars Christian Nygard

    GPL v2+; see volume.h header.
*/

#include "displaypalette.h"

#include <cmath>
#include <cstring>

namespace Aeskulap {
namespace Palette {

namespace {

struct RGB { double r, g, b; };

inline double clampd(double v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

// piecewise-linear ramp through control points (t in 0..1)
RGB ramp(const RGB* pts, int n, double t) {
	if(t <= 0) return pts[0];
	if(t >= 1) return pts[n-1];
	for(int i = 0; i < n-1; i++) {
		if(t <= (double)(i+1)/(n-1)) {
			double f = (t - (double)i/(n-1)) * (n-1);
			return RGB{ pts[i].r + (pts[i+1].r-pts[i].r)*f,
			            pts[i].g + (pts[i+1].g-pts[i].g)*f,
			            pts[i].b + (pts[i+1].b-pts[i].b)*f };
		}
	}
	return pts[n-1];
}

// 0..1 intensity -> RGB
void map_color(int id, double t, guint8& r, guint8& g, guint8& b) {
	t = clampd(t);
	switch(id) {
		case INVERSE:
			r = g = b = (guint8)(255 * (1.0 - t));
			return;
		case HOT_IRON: {
			static const RGB pts[] = {
				{0,0,0}, {0.2,0,0.4}, {0.9,0.1,0.1}, {1,0.6,0}, {1,1,0.6}, {1,1,1}
			};
			RGB c = ramp(pts, 6, t);
			r = (guint8)(c.r*255); g = (guint8)(c.g*255); b = (guint8)(c.b*255);
			return;
		}
		case COLORSTET: {
			// "Stet" color: blue -> red through green
			static const RGB pts[] = {
				{0,0,0.4}, {0,0.3,1}, {0,1,0.4}, {1,1,0}, {1,0.2,0}
			};
			RGB c = ramp(pts, 5, t);
			r = (guint8)(c.r*255); g = (guint8)(c.g*255); b = (guint8)(c.b*255);
			return;
		}
		case RAINBOW: {
			static const RGB pts[] = {
				{0.1,0.1,0.6}, {0,0.8,1}, {0,1,0.2}, {1,1,0}, {1,0.3,0}, {0.8,0,0}
			};
			RGB c = ramp(pts, 6, t);
			r = (guint8)(c.r*255); g = (guint8)(c.g*255); b = (guint8)(c.b*255);
			return;
		}
		case JET: {
			// classic jet: dark blue -> cyan -> green -> yellow -> red -> dark red
			static const RGB pts[] = {
				{0,0,0.5}, {0,0,1}, {0,1,1}, {0.5,1,0.2}, {1,1,0}, {1,0,0}, {0.5,0,0}
			};
			RGB c = ramp(pts, 7, t);
			r = (guint8)(c.r*255); g = (guint8)(c.g*255); b = (guint8)(c.b*255);
			return;
		}
		case GREEN_AMNIOTIC: {
			static const RGB pts[] = {
				{0,0,0}, {0,0.4,0.1}, {0,1,0.3}, {0.6,1,0.7}, {1,1,1}
			};
			RGB c = ramp(pts, 5, t);
			r = (guint8)(c.r*255); g = (guint8)(c.g*255); b = (guint8)(c.b*255);
			return;
		}
		default: // GRAY
			r = g = b = (guint8)(255 * t);
			return;
	}
}

} // namespace

int id_from_name(const std::string& nm) {
	for(int i = 0; i < ID_COUNT; i++) {
		if(nm == name(i)) return i;
	}
	return -1;
}

const char* name(int id) {
	switch(id) {
		case INVERSE:        return "inverse";
		case HOT_IRON:       return "hot iron";
		case COLORSTET:      return "colorstet";
		case RAINBOW:        return "rainbow";
		case JET:            return "jet";
		case GREEN_AMNIOTIC: return "green amniotic";
		default:             return "grayscale";
	}
}

const char* label(int id) {
	switch(id) {
		case INVERSE:        return "Inverse";
		case HOT_IRON:       return "Hot Iron";
		case COLORSTET:      return "ColorStet";
		case RAINBOW:        return "Rainbow";
		case JET:            return "Jet";
		case GREEN_AMNIOTIC: return "Green Amniotic";
		default:             return "Grayscale";
	}
}

void build_lut(guint8* lut, int n, int palette_id, bool invert, int center, int width) {
	if(width < 1) width = 1;
	// ramp [lo, hi] in value space
	double lo = (double)center - width/2.0;
	double hi = (double)center + width/2.0;

	for(int i = 0; i < n; i++) {
		double t = ((double)i - lo) / (hi - lo);
		guint8 r, g, b;
		if(t < 0.0) {
			// below the ramp: floor (black for gray; palette color at 0)
			map_color(palette_id, 0.0, r, g, b);
			if(palette_id == GRAY) { r = g = b = 0; }
			else if(palette_id == INVERSE) { r = g = b = 255; }
		}
		else if(t > 1.0) {
			map_color(palette_id, 1.0, r, g, b);
			if(palette_id == GRAY) { r = g = b = 255; }
			else if(palette_id == INVERSE) { r = g = b = 0; }
		}
		else {
			map_color(palette_id, t, r, g, b);
		}
		if(invert) { r = 255-r; g = 255-g; b = 255-b; }
		lut[i*3+0] = r;
		lut[i*3+1] = g;
		lut[i*3+2] = b;
	}
}

} // namespace Palette
} // namespace Aeskulap
