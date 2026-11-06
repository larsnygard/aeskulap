// aeskulap 0.2.9 — series arithmetic (Sub, MIP) engine.
// Pure logic: reads pixel data from Instance objects, writes result DICOM
// files to a directory through DCMTK's dcmImageHelper. Lives here (not in
// imagepool/) so the imagepool static library keeps its dependency shape;
// the engine is UI-free and unit-testable headless.
#ifndef __SERIESMATH_H
#define __SERIESMATH_H

#include <glibmm.h>
#include <string>
#include <vector>
#include <list>
#include "poolinstance.h"

namespace Aeskulap {

class SeriesMath {
public:
    struct Result {
        int frames;
        int width;
        int height;
        bool is_signed;
    };

    // Sub: for each (frame,row,col) compute sum(A) - sum(B). Output values
    // are saturated int16. Both series must have identical frames/width/height.
    static Result subtract(const std::vector<Glib::RefPtr<ImagePool::Instance> >& a,
                           const std::vector<Glib::RefPtr<ImagePool::Instance> >& b,
                           const std::string& outdir,
                           const std::string& studyuid,
                           const std::string& seriesuid,
                           const std::string& description);

    // MIP: per (row,col) take the max over all frames of the series.
    static Result maximum_intensity_project(const std::vector<Glib::RefPtr<ImagePool::Instance> >& a,
                                            const std::string& outdir,
                                            const std::string& studyuid,
                                            const std::string& seriesuid,
                                            const std::string& description);

    // For tests/debug: does the instance data load?
    static bool load_pixels(const Glib::RefPtr<ImagePool::Instance>& inst,
                            std::vector<gint32>& out, int& w, int& h, bool& is_signed);

private:
    static bool write_frame(const std::vector<gint16>& frame, int w, int h,
                            const std::string& path,
                            const std::string& studyuid,
                            const std::string& seriesuid,
                            const std::string& description,
                            int frame_index, int total_frames);
};

} // namespace Aeskulap

#endif // __SERIESMATH_H
