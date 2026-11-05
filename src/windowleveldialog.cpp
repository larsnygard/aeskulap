/*
    Aeskulap - DICOM image viewer and network client
    Copyright (C) 2005  Alexander Pipelka

    This file is part of Aeskulap.

    Aeskulap is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Aeskulap is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Aeskulap; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Alexander Pipelka
    pipelka@teleweb.at
*/

#include "windowleveldialog.h"

WindowLevelDialog::WindowLevelDialog() :
Gtk::Dialog("Add window level preset") {

	Gtk::Table* table = manage(new Gtk::Table(2, 4));
	table->set_col_spacings(8);
	table->set_row_spacings(4);

	table->attach(*(manage(new Gtk::Label("Description", true))), 0, 1, 0, 1,
	              Gtk::FILL, Gtk::SHRINK, 0, 0);
	table->attach(m_description, 1, 2, 0, 1);
	table->attach(*(manage(new Gtk::Label("Modality", true))), 0, 1, 1, 2,
	              Gtk::FILL, Gtk::SHRINK, 0, 0);
	table->attach(m_modality, 1, 2, 1, 2);
	table->attach(*(manage(new Gtk::Label("Center", true))), 0, 1, 2, 3,
	              Gtk::FILL, Gtk::SHRINK, 0, 0);
	table->attach(m_center, 1, 2, 2, 3);
	table->attach(*(manage(new Gtk::Label("Width", true))), 0, 1, 3, 4,
	              Gtk::FILL, Gtk::SHRINK, 0, 0);
	table->attach(m_width, 1, 2, 3, 4);

	for (Gtk::SpinButton* s : { &m_center, &m_width }) {
		s->set_adjustment(Gtk::Adjustment::create(0, -32768, 65535, 1, 10));
		s->set_digits(0);
	}

	add_action_widget(*(manage(new Gtk::Button("Cancel", true))), Gtk::RESPONSE_CANCEL);
	add_action_widget(*(manage(new Gtk::Button("OK", true))), Gtk::RESPONSE_OK);

	get_content_area()->add(*table);
	show_all_children();
}

WindowLevelDialog::~WindowLevelDialog() {
}

void WindowLevelDialog::set(const Aeskulap::WindowLevel& level) {
	m_description.set_text(level.description);
	m_modality.set_text(level.modality);
	m_center.set_value(level.center);
	m_width.set_value(level.width);
}

Aeskulap::WindowLevel WindowLevelDialog::get() {
	Aeskulap::WindowLevel level;
	level.description = m_description.get_text();
	level.modality = m_modality.get_text();
	level.center = m_center.get_value_as_int();
	level.width = m_width.get_value_as_int();
	
	return level;
}
