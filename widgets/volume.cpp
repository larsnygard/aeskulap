/*
    Aeskulap volume / slab projection core
    Copyright (C) 2026  Lars Christian Nygard

    GPL v2+; see volume.h header.
*/

#include "volume.h"
#include "displaykernel.h"
#include "poolinstance.h"
#include "poolseries.h"

#include <algorithm>
#include <cmath>
#include <cstring>

static inline int clampi_range_local(int v, int lo, int hi) {
	if(v < lo) return lo;
	if(v > hi) return hi;
	return v;
}

Volume::Volume() :
m_w(0),
m_h(0),
m_slab(1),
m_proj(Volume::PASS),
m_kernel(Aeskulap::Kernel::IDENTITY),
m_revision(1),
m_signed(false),
m_axis(Volume::AXIS_Z),
m_cx(0), m_cy(0), m_cz(0),
m_plane_rev(0) {
}

Glib::RefPtr<Volume> Volume::create() {
	return Glib::RefPtr<Volume>(new Volume);
}

bool Volume::append_slice(const guint16* pixels, int w, int h, double z,
                          const Glib::RefPtr<ImagePool::Instance>& inst) {
	if(!pixels || w <= 0 || h <= 0) {
		return false;
	}
	if(m_w == 0) {
		m_w = w;
		m_h = h;
	}
	if(w != m_w || h != m_h) {
		return false;    // grid mismatch: not part of this volume
	}
	if(m_raw.size() + (size_t)w * h > MAX_BYTES / sizeof(guint16)) {
		return false;    // volume too large: series stays slice-by-slice
	}
	if(inst) {
		m_signed = inst->is_signed();
	}
	size_t base = m_raw.size();
	m_raw.resize(base + (size_t)w * h);
	std::memcpy(&m_raw[base], pixels, (size_t)w * h * sizeof(guint16));
	m_zs.push_back(z);
	m_uids.push_back(inst ? inst->sopinstanceuid() : std::string());
	m_revision++;
	return true;
}

int Volume::nearest_slice(double z) const {
	if(m_zs.empty()) {
		return 0;
	}
	int best = 0;
	double bestd = std::fabs(m_zs[0] - z);
	for(size_t i = 1; i < m_zs.size(); i++) {
		double d = std::fabs(m_zs[i] - z);
		if(d < bestd) { bestd = d; best = (int)i; }
	}
	return best;
}

int Volume::slice_for_instance(const std::string& sopuid, int fallback) const {
	for(size_t i = 0; i < m_uids.size(); i++) {
		if(m_uids[i] == sopuid) {
			return (int)i;
		}
	}
	return fallback;
}

void Volume::set_slab(int n) {
	if(n < 1) n = 1;
	if(n > slices()) n = slices();
	if(n == m_slab) {
		return;
	}
	m_slab = n;
	m_revision++;
}

double Volume::slab_mm() const {
	if(m_zs.size() < 2 || m_slab <= 1) {
		return 0;
	}
	double pitch = (m_zs.back() - m_zs.front()) / (double)(m_zs.size() - 1);
	return std::fabs(pitch) * m_slab;
}

void Volume::set_projection(Projection p) {
	if(p == m_proj) {
		return;
	}
	m_proj = p;
	m_revision++;
}

void Volume::set_kernel(int id) {
	if(id == m_kernel) {
		return;
	}
	m_kernel = id;
	m_revision++;
}

void Volume::project(int center, std::vector<guint16>& out) const {
	size_t n = (size_t)m_w * m_h;
	if(n == 0 || m_raw.empty()) {
		out.clear();
		return;
	}
	int count = slices();
	if(center < 0) center = 0;
	if(center >= count) center = count - 1;

	out.assign(n, 0);

	const bool kerned = (m_kernel != Aeskulap::Kernel::IDENTITY);
	std::vector<guint16> tmp;
	if(kerned) {
		tmp.resize(n);
	}
	// returns pointer to the (optionally kerned) slice s
	auto slice_k = [&](int s) -> const guint16* {
		const guint16* q = &m_raw[(size_t)s * n];
		if(!kerned) {
			return q;
		}
		Aeskulap::Kernel::apply(m_kernel, q, &tmp[0], m_w, m_h, m_signed);
		return &tmp[0];
	};

	int slab = m_slab > 0 ? m_slab : 1;

	if(m_proj == PASS || slab <= 1) {
		std::memcpy(&out[0], slice_k(center), n * sizeof(guint16));
		return;
	}

	int lo = center - (slab - 1)/2;
	int hi = lo + slab - 1;
	if(lo < 0) { hi -= lo; lo = 0; }
	if(hi >= count) { lo -= (hi - count + 1); hi = count - 1; }
	if(lo < 0) lo = 0;

	// AVGIP accumulates in uint32; MINIP/MAXIP fold over the running result.
	std::vector<guint32> acc;
	if(m_proj == AVGIP) {
		acc.assign(n, 0);
		for(int s = lo; s <= hi; s++) {
			const guint16* q = slice_k(s);
			for(size_t i = 0; i < n; i++) {
				acc[i] += q[i];
			}
		}
		guint32 cnt = (guint32)(hi - lo + 1);
		for(size_t i = 0; i < n; i++) {
			out[i] = (guint16)(acc[i] / cnt);
		}
		return;
	}

	std::memcpy(&out[0], slice_k(lo), n * sizeof(guint16));
	for(int s = lo + 1; s <= hi; s++) {
		const guint16* q = slice_k(s);
		if(m_proj == MINIP) {
			for(size_t i = 0; i < n; i++) {
				if(q[i] < out[i]) out[i] = q[i];
			}
		}
		else { // MAXIP
			for(size_t i = 0; i < n; i++) {
				if(q[i] > out[i]) out[i] = q[i];
			}
		}
	}
}

Glib::RefPtr<Volume> Volume::from_series(const Glib::RefPtr<ImagePool::Series>& series) {
	if(!series || series->size() == 0) {
		return Glib::RefPtr<Volume>();
	}
	// collect 16-bit instances with pixel data; z-sorted
	struct Item { double z; Glib::RefPtr<ImagePool::Instance> inst; };
	std::vector<Item> items;
	for(ImagePool::Series::iterator i = series->begin(); i != series->end(); ++i) {
		const Glib::RefPtr<ImagePool::Instance>& inst = i->second;
		if(!inst || inst->bpp() != 16 || inst->iscolor()) {
			continue;
		}
		if(!inst->pixels(0)) {
			continue;
		}
		Item it;
		it.z = inst->get_position().z;
		it.inst = inst;
		items.push_back(it);
	}
	if(items.size() < 2) {
		return Glib::RefPtr<Volume>();
	}
	std::stable_sort(items.begin(), items.end(),
	                 [](const Item& a, const Item& b) { return a.z < b.z; });

	Glib::RefPtr<Volume> vol = Volume::create();
	for(size_t i = 0; i < items.size(); i++) {
		if(!vol->append_slice((const guint16*)items[i].inst->pixels(0),
		                      items[i].inst->width(), items[i].inst->height(),
		                      items[i].z, items[i].inst)) {
			break;   // grid mismatch / size cap
		}
	}
	if(vol->slices() < 2) {
		return Glib::RefPtr<Volume>();
	}
	vol->set_description(series->description());
	return vol;
}

void Volume::set_center(int x, int y, int z) {
	int nx = clampi_range_local(x, 0, m_w ? m_w - 1 : 0);
	int ny = clampi_range_local(y, 0, m_h ? m_h - 1 : 0);
	int nz = clampi_range_local(z, 0, slices() ? slices() - 1 : 0);
	if(nx == m_cx && ny == m_cy && nz == m_cz) {
		return;
	}
	m_cx = nx; m_cy = ny; m_cz = nz;
	m_revision++;
}

const guint16* Volume::plane(int& w, int& h) {
	if(m_raw.empty() || m_w == 0 || m_h == 0) {
		w = h = 0;
		return NULL;
	}
	int pw = m_w, ph = m_h;
	switch(m_axis) {
		case AXIS_Y: ph = slices(); break;
		case AXIS_X: pw = m_h; ph = slices(); break;
		default: break;
	}
	// rebuild only when geometry/state changed
	if(m_plane_rev != m_revision || (int)m_plane_cache.size() != pw * ph) {
		m_plane_cache.resize((size_t)pw * ph);
		if(m_axis == AXIS_Z) {
			int z = clampi_range_local(m_cz, 0, slices() - 1);
			const guint16* src = raw_slice(z);
			if(src) {
				std::memcpy(&m_plane_cache[0], src, (size_t)m_w * m_h * sizeof(guint16));
			}
		}
		else if(m_axis == AXIS_Y) {
			// coronal: out(x, k) = vol(x, m_cy, k)
			int y = clampi_range_local(m_cy, 0, m_h - 1);
			for(int k = 0; k < slices(); k++) {
				const guint16* sl = raw_slice(k);
				if(!sl) continue;
				for(int x = 0; x < m_w; x++) {
					m_plane_cache[(size_t)k * m_w + x] = sl[(size_t)y * m_w + x];
				}
			}
		}
		else {
			// sagittal: plane(row k, col y) = vol(m_cx, y, k); pw = m_h
			int x = clampi_range_local(m_cx, 0, m_w - 1);
			for(int k = 0; k < slices(); k++) {
				const guint16* sl = raw_slice(k);
				if(!sl) continue;
				for(int y = 0; y < m_h; y++) {
					m_plane_cache[(size_t)k * m_h + y] = sl[(size_t)y * m_w + x];
				}
			}
		}
		// kernels run signed-domain over slices; on the plane the geometry
		// differs, so kernel is applied only in project(); planes raw.
		m_plane_rev = m_revision;
	}
	w = pw;
	h = ph;
	return &m_plane_cache[0];
}
