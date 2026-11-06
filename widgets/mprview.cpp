/*
    Aeskulap MPR (multiplanar reconstruction) view
    Copyright (C) 2026  Lars Christian Nygard

    GPL v2+ (see asimpledisplay.h header).
*/

#include "mprview.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <cairomm/context.h>
#include <gdkmm/pixbuf.h>

static inline Gdk::EventMask em(int f) {
	return static_cast<Gdk::EventMask>(f);
}

#include "displaypalette.h"

// ============================================================================
// MPRView::Pane — one drawing surface over the shared crosshair plane
// ============================================================================

class MPRView::Pane : public Gtk::DrawingArea {
public:

	Pane(MPRView* view, Volume::Axis axis, const Glib::ustring& title) :
	m_view(view),
	m_axis(axis),
	m_title(title) {
		set_size_request(240, 240);
		add_events(em(GDK_SCROLL_MASK | GDK_BUTTON_PRESS_MASK |
		           GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK |
		           GDK_KEY_PRESS_MASK));
		set_can_focus(true);
		signal_draw().connect(sigc::mem_fun(*this, &Pane::on_draw), false);
		signal_key_press_event().connect(sigc::mem_fun(*this, &Pane::on_key_press), false);
		show();
	}

	Volume::Axis axis() const { return m_axis; }

private:

	bool on_draw(const ::Cairo::RefPtr<::Cairo::Context>& cr) {
		int aw = get_allocation().get_width();
		int ah = get_allocation().get_height();

		cr->set_source_rgb(0, 0, 0);
		cr->paint();

		int pw = 0, ph = 0;
		const guint16* plane = m_view->m_volume->plane(pw, ph);
		if(!plane || pw <= 0 || ph <= 0) {
			return true;
		}

		// render plane -> pixbuf (shared LUT already applied)
		if(m_cache_rev != m_view->m_revision ||
		   m_cache_pw != pw || m_cache_ph != ph) {
			m_pixbuf = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, false, 8, pw, ph);
			guint8* dst = m_pixbuf->get_pixels();
			int stride = m_pixbuf->get_rowstride();
			const guint8* lut = &m_view->m_lut[0];
			for(int y = 0; y < ph; y++) {
				guint8* row = dst + (long)y * stride;
				const guint16* src = plane + (long)y * pw;
				for(int x = 0; x < pw; x++) {
					guint16 v = src[x];
					row[x*3+0] = lut[(long)v*3+0];
					row[x*3+1] = lut[(long)v*3+1];
					row[x*3+2] = lut[(long)v*3+2];
				}
			}
			m_cache_rev = m_view->m_revision;
			m_cache_pw = pw;
			m_cache_ph = ph;
		}

		// fit into allocation
		double scale = std::min((double)aw / pw, (double)ah / ph);
		if(scale <= 0) {
			scale = 1.0;
		}
		double dw = pw * scale;
		double dh = ph * scale;
		double ox = (aw - dw) / 2.0;
		double oy = (ah - dh) / 2.0;

		gdk_cairo_set_source_pixbuf(cr->cobj(), m_pixbuf->gobj(), ox, oy);
		cairo_pattern_set_filter(cairo_get_source(cr->cobj()), CAIRO_FILTER_GOOD);
		cr->save();
		cr->translate(ox, oy);
		cr->scale(scale, scale);
		cr->translate(-ox, -oy);
		cr->paint();
		cr->restore();

		// crosshair (shared center)
		int cx = 0, cy = 0, cz = 0;
		m_view->m_volume->center(cx, cy, cz);
		int col = cx, row = cy;
		switch(m_axis) {
			case Volume::AXIS_Z: col = cx; row = cy; break;            // axial
			case Volume::AXIS_Y: col = cx; row = cz; break;            // coronal
			case Volume::AXIS_X: col = cy; row = cz; break;            // sagittal
		}
		double ccx = ox + (col + 0.5) * scale;
		double ccy = oy + (row + 0.5) * scale;
		cr->set_source_rgb(0.2, 0.9, 0.2);
		cr->set_line_width(1.0);
		cr->move_to(ox, ccy); cr->line_to(ox + dw, ccy);
		cr->move_to(ccx, oy); cr->line_to(ccx, oy + dh);
		cr->stroke();

		// CPR overlay
		if(m_view->m_cpr_mode && !m_view->m_cpr_pts.empty()) {
			cr->set_source_rgb(1.0, 0.55, 0.1);
			cr->set_line_width(1.5);
			const std::vector<Cpr::Point>& pts = m_view->m_cpr_pts;
			if(m_axis == Volume::AXIS_Z) {
				for(size_t i = 0; i < pts.size(); i++) {
					double px = ox + (pts[i].x + 0.5) * scale;
					double py = oy + (pts[i].y + 0.5) * scale;
					cr->arc(px, py, 3.0, 0, 2 * M_PI);
					cr->stroke();
					if(i) {
						double qx = ox + (pts[i-1].x + 0.5) * scale;
						double qy = oy + (pts[i-1].y + 0.5) * scale;
						cr->move_to(qx, qy); cr->line_to(px, py); cr->stroke();
					}
				}
			}
			else {
				// project points at the current crosshair z-row
				double row_px = oy + (cz + 0.5) * scale;
				for(size_t i = 0; i < pts.size(); i++) {
					double v = (m_axis == Volume::AXIS_Y) ? pts[i].x : pts[i].y;
					double px = ox + (v + 0.5) * scale;
					cr->move_to(px, row_px - 4); cr->line_to(px, row_px + 4);
					cr->stroke();
				}
			}
			char msg[96];
			g_snprintf(msg, sizeof(msg), "CPR: %zu pkt (Enter=BEREKNE, Esc=-nullstill)",
			           pts.size());
			cr->set_source_rgb(1.0, 0.55, 0.1);
			cr->move_to(ox + 6, oy + ph * scale - 8);
			cr->show_text(msg);
		}

		// label + index
		int idx = (m_axis == Volume::AXIS_Z) ? cz :
		          (m_axis == Volume::AXIS_Y) ? cy : cx;
		char buf[96];
		g_snprintf(buf, sizeof(buf), "%s  #%d", m_title.c_str(), idx);
		cr->set_source_rgb(0.8, 0.8, 0.8);
		cr->set_font_size(12);
		cr->move_to(ox + 6, oy + 16);
		cr->show_text(buf);

		return true;
	}

	bool on_scroll_event(GdkEventScroll* e) {
		int d = (e->direction == GDK_SCROLL_UP) ? -1 :
		        (e->direction == GDK_SCROLL_DOWN) ? 1 : 0;
		if(e->direction == GDK_SCROLL_SMOOTH) {
			double delta = 0;
			if(gdk_event_get_scroll_deltas((GdkEvent*)e, NULL, &delta)) {
				d = (delta > 0) ? 1 : (delta < 0) ? -1 : 0;
			}
		}
		if(!d) {
			return false;
		}
		int cx = 0, cy = 0, cz = 0;
		m_view->m_volume->center(cx, cy, cz);
		int steps = (e->state & GDK_SHIFT_MASK) ? 5 : 1;
		switch(m_axis) {
			case Volume::AXIS_Z: cz += d * steps; break;
			case Volume::AXIS_Y: cy += d * steps; break;
			case Volume::AXIS_X: cx += d * steps; break;
		}
		m_view->m_volume->set_center(cx, cy, cz);
		m_view->rebuild_all();
		return true;
	}

	bool on_button_press_event(GdkEventButton* e) {
		if(e->button == 3) {
			// right-drag = move crosshair to click
			set_crosshair(e->x, e->y);
			return true;
		}
		if(e->button == 1 && m_axis == Volume::AXIS_Z && m_view->m_cpr_mode) {
			add_cpr_point(e->x, e->y);
			return true;
		}
		if(e->button == 1) {
			m_dragging = true;
			m_drag_x = e->x;
			m_drag_y = e->y;
			m_drag_wc = m_view->m_wc;
			m_drag_ww = m_view->m_ww;
			return true;
		}
		return false;
	}

	bool on_button_release_event(GdkEventButton* e) {
		if(e->button == 1) {
			m_dragging = false;
			return true;
		}
		return false;
	}

	bool on_motion_notify_event(GdkEventMotion* e) {
		if(m_dragging) {
			int c = m_drag_wc + (int)(e->x - m_drag_x);
			int w = m_drag_ww + (int)(e->y - m_drag_y);
			if(w < 1) {
				w = 1;
			}
			m_view->set_window(c, w);
			return true;
		}
		if(e->state & GDK_BUTTON3_MASK) {
			set_crosshair(e->x, e->y);
			return true;
		}
		return false;
	}

	bool on_key_press(GdkEventKey* e) {
		if(e->keyval == GDK_KEY_Return || e->keyval == GDK_KEY_KP_Enter) {
			if(m_view->m_cpr_mode && m_view->m_cpr_pts.size() >= 2) {
				m_view->signal_cpr_compute();
			}
			return true;
		}
		if(e->keyval == GDK_KEY_Escape) {
			m_view->clear_cpr_points();
			return true;
		}
		return false;
	}

	void add_cpr_point(double x, double y) {
		int col = 0, row = 0;
		if(!image_coords(x, y, col, row)) {
			return;
		}
		m_view->m_cpr_pts.push_back(Cpr::Point{(double)col, (double)row});
		m_view->rebuild_all();
	}

	// widget px -> plane col/row (false if outside)
	bool image_coords(double x, double y, int& col, int& row) {
		int aw = get_allocation().get_width();
		int ah = get_allocation().get_height();
		int pw = m_cache_pw, ph = m_cache_ph;
		if(pw <= 0 || ph <= 0) {
			return false;
		}
		double scale = std::min((double)aw / pw, (double)ah / ph);
		double dw = pw * scale, dh = ph * scale;
		double ox = (aw - dw) / 2.0, oy = (ah - dh) / 2.0;
		col = (int)((x - ox) / scale);
		row = (int)((y - oy) / scale);
		return col >= 0 && col < pw && row >= 0 && row < ph;
	}

	void set_crosshair(double x, double y) {
		int aw = get_allocation().get_width();
		int ah = get_allocation().get_height();
		int pw = m_cache_pw, ph = m_cache_ph;
		if(pw <= 0 || ph <= 0) {
			return;
		}
		double scale = std::min((double)aw / pw, (double)ah / ph);
		double dw = pw * scale, dh = ph * scale;
		double ox = (aw - dw) / 2.0, oy = (ah - dh) / 2.0;
		int col = (int)((x - ox) / scale);
		int row = (int)((y - oy) / scale);
		if(col < 0 || col >= pw || row < 0 || row >= ph) {
			return;
		}
		int cx = 0, cy = 0, cz = 0;
		m_view->m_volume->center(cx, cy, cz);
		switch(m_axis) {
			case Volume::AXIS_Z: cx = col; cy = row; break;
			case Volume::AXIS_Y: cx = col; cz = row; break;
			case Volume::AXIS_X: cy = col; cz = row; break;
		}
		m_view->m_volume->set_center(cx, cy, cz);
		m_view->rebuild_all();
	}

	MPRView* m_view;
	Volume::Axis m_axis;
	Glib::ustring m_title;

	Glib::RefPtr<Gdk::Pixbuf> m_pixbuf;
	guint64 m_cache_rev = 0;
	int m_cache_pw = 0;
	int m_cache_ph = 0;

	bool m_dragging = false;
	double m_drag_x = 0, m_drag_y = 0;
	int m_drag_wc = 0, m_drag_ww = 0;
};

// ============================================================================
// MPRView
// ============================================================================

static const int LUT_N = 65536;

MPRView::MPRView(const Glib::RefPtr<Volume>& volume,
                 int center_x, int center_y, int center_z,
                 int window_center, int window_width) :
m_volume(volume),
m_wc(window_center),
m_ww(window_width),
m_invert(false),
m_palette(Aeskulap::Palette::GRAY),
m_revision(1) {
	m_center[0] = center_x;
	m_center[1] = center_y;
	m_center[2] = center_z;

	m_volume->set_center(center_x, center_y, center_z);

	m_lut.resize((size_t)LUT_N * 3);

	const int nsl = m_volume->slices();
	const int w = m_volume->width();
	const int h = m_volume->height();

	Gtk::Grid* grid = Gtk::manage(new Gtk::Grid);
	grid->set_column_spacing(2);
	grid->set_row_spacing(2);

	m_pane[0] = Gtk::manage(new Pane(this, Volume::AXIS_Z,
		Glib::ustring::compose("Axial %1x%2", w, h)));
	m_pane[1] = Gtk::manage(new Pane(this, Volume::AXIS_Y,
		Glib::ustring::compose("Coronal %1x%2", w, nsl)));
	m_pane[2] = Gtk::manage(new Pane(this, Volume::AXIS_X,
		Glib::ustring::compose("Sagittal %1x%2", h, nsl)));

	grid->attach(*m_pane[0], 0, 0, 1, 1);
	grid->attach(*m_pane[1], 1, 0, 1, 1);
	grid->attach(*m_pane[2], 2, 0, 1, 1);

	grid->show();
	add(*grid);

	rebuild_lut();
	rebuild_all();
}

void MPRView::set_window(int center, int width) {
	m_wc = center;
	m_ww = width;
	rebuild_lut();
	rebuild_all();
}

void MPRView::rebuild_lut() {
	int wd = m_ww;
	if(wd < 1) {
		wd = 1;
	}
	bool signed_vol = m_volume->is_signed();

	if(m_palette == Aeskulap::Palette::GRAY || m_palette < 0) {
		// gray ramp computed in HU space: index i is the raw stored pixel
		// (two's complement for signed), so decode i -> HU before the ramp.
		// legacy ramp: stored-index domain; for signed data flip the
		// center (cd = 65536 - center) exactly like SimpleDisplay's
		// invert_lut_shape path, so HU 0 maps identically.
		double cd = signed_vol ? (double)(65536 - m_wc) : (double)m_wc;
		double k = 256.0 / wd;
		double c0 = (k * (cd - wd / 2.0)) * -1.0;
		for(int i = 0; i < LUT_N; i++) {
			double g = k * i + c0;
			if(g > 255) {
				g = 255;
			}
			if(g < 0) {
				g = 0;
			}
			guint8 p = m_invert ? (guint8)(255 - (guint8)g) : (guint8)g;
			m_lut[(size_t)i*3+0] = p;
			m_lut[(size_t)i*3+1] = p;
			m_lut[(size_t)i*3+2] = p;
		}
	}
	else {
		// palette path ramps over the stored index domain; mirror the
		// signed ramp flip the gray path uses.
		int cd = signed_vol ? (((65536 - m_wc) % 65536 + 65536) % 65536) : m_wc;
		Aeskulap::Palette::build_lut(&m_lut[0], LUT_N, m_palette,
		                             m_invert, cd, wd);
	}
	m_revision++;
}

void MPRView::set_palette(int id) {
	m_palette = id;
	rebuild_lut();
	rebuild_all();
}

void MPRView::set_invert(bool inv) {
	m_invert = inv;
	rebuild_lut();
	rebuild_all();
}

void MPRView::rebuild_all() {
	for(int i = 0; i < 3; i++) {
		m_pane[i]->queue_draw();
	}
}

