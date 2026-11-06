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
    Update Date:      $Date: 2007/04/25 14:32:02 $
    Source File:      $Source: /cvsroot/aeskulap/aeskulap/src/main.cpp,v $
    CVS/RCS Revision: $Revision: 1.11 $
    Status:           $State: Exp $
*/

#include "config.h"
#include "imagepool.h"
#include "mainwindow.h"

#include "aiconfactory.h"
#include "abusycursor.h"
#include "aconfiguration.h"

#include "binreloc.h"

#include <gtkmm.h>
#include "glade_compat.h"

#include <iostream>
#include "gettext.h"

#include <climits>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

// Relocatable install: prefer the share/ directory next to the executable
// (works for local prefixes AND mounted AppImages), fall back to the
// compile-time datadir.
static std::string find_datadir() {
	char buf[PATH_MAX > 4096 ? PATH_MAX : 4096];
#ifdef _WIN32
	DWORD n = GetModuleFileNameA(NULL, buf, sizeof(buf));
	if(n == 0 || n >= sizeof(buf)) {
		return AESKULAP_DATADIR;
	}
	buf[n] = '\0';
#else
	ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
	if(n <= 0 || (size_t)n >= sizeof(buf) - 1) {
		return AESKULAP_DATADIR;
	}
	buf[n] = '\0';
#endif
	std::string exe(buf);
	size_t pos = exe.find_last_of("\\/");
	std::string dir = (pos == std::string::npos) ? std::string(".") : exe.substr(0, pos);
	std::string share = dir + "/../share";
	if(Glib::file_test(share + "/aeskulap/glade", Glib::FILE_TEST_IS_DIR)) {
		return share;
	}
	return AESKULAP_DATADIR;
}

int main(int argc, char* argv[]) {

	br_init(NULL);

	std::string datadir = find_datadir();

#ifdef _WIN32

	// Relocate the GTK runtime to the bundle (share/ + bin/ next to the exe)
	// without touching the Windows registry: schemas, pixbuf loaders.
	{
		g_setenv("GSETTINGS_SCHEMA_DIR", (datadir + "/glib-2.0/schemas").c_str(), FALSE);

		// gdk-pixbuf loaders: the bundle's loaders.cache stores RELATIVE paths
		// ("loaders/libpixbufloader-*.dll"). Expand them against the bundle
		// location, write an expanded cache to temp and point gdk-pixbuf at it.
		// Fully portable: no registry, bundle can live anywhere.
		{
			char buf[4096];
			DWORD n = GetModuleFileNameA(NULL, buf, sizeof(buf));
			if(n > 0 && n < sizeof(buf)) {
				std::string exe(buf, n);
				size_t pos = exe.find_last_of("\\/");
				std::string bin = (pos == std::string::npos) ? std::string(".") : exe.substr(0, pos);
				std::string root = bin + "/gdk-pixbuf-2.0";
				for(size_t i = 0; i < root.size(); i++) {
					if(root[i] == '\\') {
						root[i] = '/';
					}
				}
				std::string cache_in = root + "/loaders.cache";
				Glib::ustring tmpl;
				if(Glib::file_get_contents(cache_in, tmpl)) {
					std::string out;
					for(Glib::ustring line : tmpl.split("\n")) {
						if(line.size() > 10 && line[0] == '"' && line.compare(1, 8, "loaders/") == 0) {
							line = """ + root + "/" + line.substr(1);
						}
						out += line + "\n";
					}
					std::string cache_out = Glib::build_filename(Glib::get_tmp_dir(), "aeskulap-loaders.cache");
					if(Glib::file_set_contents(cache_out, out)) {
						g_setenv("GDK_PIXBUF_MODULE_FILE", cache_out.c_str(), TRUE);
					}
				}
			}
		}
	}
#endif

	std::cout << "datadir: " << datadir << std::endl;

	std::string localedir = datadir + "/locale";
	std::string gladedir = datadir + "/aeskulap/glade";

	bindtextdomain(GETTEXT_PACKAGE, localedir.c_str());
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
 

	// initialize glib threads
	if(!Glib::thread_supported()) {
		Glib::thread_init();
	}

	// initialize DICOM interface
	ImagePool::init();

	// get configuration values
	Aeskulap::Configuration& config = Aeskulap::Configuration::get_instance();
	
	// set default DICOM encoding (characterset)
	ImagePool::set_encoding(config.get_encoding());

	Gtk::Main kit(argc, argv);

 	// set locale "C" for numeric conversion (strtod)
 
 	if(setlocale(LC_NUMERIC, "C") == NULL) {
		std::cout << "locale C failed" << std::endl;
	}
 
	Glib::RefPtr<Gnome::Glade::Xml> refXml;

	try  {
		refXml = Gnome::Glade::Xml::create(gladedir+"/aeskulap.ui");
	}
	catch(Gnome::Glade::XmlError) {	
		refXml = Gnome::Glade::Xml::create("aeskulap.ui");
	}

	Aeskulap::IconFactory aeskulap_icons;

	MainWindow* mainWindow = NULL;
	refXml->get_widget_derived("window_main", mainWindow);
	
	if(mainWindow == NULL) {
		exit(-1);
	}

	Aeskulap::set_mainwindow(mainWindow);

	mainWindow->maximize();
	mainWindow->show();

	std::list< Glib::ustring > list;
	for(int c=1; c<argc; c++) {
		if(argv[c][0] != '-') {
			list.push_back(argv[c]);
		}
	}
	
	if(list.size() > 0) {
		mainWindow->load_files(list);
		list.clear();
	}

	kit.run(*mainWindow);

	delete mainWindow;

	ImagePool::close();
	
	return 0;
}
