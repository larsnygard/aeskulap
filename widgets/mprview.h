/*
    Aeskulap MPR (multiplanar reconstruction) view
    Copyright (C) 2026  Lars Christian Nygard

    GPL v2+ (see asimpledisplay.h header).
*/

#ifndef AESKULAP_MPRVIEW_H
#define AESKULAP_MPRVIEW_H

#include <gtkmm.h>
#include <glibmm/refptr.h>
#include <vector>

#include "volume.h"

/**
 * Three panes (axial / coronal / sagittal) sharing one Volume.
 * Each pane renders the plane through the shared crosshair with the
 * shared window/level + palette. Scroll over a pane moves the
 * crosshair along that pane's plane normal; left-drag adjusts
 * window/level, right-drag/click moves the crosshair.
 */
class MPRView : public Gtk::Grid {
public:

	MPRView(const Glib::RefPtr<Volume>& volume,
	        int center_x, int center_y, int center_z,
	        int window_center, int window_width);

	void set_window(int center, int width);
	void set_palette(int id);
	void set_invert(bool inv);

private:

	class Pane;

	void rebuild_lut();
	void rebuild_all();

	Glib::RefPtr<Volume> m_volume;

	int m_center[3];   // x, y, z indices
	int m_wc, m_ww;
	bool m_invert;
	int m_palette;
	std::vector<guint8> m_lut;   // 65536*3 shared LUT
	guint64 m_revision;

	Pane* m_pane[3];

};

#endif // AESKULAP_MPRVIEW_H
