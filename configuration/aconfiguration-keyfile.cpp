/*
    Aeskulap Configuration — GKeyFile backend (2026)

    Replaces the 2005 gconfmm backend: GSettings/GConf were daemon-based
    session services from that era. GKeyFile persists to
    $XDG_CONFIG_HOME/aeskulap/settings.ini with zero runtime dependencies —
    fast, lightweight, and inspectable.

    Layout:
      [preferences]        local_aet, local_port, characterset
      [server <name>]      aet, port, hostname, group, lossy, relational
      [windowlevel CT/Head] center, width  (one section per preset)
*/

#include "aconfiguration.h"

#include <glibmm.h>
#include <glibmm/keyfile.h>

#include <cstdio>
#include <iostream>
#include <glibmm.h>
#include <sys/types.h>

namespace Aeskulap {

namespace {

const char* PREFS = "preferences";

Glib::ustring config_path() {
  return Glib::build_filename(Glib::get_user_config_dir(), "aeskulap", "settings.ini");
}

class Store {
public:
  Glib::KeyFile kf;

  Store() {
    Glib::ustring path = config_path();
    Glib::ustring dir = Glib::path_get_dirname(path);
    if (!Glib::file_test(dir, Glib::FILE_TEST_EXISTS)) {
      g_mkdir_with_parents(dir.c_str(), 0755);
    }
    try {
      kf.load_from_file(path);
    } catch (const Glib::Error&) {
      // first run: no settings.ini yet — start with an empty keyfile
    }
    ensure_defaults();
  }

  void ensure_defaults() {
    if (!kf.has_group(PREFS)) {
      kf.set_string(PREFS, "local_aet", "AESKULAP");
      kf.set_integer(PREFS, "local_port", 6000);
      kf.set_string(PREFS, "characterset", "ISO_IR 100");
    }
  }

  void save() {
    try {
      kf.save_to_file(config_path());
    } catch (const Glib::Error& e) {
      std::cerr << "aeskulap: cannot write config: " << e.what() << std::endl;
    }
  }

  // server sections are "server <name>"
  static Glib::ustring server_group(const Glib::ustring& name) {
    return "server " + name;
  }
  // windowlevel sections are "windowlevel <MODALITY>/<desc>"
  static Glib::ustring wl_group(const Glib::ustring& modality, const Glib::ustring& desc) {
    return "windowlevel " + modality + "/" + desc;
  }
};

Store& store() {
  static Store s;
  return s;
}

}  // namespace

Configuration::Configuration() {
  Store& s = store();
  if (!s.kf.has_group("windowlevel CT/Head")) {
    add_default_presets_ct();
  }
}

std::string Configuration::get_local_aet() {
  try {
    return store().kf.get_string(PREFS, "local_aet");
  } catch (const Glib::Error&) {
    return "AESKULAP";
  }
}

void Configuration::set_local_aet(const std::string& aet) {
  store().kf.set_string(PREFS, "local_aet", aet);
  store().save();
}

unsigned int Configuration::get_local_port() {
  try {
    int p = store().kf.get_integer(PREFS, "local_port");
    return p > 0 ? (unsigned int)p : 6000;
  } catch (const Glib::Error&) {
    return 6000;
  }
}

void Configuration::set_local_port(unsigned int port) {
  if (port <= 0) port = 6000;
  store().kf.set_integer(PREFS, "local_port", (int)port);
  store().save();
}

std::string Configuration::get_encoding() {
  try {
    return store().kf.get_string(PREFS, "characterset");
  } catch (const Glib::Error&) {
    return "ISO_IR 100";
  }
}

void Configuration::set_encoding(const std::string& encoding) {
  store().kf.set_string(PREFS, "characterset", encoding);
  store().save();
}

Configuration::ServerList* Configuration::get_serverlist() {
  ServerList* list = new ServerList;
  Store& s = store();

  for (const auto& group : s.kf.get_groups()) {
    if (!group.compare(0, 6, "server ")) continue;
    Glib::ustring name = group.substr(7);
    ServerData& d = (*list)[name];
    d.m_name = name;
    try {
      d.m_aet = s.kf.get_string(group, "aet");
      d.m_hostname = s.kf.get_string(group, "hostname");
      d.m_port = (unsigned int)s.kf.get_integer(group, "port");
      d.m_group = s.kf.has_key(group, "group") ? s.kf.get_string(group, "group") : "";
      d.m_lossy = s.kf.has_key(group, "lossy") ? s.kf.get_boolean(group, "lossy") : false;
      d.m_relational =
          s.kf.has_key(group, "relational") ? s.kf.get_boolean(group, "relational") : false;
    } catch (const Glib::Error& e) {
      std::cerr << "aeskulap: bad server section '" << group << "': " << e.what() << std::endl;
    }
  }
  return list;
}

void Configuration::set_serverlist(std::vector<ServerData>& list) {
  Store& s = store();
  // wipe existing server sections, then write the new set
  for (const auto& group : s.kf.get_groups()) {
    if (!group.compare(0, 6, "server ")) s.kf.remove_group(group);
  }
  for (auto& i : list) {
    Glib::ustring group = Store::server_group(i.m_name);
    s.kf.set_string(group, "aet", i.m_aet);
    s.kf.set_string(group, "hostname", i.m_hostname);
    s.kf.set_integer(group, "port", (int)i.m_port);
    s.kf.set_string(group, "group", i.m_group);
    s.kf.set_boolean(group, "lossy", i.m_lossy);
    s.kf.set_boolean(group, "relational", i.m_relational);
  }
  s.save();
}

bool Configuration::get_windowlevel(const Glib::ustring& modality, const Glib::ustring& desc,
                                    WindowLevel& w) {
  Store& s = store();
  Glib::ustring group = Store::wl_group(modality, desc);
  if (!s.kf.has_group(group)) return false;
  try {
    w.modality = modality;
    w.description = desc;
    w.center = s.kf.get_integer(group, "center");
    w.width = s.kf.get_integer(group, "width");
    return true;
  } catch (const Glib::Error&) {
    return false;
  }
}

bool Configuration::get_windowlevel_list(const Glib::ustring& modality, WindowLevelList& list) {
  if (modality.empty()) return false;
  Store& s = store();
  Glib::ustring prefix = "windowlevel " + modality + "/";
  bool found = false;

  list.clear();
  for (const auto& group : s.kf.get_groups()) {
    if (group.compare(0, prefix.size(), prefix)) continue;
    WindowLevel w;
    if (get_windowlevel(modality, group.substr(prefix.size()), w)) {
      list[w.description] = w;
      found = true;
    }
  }
  return found;
}

bool Configuration::set_windowlevel(const WindowLevel& w) {
  Store& s = store();
  Glib::ustring group = Store::wl_group(w.modality, w.description);
  s.kf.set_integer(group, "center", w.center);
  s.kf.set_integer(group, "width", w.width);
  s.save();
  return true;
}

bool Configuration::set_windowlevel_list(const Glib::ustring& modality, WindowLevelList& list) {
  WindowLevelList::iterator i;
  for (i = list.begin(); i != list.end(); i++) {
    i->second.modality = modality;
    set_windowlevel(i->second);
  }
  return true;
}

bool Configuration::unset_windowlevels(const Glib::ustring& modality) {
  Store& s = store();
  Glib::ustring prefix = "windowlevel " + modality + "/";
  bool found = false;
  for (const auto& group : s.kf.get_groups()) {
    if (group.compare(0, prefix.size(), prefix)) continue;
    s.kf.remove_group(group);
    found = true;
  }
  if (found) s.save();
  return found;
}

}  // namespace Aeskulap
