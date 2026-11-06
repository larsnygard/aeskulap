/*
    Aeskulap post-processing toolbar widget
    Copyright (C) 2026  Lars Christian Nygard

    GPL v2+ (see asimpledisplay.h header).
*/

#include "apostproctoolbutton.h"
#include "displaypalette.h"
#include "displaykernel.h"
#include "volume.h"

#include "gettext.h"

namespace Aeskulap {

PostProcToolButton::PostProcToolButton() :
m_updating(false) {
	Gtk::HBox* hbox = Gtk::manage(new Gtk::HBox);

	Gtk::VBox* vbox;

	// ---- palette ----
	vbox = Gtk::manage(new Gtk::VBox);
	m_palette = Gtk::manage(new Gtk::ComboBoxText);
	m_palette->set_size_request(90, 32);
	m_palette->signal_changed().connect(sigc::mem_fun(*this, &PostProcToolButton::on_palette_changed));
	vbox->pack_start(*m_palette, true, false);
	vbox->show();
	hbox->pack_start(*vbox, Gtk::PACK_SHRINK);

	// ---- kernel ----
	vbox = Gtk::manage(new Gtk::VBox);
	m_kernel = Gtk::manage(new Gtk::ComboBoxText);
	m_kernel->set_size_request(110, 32);
	m_kernel->signal_changed().connect(sigc::mem_fun(*this, &PostProcToolButton::on_kernel_changed));
	vbox->pack_start(*m_kernel, true, false);
	vbox->show();
	hbox->pack_start(*vbox, Gtk::PACK_SHRINK);

	// ---- projection ----
	vbox = Gtk::manage(new Gtk::VBox);
	m_projection = Gtk::manage(new Gtk::ComboBoxText);
	m_projection->set_size_request(80, 32);
	m_projection->signal_changed().connect(sigc::mem_fun(*this, &PostProcToolButton::on_projection_changed));
	vbox->pack_start(*m_projection, true, false);
	vbox->show();
	hbox->pack_start(*vbox, Gtk::PACK_SHRINK);

	// palette entries
	for(int i = 0; i < Palette::ID_COUNT; i++) {
		char id[8];
		g_snprintf(id, sizeof(id), "%d", i);
		m_palette->append(id, Palette::label(i));
	}
	// kernel entries
	for(int i = 0; i < Kernel::ID_COUNT; i++) {
		char id[8];
		g_snprintf(id, sizeof(id), "%d", i);
		m_kernel->append(id, Kernel::label(i));
	}
	// projection entries
	const char* proj_names[4] = {
		gettext("Slice"), gettext("MIP"), gettext("MinIP"), gettext("Avg")
	};
	for(int i = 0; i < 4; i++) {
		char id[8];
		g_snprintf(id, sizeof(id), "%d", i);
		m_projection->append(id, proj_names[i]);
	}

	m_palette->set_active_id("0");
	m_kernel->set_active_id("0");
	m_projection->set_active_id("0");

	hbox->show();
	add(*hbox);
	show();

	set_enabled_state(false);
}

void PostProcToolButton::set_enabled_state(bool has_volume) {
	m_kernel->set_sensitive(true);      // kernels work per-slice too
	m_palette->set_sensitive(true);     // palettes work on any greyscale image
	m_projection->set_sensitive(has_volume);
}

void PostProcToolButton::set_palette(int id) {
	char buf[8];
	g_snprintf(buf, sizeof(buf), "%d", id);
	m_updating = true;
	m_palette->set_active_id(buf);
	m_updating = false;
}

void PostProcToolButton::set_kernel(int id) {
	char buf[8];
	g_snprintf(buf, sizeof(buf), "%d", id);
	m_updating = true;
	m_kernel->set_active_id(buf);
	m_updating = false;
}

void PostProcToolButton::set_projection(int id) {
	char buf[8];
	g_snprintf(buf, sizeof(buf), "%d", id);
	m_updating = true;
	m_projection->set_active_id(buf);
	m_updating = false;
}

void PostProcToolButton::on_palette_changed() {
	if(m_updating) {
		return;
	}
	Glib::ustring id = m_palette->get_active_id();
	if(!id.empty()) {
		signal_palette_changed.emit(atoi(id.c_str()));
	}
}

void PostProcToolButton::on_kernel_changed() {
	if(m_updating) {
		return;
	}
	Glib::ustring id = m_kernel->get_active_id();
	if(!id.empty()) {
		signal_kernel_changed.emit(atoi(id.c_str()));
	}
}

void PostProcToolButton::on_projection_changed() {
	if(m_updating) {
		return;
	}
	Glib::ustring id = m_projection->get_active_id();
	if(!id.empty()) {
		signal_projection_changed.emit(atoi(id.c_str()));
	}
}

} // namespace Aeskulap
