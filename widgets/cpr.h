// aeskulap 0.2.9 — Curved Planar Reformation (CPR) engine.
// Pure logic, headless-testable. A 2D path (Catmull-Rom spline through user
// control points) runs in the axial plane: x/y in PIXEL units, z in mm
// (Volume stores no pixel spacing). The straightened output has:
//   columns = arc length along the path (sample spacing = 1 pixel)
//   rows    = slices (top = lowest z), sampled bilinear in (x,y),
//             linear in z between slices.
// Output is raw two's-complement int16 (Volume's stored domain).
#ifndef __CPR_H
#define __CPR_H

#include <glibmm.h>
#include <vector>
#include <utility>
#include "volume.h"

namespace Aeskulap {

class Cpr {
public:
    struct Point { double x; double y; };   // axial plane, pixel units

    // Catmull-Rom through pts, arc-length sampled with ~1 px spacing.
    // pts.size() < 2 -> empty. Closed curve when first == last.
    static std::vector<Point> spline(const std::vector<Point>& pts);

    // Straightened image. Returns frame count (always 1 on success, 0 on
    // failure); w = spline samples, h = volume slices. out is w*h int16.
    static int reformat(const Glib::RefPtr<Volume>& vol,
                        const std::vector<Point>& control_points,
                        std::vector<gint16>& out, int& w, int& h);
};

} // namespace Aeskulap

#endif // __CPR_H
