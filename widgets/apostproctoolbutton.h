/*
    Aeskulap post-processing toolbar widget
    Copyright (C) 2026  Lars Christian Nygard

    GPL v2+ (see asimpledisplay.h header).
*/

#ifndef AESKULAP_POSTPROCTOOLBUTTON_H
#define AESKULAP_POSTPROCTOOLBUTTON_H

#include <gtkmm.h>

namespace Aeskulap {

/**
 * Toolbar item with three dropdowns: palette (grayscale/colour),
 * image kernel (sharpen/blur/...) and slab projection (MIP/MinIP/AvgIP).
 * Emits signals; StudyView routes them to the selected SeriesView.
 */
class PostProcToolButton : public Gtk::ToolItem {
public:

	PostProcToolButton();

	void set_enabled_state(bool has_volume);

	void set_palette(int id);
	void set_kernel(int id);
	void set_projection(int id);   // Volume::Projection

	sigc::signal<void, int> signal_palette_changed;
	sigc::signal<void, int> signal_kernel_changed;
	sigc::signal<void, int> signal_projection_changed;

private:

	void on_palette_changed();
	void on_kernel_changed();
	void on_projection_changed();

	Gtk::ComboBoxText* m_palette;
	Gtk::ComboBoxText* m_kernel;
	Gtk::ComboBoxText* m_projection;

	bool m_updating;
};

} // namespace Aeskulap

#endif // AESKULAP_POSTPROCTOOLBUTTON_H
