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

    Last Update:      $Author: braindead $
    Update Date:      $Date: 2006/03/20 20:39:37 $
    Source File:      $Source: /cvsroot/aeskulap/aeskulap/widgets/studyview.cpp,v $
    CVS/RCS Revision: $Revision: 1.29 $
    Status:           $State: Exp $
*/

#include "studyview.h"
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkkeysyms-compat.h>
#include "seriesview.h"
#include "imagepool.h"
#include "astockids.h"
#include "adisplay.h"
#include "awindowleveltoolbutton.h"
#include "apostproctoolbutton.h"
#include "mprview.h"
#include "seriesmath.h"
#include <dcmtk/dcmdata/dcuid.h>
#include <cstdlib>
#include "avaluetool.h"

#include "serieslayouttoolbutton.h"
#include "imagelayouttoolbutton.h"
#include "poolinstance.h"
#include "poolseries.h"
#include "poolstudy.h"

#include <iostream>
#include "gettext.h"

StudyView::StudyView(const Glib::RefPtr<ImagePool::Study>& study) :
Aeskulap::Tiler<SeriesView>(2, 1),
m_single_series(false),
m_study(study),
m_selected(NULL),
m_draw_reference_frame_ends(false) {
	m_seriescount = 0;

	Gtk::HBox* hbox = Gtk::manage(new Gtk::HBox);
	hbox->show();

	// SET LAYOUT (BASED ON STUDYDATA)
	
	int count = study->seriescount();
	if(count != 0) {
		if(count == 1) {
			Aeskulap::Tiler<SeriesView>::set_layout(1, 1);
			m_single_series = true;
		}
		else if(count == 2) {
			Aeskulap::Tiler<SeriesView>::set_layout(2, 1);
		}
		else if(count <= 4) {
			Aeskulap::Tiler<SeriesView>::set_layout(2, 2);
		}		
		else if(count <= 6) {
			Aeskulap::Tiler<SeriesView>::set_layout(3, 2);
		}
		else if(count <= 9) {
			Aeskulap::Tiler<SeriesView>::set_layout(3, 3);
		}
		else {
			Aeskulap::Tiler<SeriesView>::set_layout(4, 4);
		}
	}

	// SERIES TABLE
	
	m_table = Gtk::manage(new Gtk::Table(m_tile_y, m_tile_x, true));

	m_table->set_row_spacings(2);
	m_table->set_col_spacings(2);

	// MEASURE TOOLBAR
	
	m_toolbar_measure = Gtk::manage(new Gtk::Toolbar);
	m_toolbar_measure->set_toolbar_style(Gtk::TOOLBAR_ICONS);
	g_object_set(m_toolbar_measure->gobj(), "orientation", GTK_ORIENTATION_VERTICAL, NULL);
	//m_toolbar_measure->show();

	Gtk::ToggleToolButton* m_draw_eraser = Gtk::manage(new Gtk::ToggleToolButton(Aeskulap::Stock::DRAW_ERASER));
	m_draw_eraser->show();
	m_toolbar_measure->append(*m_draw_eraser);
	
	// MAIN TOOLBAR (STUDY)

	m_toolbar = Gtk::manage(new Gtk::Toolbar);

	m_series_layout = Gtk::manage(new SeriesLayoutToolButton());
	m_toolbar->append(*m_series_layout);
	m_series_layout->set_layout(m_tile_x, m_tile_y);
	m_series_layout->set_tooltip_text(gettext("Rearrange the series of the current study"));
	m_series_layout->set_arrow_tooltip_text(gettext("Display the series tiling menu"));
	m_series_layout->signal_change_layout.connect(sigc::mem_fun(*this, &StudyView::on_change_layout));
	if(count != 1) {
		m_series_layout->show();
	}

	m_toggle_full = Gtk::manage(new Gtk::MenuToolButton(Aeskulap::Stock::SERIES_SINGLE));
	m_toggle_full->signal_show_menu().connect(sigc::mem_fun(*this, &StudyView::on_popup_full));
	m_toolbar->append(*m_toggle_full, sigc::mem_fun(*this, &StudyView::on_toggle_full));
	m_toggle_full->set_tooltip_text(gettext("Toggle single series mode"));
	m_toggle_full->set_arrow_tooltip_text(gettext("Display the series selection menu"));
	m_toggle_full->set_menu(m_series_menu);
	if(count != 1) {
		m_toggle_full->show();
	}

	m_image_layout = Gtk::manage(new ImageLayoutToolButton());
	m_toolbar->append(*m_image_layout);
	m_image_layout->set_tooltip_text(gettext("Rearrange the images of the selected series"));
	m_image_layout->set_arrow_tooltip_text(gettext("Display the the image tiling menu"));
	m_image_layout->signal_change_layout.connect(sigc::mem_fun(*this, &StudyView::on_change_layout_series));
	m_image_layout->show();

	m_seperator_reference = Gtk::manage(new Gtk::SeparatorToolItem);
	m_seperator_reference->show();
	m_toolbar->append(*m_seperator_reference);
	
	m_refframe = Gtk::manage(new Gtk::ToggleToolButton(Aeskulap::Stock::REFFRAME));
	m_refframe->set_tooltip_text(gettext("Display references of the selected series"));
	m_toolbar->append(*m_refframe, sigc::mem_fun(*this, &StudyView::on_toggle_refframe));

	m_btn_3dcursor = Gtk::manage(new Gtk::ToggleToolButton(Aeskulap::Stock::THREEDEE_CURSOR));
	m_btn_3dcursor->set_tooltip_text(gettext("Navigate through 3D views"));
	m_toolbar->append(*m_btn_3dcursor, sigc::mem_fun(*this, &StudyView::on_toggle_3dcursor));

	m_btn_valuecursor = Gtk::manage(new Gtk::ToggleToolButton(Aeskulap::Stock::VALUE_CURSOR));
	m_btn_valuecursor->set_tooltip_text(gettext("Display image values under the cursor"));
	m_btn_valuecursor->show();
	m_toolbar->append(*m_btn_valuecursor, sigc::mem_fun(*this, &StudyView::on_toggle_valuecursor));

	Gtk::SeparatorToolItem* seperator = Gtk::manage(new Gtk::SeparatorToolItem);
	m_toolbar->append(*seperator);
	seperator->show();

	m_measure = Gtk::manage(new Gtk::ToggleToolButton(Aeskulap::Stock::MEASURE));
	m_measure->set_tooltip_text(gettext("Measurement tools"));
	m_toolbar->append(*m_measure, sigc::mem_fun(*this, &StudyView::on_toggle_measure));
	//m_measure->show();

	m_windowlevel = Gtk::manage(new Aeskulap::WindowLevelToolButton);
	m_windowlevel->signal_windowlevel_changed.connect(sigc::mem_fun(*this, &StudyView::on_windowlevel_changed));
	m_windowlevel->signal_windowlevel_default.connect(sigc::mem_fun(*this, &StudyView::on_windowlevel_default));
	m_windowlevel->signal_windowlevel_add.connect(sigc::mem_fun(*this, &StudyView::on_windowlevel_add));
	m_windowlevel->signal_windowlevel_invert.connect(sigc::mem_fun(*this, &StudyView::on_windowlevel_invert));
	m_windowlevel->show();
	m_toolbar->append(*m_windowlevel);

	m_postproc = Gtk::manage(new Aeskulap::PostProcToolButton);
	m_postproc->signal_palette_changed.connect(sigc::mem_fun(*this, &StudyView::on_palette_changed));
	m_postproc->signal_kernel_changed.connect(sigc::mem_fun(*this, &StudyView::on_kernel_changed));
	m_postproc->signal_projection_changed.connect(sigc::mem_fun(*this, &StudyView::on_projection_changed));
	m_postproc->show();
	m_toolbar->append(*m_postproc);

	m_content_notebook = Gtk::manage(new Gtk::Notebook);
	m_content_notebook->set_show_tabs(false);
	m_content_notebook->set_show_border(false);
	m_content_notebook->append_page(*m_table);
	m_content_notebook->show();

	m_mpr_button = Gtk::manage(new Gtk::ToggleToolButton("MPR"));
	m_mpr_button->set_tooltip_text("Multiplanar reconstruction (axial / coronal / sagittal)");
	m_mpr_button->signal_toggled().connect(sigc::mem_fun(*this, &StudyView::on_mpr_toggled));
	m_mpr_button->show();
	m_toolbar->append(*m_mpr_button);

	m_mpr_view = NULL;

	m_sub_button = Gtk::manage(new Gtk::ToolButton());
	m_sub_button->set_icon_name("aeskulap-info");
	m_sub_button->set_label("Sub");
	m_sub_button->set_tooltip_text(gettext("Subtract: selected series minus the next compatible series (writes computed SC series)"));
	m_sub_button->signal_clicked().connect(sigc::mem_fun(*this, &StudyView::series_subtract));
	m_sub_button->show();
	m_toolbar->append(*m_sub_button);

	m_mip_button = Gtk::manage(new Gtk::ToolButton());
	m_mip_button->set_icon_name("aeskulap-info");
	m_mip_button->set_label("MIP");
	m_mip_button->set_tooltip_text(gettext("Maximum intensity projection of the selected series (writes computed SC series)"));
	m_mip_button->signal_clicked().connect(sigc::mem_fun(*this, &StudyView::series_mip));
	m_mip_button->show();
	m_toolbar->append(*m_mip_button);

	hbox->pack_end(*m_content_notebook);
	hbox->pack_end(*m_toolbar_measure, false, false);

	pack_end(*hbox);
	pack_end(*m_toolbar, false, false);
	
	m_table->show();
	m_toolbar->show();
	
	set_layout(m_tile_x, m_tile_y);
	
	// VALUE TOOL

	m_valuetool = Gtk::manage(new Aeskulap::ValueTool(*this));

	set_redraw_on_allocate(false);
}

StudyView::~StudyView() {
	ImagePool::remove_study(m_study);
	m_study.clear();
}

void StudyView::on_realize() {
	Gtk::VBox::on_realize();

}

void StudyView::accelerate(Gtk::Window& window) {
	m_image_layout->accelerate(window);
	m_series_layout->accelerate(window);
}

SeriesView* StudyView::create_seriesview(const Glib::RefPtr<ImagePool::Series>& series) {
	SeriesView* r = new SeriesView(series);
	r->signal_draw.connect(sigc::mem_fun(*this, &StudyView::on_draw_instance));
	r->signal_selected.connect(sigc::mem_fun(*this, &StudyView::on_series_selected));
	r->signal_update.connect(sigc::mem_fun(*this, &StudyView::on_series_update));
	r->signal_popup.connect(sigc::mem_fun(*this, &StudyView::on_popup_series));
	r->signal_motion.connect(sigc::bind(sigc::mem_fun(*this, &StudyView::on_signal_motion), r));
	r->signal_image_changed.connect(sigc::mem_fun(*this, &StudyView::on_image_changed));
	r->signal_image_selected.connect(sigc::mem_fun(*this, &StudyView::on_image_selected));
	r->signal_doubleclick.connect(sigc::mem_fun(*this, &StudyView::on_doubleclick));

	return r;
}

void StudyView::add_series(const Glib::RefPtr<ImagePool::Series>& series) {
	std::cout << "new series " << series->seriesinstanceuid() << std::endl;

	int x = 0;
	int y = 0;

	m_seriescount++;
	m_series.push_back(series);

	get_xy_from_pos(m_seriescount, x, y);

	SeriesView* w = create_seriesview(series);

	series->signal_instance_added.connect(sigc::mem_fun(*w, &SeriesView::on_instance_added));
	series->signal_instance_added.connect(sigc::mem_fun(m_series_menu, &Aeskulap::SeriesMenu::set_thumbnail));
	series->signal_instance_added.connect(sigc::mem_fun(*this, &StudyView::on_instance_added));

	if(m_widgets.size() >= m_seriescount) {
		delete m_widgets[m_seriescount-1];
		m_widgets[m_seriescount-1] = w;
	}
	else {
		m_widgets.push_back(w);
	}

	// all views occupied ?
	if(m_seriescount > max_size()) {
		w->hide();
	}
	else {
		m_table->attach(*w, x-1, x, y-1, y);
		w->show();
	}

	m_series_menu.add_series(series, w);

	if(m_selected == NULL) {
		w->select(true);
	}
	
	if(!m_series_layout->is_visible() && m_series.size() > 1) {
		m_toggle_full->show();
		m_series_layout->show();
		m_single_series = false;
		set_layout(2, 1);
		m_series_layout->set_layout(2, 1);
	}
}

void StudyView::on_instance_added(const Glib::RefPtr<ImagePool::Instance>& instance) {
	if(!m_refframe->is_visible()) {
		if(m_study->has_3d_information() > 1) {
			m_seperator_reference->show();
			m_refframe->show();
			m_btn_3dcursor->show();
		}
	}
}

void StudyView::on_series_added(const Glib::RefPtr<ImagePool::Series>& series) {
	add_series(series);	
}

void StudyView::set_layout(int tilex, int tiley) {
	Aeskulap::Tiler<SeriesView>::set_layout(tilex, tiley);

	// remove widgets (series)
	for(unsigned int i = 0; i < m_widgets.size(); i++) {
		SeriesView* v = m_widgets[i];
		if(v != NULL) {
			m_table->remove(*v);
		}
	}

	// remove unneeded seriesviews
	for(unsigned int i = m_seriescount; i < m_widgets.size(); i++) {
		SeriesView* v = m_widgets[i];
		if(m_selected == v) {
			m_selected = NULL;
		}
		delete m_widgets[i];
		m_widgets[i] = NULL;
	}

	// resize table
	m_table->resize(tiley, tilex);

	// rearrange widgets
	int x = 0;
	int y = 0;

	// create seriesviews
	for(unsigned int i=0; i< m_seriescount; i++) {
		
		get_xy_from_pos(i+1, x, y);
		Gtk::Widget* w = m_widgets[i];

		if(i+1 > max_size()) {
			w->hide();
		}
		else {
			w->show();
			m_table->attach(*w, x-1, x, y-1, y);
		}
		((SeriesView*)w)->schedule_repaint(1000);
	}
	
	// fill space with empty seriesviews
	for(unsigned int i=m_seriescount; i< max_size(); i++) {
		SeriesView* w = new SeriesView;
		m_widgets.push_back(w);
		get_xy_from_pos(i+1, x, y);
		m_table->attach(*w, x-1, x, y-1, y);
		w->show();
	}

	Aeskulap::FloatWidget::raise_global();
}

void StudyView::on_series_update(SeriesView* view) {
	if(!m_refframe->get_active()) {
		return;
	}

	queue_draw();
}

void StudyView::on_series_selected(SeriesView* view, bool s) {
	if(!s) {
		return;
	}
	
	if(view == m_selected) {
		return;
	}

	if(m_selected != NULL) {
		m_selected->select(false);
	}
	
	m_selected = view;
	
	int x,y;
	m_selected->get_layout(x, y);
	m_image_layout->set_layout(x, y);
	
	Glib::RefPtr<ImagePool::Series> series = m_selected->get_series();
	m_windowlevel->set_modality(series->modality());
	sync_postproc_controls();
}


void StudyView::on_mpr_toggled() {
	if(m_mpr_button->get_active()) {
		if(!m_selected) {
			m_mpr_button->set_active(false);
			return;
		}
		m_selected->ensure_volume();
		Glib::RefPtr<Volume> vol = m_selected->volume();
		if(!vol || vol->slices() < 2) {
			m_mpr_button->set_active(false);
			return;
		}
		int cx = vol->width() / 2;
		int cy = vol->height() / 2;
		int cz = vol->slices() / 2;
		Aeskulap::Display* d = m_selected->get_selected_display();
		int wc = d ? d->get_windowlevel().center : 40;
		int ww = d ? d->get_windowlevel().width : 400;
		m_mpr_view = new MPRView(vol, cx, cy, cz, wc, ww);
		m_content_notebook->append_page(*m_mpr_view);
		m_mpr_view->show_all();
		m_content_notebook->set_current_page(1);
	}
	else {
		if(m_mpr_view) {
			m_content_notebook->remove_page(1);
			delete m_mpr_view;
			m_mpr_view = NULL;
		}
		m_content_notebook->set_current_page(0);
	}
}


static std::vector<Glib::RefPtr<ImagePool::Instance> > inst_vector(const Glib::RefPtr<ImagePool::Series>& ser) {
	std::vector<Glib::RefPtr<ImagePool::Instance> > v;
	if(!ser) {
		return v;
	}
	for(ImagePool::Series::iterator i = ser->begin(); i != ser->end(); ++i) {
		if(i->second) {
			v.push_back(i->second);
		}
	}
	return v;
}

void StudyView::series_mip() {
	if(!m_selected) {
		return;
	}
	const Glib::RefPtr<ImagePool::Series>& ser = m_selected->get_series();
	std::vector<Glib::RefPtr<ImagePool::Instance> > v = inst_vector(ser);
	if(v.size() < 2) {
		return;
	}
	char outdir[512];
	snprintf(outdir, sizeof(outdir), "/tmp/aeskulap-mip-XXXXXX");
	if(!mkdtemp(outdir)) {
		return;
	}
	char suid[64];
	dcmGenerateUniqueIdentifier(suid, SITE_SERIES_UID_ROOT);
	Aeskulap::SeriesMath::Result r = Aeskulap::SeriesMath::maximum_intensity_project(
		v, outdir, m_study->studyinstanceuid(), suid,
		std::string(ser->description()) + " MIP");
	if(r.frames < 1) {
		return;
	}
	std::list<Glib::ustring> files;
	for(int i = 1; i <= r.frames; i++) {
		char name[600];
		snprintf(name, sizeof(name), "%s/mip%04d.dcm", outdir, i);
		files.push_back(name);
	}
	main_window_load_files(files);
}

void StudyView::series_subtract() {
	if(!m_selected) {
		return;
	}
	// pick the selected series plus the largest other series with the same
	// frame count, width and height
	const Glib::RefPtr<ImagePool::Series>& sel = m_selected->get_series();
	std::vector<Glib::RefPtr<ImagePool::Instance> > a = inst_vector(sel);
	if(a.size() < 1) {
		return;
	}
	int w = a[0]->width(), h = a[0]->height();
	Glib::RefPtr<ImagePool::Series> other;
	for(ImagePool::Study::iterator i = m_study->begin(); i != m_study->end(); ++i) {
		const Glib::RefPtr<ImagePool::Series>& s2 = i->second;
		if(!s2 || s2 == sel || s2->size() != a.size()) {
			continue;
		}
		const Glib::RefPtr<ImagePool::Instance>& f2 = s2->begin()->second;
		if(f2 && f2->width() == w && f2->height() == h) {
			other = s2;
			break;
		}
	}
	if(!other) {
		return;
	}
	std::vector<Glib::RefPtr<ImagePool::Instance> > b = inst_vector(other);
	char outdir[512];
	snprintf(outdir, sizeof(outdir), "/tmp/aeskulap-sub-XXXXXX");
	if(!mkdtemp(outdir)) {
		return;
	}
	char suid[64];
	dcmGenerateUniqueIdentifier(suid, SITE_SERIES_UID_ROOT);
	Aeskulap::SeriesMath::Result r = Aeskulap::SeriesMath::subtract(
		a, b, outdir, m_study->studyinstanceuid(), suid,
		std::string(sel->description()) + " - " + std::string(other->description()));
	if(r.frames < 1) {
		return;
	}
	std::list<Glib::ustring> files;
	for(int i = 1; i <= r.frames; i++) {
		char name[600];
		snprintf(name, sizeof(name), "%s/sub%04d.dcm", outdir, i);
		files.push_back(name);
	}
	main_window_load_files(files);
}

void StudyView::sync_postproc_controls() {
	if(!m_postproc) {
		return;
	}
	if(m_selected) {
		m_postproc->set_palette(m_selected->palette());
		m_postproc->set_kernel(m_selected->kernel());
		m_postproc->set_projection((int)m_selected->projection());
		m_postproc->set_enabled_state(true);
	} else {
		m_postproc->set_enabled_state(false);
	}
}

void StudyView::on_palette_changed(int id) {
	if(m_selected) {
		m_selected->set_palette(id);
	}
}

void StudyView::on_kernel_changed(int id) {
	if(m_selected) {
		m_selected->set_kernel(id);
	}
}

void StudyView::on_projection_changed(int id) {
	if(m_selected) {
		m_selected->set_projection((Volume::Projection)id);
	}
}

void StudyView::draw_reference(Aeskulap::Display* display, const Glib::RefPtr<ImagePool::Instance>& instance) {
	ImagePool::Instance::Point p0 = instance->get_position();
	ImagePool::Instance::Point p1;
	ImagePool::Instance::Point p2;
	ImagePool::Instance::Point p3;
	ImagePool::Instance::Orientation o = instance->get_orientation();

	double sx = instance->width() * instance->spacing_x();
	double sy = instance->height() * instance->spacing_y();

	p1.x = p0.x + o.y.x * sy;
	p1.y = p0.y + o.y.y * sy;
	p1.z = p0.z + o.y.z * sy;

	p2.x = p0.x + o.x.x * sx;
	p2.y = p0.y + o.x.y * sx;
	p2.z = p0.z + o.x.z * sx;

	p3.x = p0.x + o.y.x * sy + o.x.x * sx;
	p3.y = p0.y + o.y.y * sy + o.x.y * sx;
	p3.z = p0.z + o.y.z * sy + o.x.z * sx;

	display->draw_line(p0, p1);
	display->draw_line(p0, p2);
	display->draw_line(p1, p3);
	display->draw_line(p2, p3);

	display->draw_point(p0);
	display->draw_point(p1);
	display->draw_point(p2);
	display->draw_point(p3);
}

void StudyView::on_draw_instance(SeriesView* s, Aeskulap::Display* d, const Cairo::RefPtr<Cairo::Context>& cr) {
	if(s == m_selected) {
		return;
	}
	if(m_selected == NULL || m_selected->m_instance.size() == 0) {
		return;
	}

	// draw reference frames
	Glib::RefPtr<ImagePool::Instance> inst;
	
	if(m_refframe->get_active()) {
		if(m_draw_reference_frame_ends) {
			Aeskulap::set_cairo_color(cr, d->m_colorReference);
			inst = m_selected->m_instance[0];
		
			draw_reference(d, inst);
		
			inst = m_selected->m_instance[m_selected->m_instancecount-1];
			draw_reference(d, inst);
		}
	
		inst = m_selected->m_instance[m_selected->m_selected_image];
	
		Aeskulap::set_cairo_color(cr, d->m_colorSelected);
		draw_reference(d, inst);
	}
	
	// draw 3d cursor
	if(m_btn_3dcursor->get_active()) {
		Aeskulap::set_cairo_color(cr, d->m_colorSelected);
		d->draw_cross(m_3dcursor);
	}
}

void StudyView::on_change_layout(int x, int y) {
	if(m_single_series) {
		return;
	}
	set_layout(x, y);
}

void StudyView::on_change_layout_series(int x, int y) {
	if(m_selected == NULL) {
		return;
	}
	
	m_selected->set_layout(x, y);
}

void StudyView::on_toggle_full() {
	if(m_selected == NULL) {
		return;
	}

	m_single_series = !m_single_series;
	
	if(!m_single_series) {
		m_series_layout->set_sensitive(true);
		m_series_layout->show();
		set_layout(m_tile_x, m_tile_y);
		return;
	}

	view_single_series(m_selected);
	
	Aeskulap::FloatWidget::raise_global();
}

void StudyView::view_single_series(SeriesView* view) {
	m_series_layout->set_sensitive(false);

	m_table->hide();
	view->set_redraw_on_allocate(false);
	
	// remove widgets (series)
	for(unsigned int i = 0; i < m_widgets.size(); i++) {
		SeriesView* v = m_widgets[i];
		if(v != NULL) {
			v->hide();
			m_table->remove(*v);
		}
	}

	m_table->resize(1, 1);
	m_table->attach(*view, 0, 1, 0, 1);
	view->show();
	view->set_redraw_on_allocate(true);

	m_table->show();

	view->select(true);
	m_single_series = true;
	view->schedule_repaint(1000);
	
}

void StudyView::on_popup_series(GdkEventButton* button, SeriesView* view) {

	// set new signal handlers for series popup
	std::vector< Glib::RefPtr<ImagePool::Series> >::iterator i;
	for(i = m_series.begin(); i != m_series.end(); i++) {
		m_series_menu.set_connection(*i, sigc::bind(sigc::mem_fun(*this, &StudyView::on_popup_exchange_series), view));
	}

	m_series_menu.popup(button->button, button->time);
}

void StudyView::on_popup_full() {
	// connections for fullscreen seriesmenu
	std::vector< Glib::RefPtr<ImagePool::Series> >::iterator i;
	for(i = m_series.begin(); i != m_series.end(); i++) {
		m_series_menu.set_connection(*i, sigc::mem_fun(*this, &StudyView::view_single_series));
	}
}

void StudyView::on_popup_exchange_series(SeriesView* target, SeriesView* source) {
	unsigned int index1;
	unsigned int index2;

	if(m_single_series) {
		view_single_series(target);
		return;
	}
	
	if(!find_index(target, index1)) {
		return;
	}
	if(!find_index(source, index2)) {
		return;
	}

	bool v1 = m_widgets[index1]->is_visible();
	bool v2 = m_widgets[index2]->is_visible();
	
	SeriesView* help = m_widgets[index1];
	m_widgets[index1] = m_widgets[index2];
	m_widgets[index2] = help;

	int x1,y1;
	int x2,y2;
	get_xy_from_pos(index1+1, x1, y1);
	get_xy_from_pos(index2+1, x2, y2);
	
	m_table->remove(*target);
	m_table->remove(*source);
	m_table->attach(*target, x2-1, x2, y2-1, y2);
	m_table->attach(*source, x1-1, x1, y1-1, y1);

	if(v1) {
		source->show();
	}
	else {
		source->hide();
	}

	if(v2) {
		target->show();
	}
	else {
		target->hide();
	}

	target->select(true);
	m_series_menu.swap_entries(m_series[index1], m_series[index2]);
}

bool StudyView::on_key_press_event(GdkEventKey* event) {
	if(event->type != GDK_KEY_PRESS || m_selected == NULL) {
		return true;
	}
	
	if(event->keyval == GDK_Up) {
		m_selected->scroll_up();
	}
	else if(event->keyval == GDK_Down) {
		m_selected->scroll_down();
	}
	else if(event->keyval == GDK_Tab) {
		unsigned int index;
		if(find_index(m_selected, index)) {
			index++;
			if(index >= m_seriescount) {
				index = 0;
			}
			if(m_single_series) {
				view_single_series(m_widgets[index]);
			}
			else {
				m_widgets[index]->select(true);
			}
		}
	}
	else if(event->keyval == GDK_Menu) {
		//on_popup_series(GdkEventButton* button, m_selected);
		Glib::RefPtr<Gdk::Window> w = m_selected->get_window();
		int x,y;
		w->get_position(x, y);
		x += m_selected->get_width()/2;
		y += m_selected->get_height()/2;
		
		std::vector< Glib::RefPtr<ImagePool::Series> >::iterator i;
		for(i = m_series.begin(); i != m_series.end(); i++) {
			m_series_menu.set_connection(*i, sigc::bind(sigc::mem_fun(*this, &StudyView::on_popup_exchange_series), m_selected));
		}

		m_series_menu.popup(0, 0);
	}
	
	return true;
}

void StudyView::on_toggle_refframe() {
	if(m_refframe->get_active()) {
		m_btn_valuecursor->set_active(false);
		m_btn_valuecursor->set_sensitive(false);
	}
	else {
		m_btn_valuecursor->set_sensitive(true);
	}
	queue_draw();
}

void StudyView::on_toggle_valuecursor() {
	if(m_btn_valuecursor->get_active()) {
		m_refframe->set_active(false);
		m_refframe->set_sensitive(false);
		m_btn_3dcursor->set_active(false);
		m_btn_3dcursor->set_sensitive(false);
		m_valuetool->show();
	}
	else {
		m_refframe->set_sensitive(true);
		m_btn_3dcursor->set_sensitive(true);
		m_valuetool->hide();
	}
}

void StudyView::on_toggle_3dcursor() {
	if(m_btn_3dcursor->get_active()) {
		m_btn_valuecursor->set_active(false);
		m_btn_valuecursor->set_sensitive(false);
	}
	else {
		m_btn_valuecursor->set_sensitive(true);
	}
	queue_draw();
}

void StudyView::on_toggle_measure() {
	if(m_measure->get_active()) {
		m_refframe->set_active(false);
		m_refframe->set_sensitive(false);
		m_btn_3dcursor->set_active(false);
		m_btn_3dcursor->set_sensitive(false);
		m_toolbar_measure->show();
		enable_mouse_functions(false);
		queue_draw();
	}
	else {
		m_refframe->set_sensitive(true);
		m_btn_3dcursor->set_sensitive(true);
		m_toolbar_measure->hide();
		enable_mouse_functions(true);
	}
}

void StudyView::on_signal_motion(GdkEventMotion* event, Aeskulap::Display* d, SeriesView* s) {
	static int old_x = 0;
	static int old_y = 0;
	
	int x,y;
	d->get_pointer(x, y);

	if(x == old_x && y == old_y) {
		return;
	}

	if(x < 0 || y < 0 || x > d->get_width() || y > d->get_height()) {
		return;
	}

	old_x = x;
	old_y = y;

	// handle value cursor
	
	if(m_btn_valuecursor->get_active()) {
		ImagePool::Instance::Point p;
		if(!d->screen_to_image(x, y, p)) {
			return;
		}
		const Glib::RefPtr<ImagePool::Instance>& i = d->get_image();
		m_valuetool->set_value(i->pixel_value((int)p.x, (int)p.y));
	}

	// handle 3D cursor

	if(m_btn_3dcursor->get_active() && d != NULL & d->get_selected()) {
		ImagePool::Instance::Point p;
		if(!d->screen_to_point(x, y, p)) {
			return;
		}
		const Glib::RefPtr<ImagePool::Instance>& i = d->get_image();
		if(!i) {
			return;
		}
		if(!i->transform_to_world(p, m_3dcursor)) {
			return;
		}
		for(unsigned int i=0; i< m_widgets.size(); i++) {
			SeriesView* s1 = m_widgets[i];
			if(s1 == NULL) {
				continue;
			}

			if(!s1->get_selected()) {
				Glib::RefPtr<ImagePool::Series> series = s1->get_series();
				if(series) {
					Glib::RefPtr<ImagePool::Instance> instance = series->find_nearest_instance(m_3dcursor);
					s1->scroll_to(instance);
					s1->update(true, false, false);
					s1->schedule_repaint(1000);
				}
			}
		}
	}
}

void StudyView::enable_mouse_functions(bool enable) {
	std::cout << "StudyView::enable_mouse_functions()" << std::endl;
	for(unsigned int i = 0; i < m_widgets.size(); i++) {
		if(m_widgets[i] != NULL) {
			m_widgets[i]->enable_mouse_functions(enable);
		}
	}
}

void StudyView::on_image_selected(SeriesView* s, Aeskulap::Display* d) {
	std::cout << "StudyView::on_image_selected()" << std::endl;

	m_windowlevel->set_windowlevel_invert(d->get_inverted());

	if(d->get_windowlevel() == d->get_default_windowlevel()) {
		std::cout << "default windowlevel" << std::endl;
		m_windowlevel->set_windowlevel_default();
		return;
	}
	std::cout << "custom windowlevel" << std::endl;
	m_windowlevel->set_windowlevel(d->get_windowlevel());
}

void StudyView::on_image_changed(SeriesView* s, Aeskulap::Display* d) {
	std::cout << "StudyView::on_image_changed()" << std::endl;
	if(d->get_windowlevel() == d->get_default_windowlevel()) {
		std::cout << "default windowlevel" << std::endl;
		m_windowlevel->set_windowlevel_default();
		return;
	}
	std::cout << "custom windowlevel" << std::endl;
	m_windowlevel->set_windowlevel(d->get_windowlevel());
}

void StudyView::on_windowlevel_changed(const Aeskulap::WindowLevel& l) {
	if(m_selected == NULL) {
		return;
	}
	m_selected->apply_changes(l);
}

void StudyView::on_windowlevel_default() {
	if(m_selected == NULL) {
		return;
	}
	m_selected->apply_default_windowlevel();
}

void StudyView::on_windowlevel_add(Aeskulap::WindowLevelToolButton* btn) {
	if(m_selected == NULL) {
		return;
	}

	Glib::ustring modality = btn->get_modality();
	Aeskulap::Display* d = m_selected->get_selected_display();
	Aeskulap::WindowLevel w = d->get_windowlevel();
	w.modality = modality;
	w.description = gettext("new windowlevel");

	std::cout << "signal_windowlevel_add('" << modality << "', " << w.center << "/" << w.width << ")" << std::endl;

	if(!signal_windowlevel_add(w)) {
		return;
	}
	
	Aeskulap::WindowLevelToolButton::update_all();
	m_windowlevel->set_windowlevel(w, true);
}

void StudyView::on_windowlevel_invert(bool invert) {
	if(m_selected == NULL) {
		return;
	}

	m_selected->set_inverted(invert);
}

void StudyView::on_doubleclick(SeriesView* s, Aeskulap::Display* d) {
	on_toggle_full();
}
