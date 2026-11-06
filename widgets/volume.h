/*
    Aeskulap - DICOM image viewer and network client
    Copyright (C) 2005  Alexander Pipelka
    Copyright (C) 2026  Lars Christian Nygard (volume/slab additions)

    This file is part of Aeskulap. Aeskulap is free software; you can
    redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#ifndef AESKULAP_VOLUME_H
#define AESKULAP_VOLUME_H

#include <glibmm/refptr.h>
#include <glibmm/object.h>
#include <glib.h>

#include <vector>
#include <string>

namespace ImagePool {
class Series;
class Instance;
}

/**
 * A 16-bit image stack sharing one grid (w x h), with per-slice z (mm),
 * carrying the per-series post-processing state shared by every Display
 * in a SeriesView: slab (slice thickness), projection, 3x3 kernel.
 *
 * Kernel and projection are applied together in project(): the kernel is
 * linear per slice, projections (max/min/avg) are not, so project()
 * kernels the slab slices on the fly. Cheap for thin slabs.
 */
class Volume : public Glib::Object {
public:

	enum Projection {
		PASS,       // single slice (center)
		MAXIP,      // maximum intensity projection
		MINIP,      // minimum intensity projection
		AVGIP       // average intensity projection (slice thickness)
	};

	static Glib::RefPtr<Volume> create();

	/** copy one instance's 16-bit pixels in; false if grid mismatch/cap */
	bool append_slice(const guint16* pixels, int w, int h, double z,
	                  const Glib::RefPtr<ImagePool::Instance>& inst);

	int width() const { return m_w; }
	int height() const { return m_h; }
	int slices() const { return (int)m_zs.size(); }
	double z(int i) const { return m_zs[i]; }

	/** index (into z()) of the slice nearest world-z */
	int nearest_slice(double z) const;
	int slice_for_instance(const std::string& sopuid, int fallback) const;

	/** slab thickness in slices, >= 1 (0 disables) */
	void set_slab(int n);
	int slab() const { return m_slab; }
	double slab_mm() const;

	void set_projection(Projection p);
	Projection projection() const { return m_proj; }

	/** 3x3 kernel id (see displaykernel.h) */
	void set_kernel(int id);
	int kernel() const { return m_kernel; }

	const guint16* raw_slice(int i) const {
		return (i < 0 || i >= slices() || m_raw.empty()) ? NULL : &m_raw[(size_t)i * m_w * m_h];
	}

	const std::string& description() const { return m_description; }
	void set_description(const std::string& d) { m_description = d; }

	/** bump on any state change; Display uses it to invalidate its cache */
	guint64 revision() const { return m_revision; }

	/**
	 * Kernel the slab slices around `center`, project, write to out
	 * (resized). slab<=1 or PASS: just the (kerneld) center slice.
	 */
	void project(int center, std::vector<guint16>& out) const;

	/** build a volume from all 16-bit instances of a series (z-sorted) */
	static Glib::RefPtr<Volume> from_series(const Glib::RefPtr<ImagePool::Series>& series);

	/** raw pixel cap: series stacks beyond this don't get a volume */
	static const size_t MAX_BYTES = 512u * 1024u * 1024u;

protected:

	Volume();

	int m_w;
	int m_h;
	std::vector<double> m_zs;
	std::vector<std::string> m_uids;
	std::vector<guint16> m_raw;
	int m_slab;
	Projection m_proj;
	int m_kernel;
	guint64 m_revision;
	bool m_signed;   // pixels are two's-complement (CT HU)
	std::string m_description;
};

#endif // AESKULAP_VOLUME_H
