/*
    Aeskulap - DICOM image viewer and network client

    glade_compat.h - minimal replacement for libglademm (Gnome::Glade::Xml)
    implemented directly on GtkBuilder, so the 2005-era call sites
    (get_widget / get_widget_derived) keep working unchanged.

    GPL v2+ as the rest of Aeskulap.
*/

#ifndef AESKULAP_GLADE_COMPAT_H
#define AESKULAP_GLADE_COMPAT_H

#include <gtkmm.h>
#include <iostream>

namespace Gnome {
namespace Glade {

class Xml;

class XmlError {};

class Xml {
public:
	static Glib::RefPtr<Xml> create(const Glib::ustring& file) {
		Xml* self = new Xml();
		GtkBuilder* b = gtk_builder_new();
		if(gtk_builder_add_from_file(b, file.c_str(), NULL)) {
			self->m_builder = b;
		}
		else {
			g_object_unref(b);
			delete self;
			throw XmlError();
		}
		// RefPtr(T*) adopts without ref'ing: seed the count so every owner
		// (this return value + copies handed to derived ctors) is counted
		self->m_ref = 1;
		return Glib::RefPtr<Xml>(self);
	}

	void reference() const { ++m_ref; }
	void unreference() const {
		if(!--m_ref) {
			delete const_cast<Xml*>(this);
		}
	}

	// plain widget lookup
	template<class T>
	void get_widget(const Glib::ustring& name, T*& outObj) {
		outObj = NULL;
		if(!GTK_IS_BUILDER(m_builder)) {
			std::cerr << "glade_compat: builder dead when looking up '" << name << "' (m_ref=" << m_ref << ")" << std::endl;
			return;
		}
		GtkWidget* w = GTK_WIDGET(gtk_builder_get_object(m_builder, name.c_str()));
		if(w) {
			outObj = static_cast<T*>(Glib::wrap(w));
		}
		if(!outObj) {
			std::cerr << "glade_compat: widget '" << name << "' not found/cast failed" << std::endl;
		}
	}

	// derived widget lookup: wraps the builder-instantiated base widget in
	// the C++ subclass, exactly as libglademm did
	template<class T>
	void get_widget_derived(const Glib::ustring& name, T*& outObj) {
		outObj = NULL;
		GtkWidget* w = GTK_WIDGET(gtk_builder_get_object(m_builder, name.c_str()));
		if(!w) {
			return;
		}
		outObj = Gtk::manage(new T(
			reinterpret_cast<typename T::BaseObjectType*>(w), Glib::RefPtr<Xml>(this)));
	}

	~Xml() {
		if(m_builder) {
			g_object_unref(m_builder);
		}
	}

private:
	Xml() : m_builder(NULL), m_ref(0) {}
	GtkBuilder* m_builder;
	mutable unsigned int m_ref;
};

} // namespace Glade
} // namespace Gnome

#endif // AESKULAP_GLADE_COMPAT_H
