#!/usr/bin/env python3
"""Convert the 2005 glade-2.0 UI file to GTK3 GtkBuilder format (2026)."""
import xml.etree.ElementTree as ET
import sys

STOCK_MENU = {
    "gtk-open": "_Open", "gtk-quit": "_Quit", "gtk-save": "_Save",
    "gtk-preferences": "_Preferences", "gtk-about": "_About",
    "gtk-add": "_Add", "gtk-remove": "_Remove", "gtk-cancel": "Cancel",
    "gtk-ok": "OK", "gtk-close": "_Close", "gtk-delete": "_Delete",
}
STOCK_ICON = {
    "gtk-find": "edit-find", "gtk-delete": "edit-delete", "gtk-open": "document-open",
    "gtk-network": "network-workgroup", "gtk-cdrom": "media-optical",
    "gtk-quit": "application-exit", "gtk-about": "help-about",
    "gtk-preferences": "preferences-system", "gtk-add": "list-add",
    "gtk-remove": "list-remove", "gtk-cancel": "process-stop", "gtk-ok": "emblem-ok",
    "gtk-close": "window-close", "gtk-clear": "edit-clear",
}
ENUM_SHORT = {
    "GTK_WIN_POS_CENTER": "center", "GTK_WIN_POS_CENTER_ALWAYS": "center-always",
    "GTK_WINDOW_POS_CENTER": "center", "GTK_WINDOW_POS_CENTER_ALWAYS": "center-always",
    "GDK_WINDOW_TYPE_HINT_NORMAL": "normal", "GDK_WINDOW_TYPE_HINT_DIALOG": "dialog",
    "GTK_SHADOW_NONE": "none", "GTK_SHADOW_IN": "in",
    "GTK_POS_BOTTOM": "bottom", "GTK_JUSTIFY_CENTER": "center",
    "GDK_GRAVITY_CENTER": "center", "GTK_BUTTONBOX_END": "end",
    "GTK_POLICY_AUTOMATIC": "automatic", "GTK_POLICY_NEVER": "never",
    "GTK_POLICY_ALWAYS": "always",
}
PACK_ENUM = {"GTK_PACK_END": "end", "GTK_FILL": "fill"}

CONTAINER_MAP = {
    "GtkVBox": ("GtkBox", {"orientation": "vertical"}),
    "GtkHBox": ("GtkBox", {"orientation": "horizontal"}),
    "GtkVButtonBox": ("GtkButtonBox", {"orientation": "vertical"}),
    "GtkHButtonBox": ("GtkButtonBox", {"orientation": "horizontal"}),
    "GtkComboBoxEntry": ("GtkComboBox", {"has-entry": "True"}),
}

PROP_RENAME = {
    "width_request": "width-request", "height_request": "height-request",
    "hscrollbar_policy": "hscrollbar-policy", "vscrollbar_policy": "vscrollbar-policy",
    "tab_pos": "tab-pos", "shadow_type": "shadow-type", "window_position": "window-position",
    "invisible_char": "invisible-char", "max_length": "max-length",
    "width_chars": "width-chars", "label_xalign": "label-xalign",
    "column_spacing": "column-spacing", "row_spacing": "row-spacing",
    "layout_style": "layout-style", "justify": "justify",
    "single_line_mode": "single-line-mode", "draw_indicator": "draw-indicator",
    "headers_visible": "headers-visible", "can_focus": "can-focus",
    "can_default": "can-default", "use_underline": "use-underline",
    "use_stock": "use-stock", "use_markup": "use-markup",
    "xalign": "xalign", "xpad": "xpad", "ypad": "ypad",
}

def props(widget):
    return {p.get("name"): p for p in widget.findall("property")}

def convert_widget(w, out, parent_class):
    cls = w.get("class")
    wid = w.get("id")
    p = props(w)

    stock_label = None
    stock_icon = None
    if "stock" in p:  # GtkImage
        stock_icon = STOCK_ICON.get(p["stock"].text.strip(), "image-missing")
    if cls == "GtkImageMenuItem" and "label" in p and p["label"].text.strip().startswith("gtk-"):
        stock_label = STOCK_MENU.get(p["label"].text.strip())
    if cls == "GtkButton" and "label" in p and "use_stock" in p and p["label"].text.strip().startswith("gtk-"):
        stock_label = STOCK_MENU.get(p["label"].text.strip())

    if cls in CONTAINER_MAP:
        cls2, extra = CONTAINER_MAP[cls]
    else:
        cls2, extra = cls, {}

    o = ET.Element("object", {"class": cls2, "id": wid})
    for k, v in extra.items():
        ET.SubElement(o, "property", {"name": k}).text = v

    drop = {"use_stock", "stock", "items", "accelerator-key", "response_id", "tooltip"}
    response_id = p["response_id"].text.strip() if "response_id" in p else None
    tooltip = (p["tooltip"].text or "").strip() if "tooltip" in p else None
    if response_id is not None:
        pending_responses.append((wid, response_id))
    if stock_label is not None:
        drop.add("label")
    for name, el in p.items():
        if name in drop:
            continue
        text = (el.text or "").strip()
        out_name = PROP_RENAME.get(name, name)
        if text in ENUM_SHORT:
            text = ENUM_SHORT[text]
        elif name in ("shadow_type",) and text:
            text = ENUM_SHORT.get(text, text)
        if name == "invisible_char":
            text = "*"
        if name == "label" and stock_label is None and cls2 == "GtkButton":
            continue  # plain button label set below
        if name == "label" and cls2 == "GtkLabel":
            ET.SubElement(o, "property", {"name": "label"}).text = text
            continue
        ne = ET.SubElement(o, "property", {"name": out_name})
        ne.text = text
    if stock_label is not None:
        ET.SubElement(o, "property", {"name": "label"}).text = stock_label
        ET.SubElement(o, "property", {"name": "use-underline"}).text = "True"
    if stock_icon is not None:
        ET.SubElement(o, "property", {"name": "icon-name"}).text = stock_icon
    if response_id is not None:
        pass  # emitted via <action-widgets> on the dialog
    if tooltip is not None:
        ET.SubElement(o, "property", {"name": "tooltip-text"}).text = tooltip

    # spinbutton adjustment -> referenced GtkAdjustment object (emitted by caller)
    if cls == "GtkSpinButton" and "adjustment" in p:
        vals = p["adjustment"].text.split()
        value, lower, upper, step, page_incr, page_size = vals[:6]
        adj_id = (wid or "adj") + "-adjustment"
        ADJUSTMENTS.setdefault(adj_id, make_adjustment(adj_id, value, lower, upper, step, page_incr, page_size))
        for e in list(o):
            if e.tag == "property" and e.get("name") == "adjustment":
                o.remove(e)
        ET.SubElement(o, "property", {"name": "adjustment", "translatable": "no"}).text = adj_id

    # children
    for ch in w.findall("child"):
        pk = ch.find("packing")
        inner = None
        itc = ch.get("internal-child")
        if itc == "entry":  # combobox internal entry; builder creates it
            continue
        child_widget = ch.find("widget")
        if child_widget is None:
            continue
        child_type = None
        if pk is not None:
            pprops = {q.get("name"): (q.text or "").strip() for q in pk.findall("property")}
            ptype = pprops.get("type")
            if ptype == "tab":
                child_type = "tab"
            elif ptype == "label_item":
                child_type = "label_item"
            elif ptype == "fill":
                child_type = "fill"
        # internal dialog children
        if itc in ("vbox", "action_area"):
            wrap = ET.Element("child", {"internal-child": itc})
            co = convert_widget(child_widget, out, cls2)
            co.attrib.pop("id", None)
            wrap.append(co)
            o.append(wrap)
            sub = child_widget.get("id", "")
            matched = [(bwid, resp) for (bwid, resp) in pending_responses if bwid.startswith(sub)]
            if matched:
                awx = ET.SubElement(o, "action-widgets")
                for (bwid, resp) in matched:
                    aw = ET.SubElement(awx, "action-widget", {"response": resp})
                    aw.text = bwid
                    pending_responses.remove((bwid, resp))
            continue

        # GtkImageMenuItem: GTK3 deprecates menu icons; drop the image child
        if cls == "GtkImageMenuItem" and child_widget.get("class") == "GtkImage":
            continue

        # GtkMenuItem with a submenu, GtkFrame/Expander with a custom label
        if child_widget.get("class") == "GtkMenu" and cls in ("GtkMenuItem", "GtkImageMenuItem"):
            co = convert_widget(child_widget, out, cls2)
            cw = ET.Element("child", {"type": "submenu"})
            cw.append(co)
            o.append(cw)
            continue
        if child_type == "label_item" and cls == "GtkExpander":
            co = convert_widget(child_widget, out, cls2)
            top_level_objects.append(co)
            ET.SubElement(o, "property", {"name": "label-widget", "translatable": "no"}).text = co.get("id")
            continue
        if child_type == "label_item" and cls == "GtkFrame":
            co = convert_widget(child_widget, out, cls2)
            cw = ET.Element("child", {"type": "label"})
            cw.append(co)
            o.append(cw)
            continue
        if child_type == "label_item":
            child_type = None  # glade-2 attached labels loosely inside boxes

        co = convert_widget(child_widget, out, cls2)
        cw = ET.Element("child")
        if child_type:
            cw.set("type", child_type)
        cw.append(co)
        if pk is not None:
            pack = ET.Element("packing")
            for q in pk.findall("property"):
                name = q.get("name")
                text = (q.text or "").strip()
                if name == "type" or name == "tab_expand" and False:
                    continue
                if name in ("x_options", "y_options"):
                    if text in PACK_ENUM:
                        text = PACK_ENUM[text]
                    ET.SubElement(pack, "property", {"name": name}).text = text
                elif name == "pack_type":
                    ET.SubElement(pack, "property", {"name": "pack-type"}).text = PACK_ENUM.get(text, text)
                elif name == "tab_expand":
                    ET.SubElement(pack, "property", {"name": "tab-expand"}).text = "True" if text == "True" else "False"
                elif name == "tab_fill":
                    ET.SubElement(pack, "property", {"name": "tab-fill"}).text = "True" if text == "True" else "False"
                else:
                    ET.SubElement(pack, "property", {"name": name}).text = text
            cw.append(pack)
        o.append(cw)

    # dialog action widgets
    for ch in w.findall("child"):
        for dch in ch.findall("child"):
            pass
    return o

def make_adjustment(adj_id, value, lower, upper, step, page_incr, page_size):
    a = ET.Element("object", {"class": "GtkAdjustment", "id": adj_id})
    for n, v in [("value", value), ("lower", lower), ("upper", upper),
                 ("step-increment", step), ("page-increment", page_incr), ("page-size", page_size)]:
        ET.SubElement(a, "property", {"name": n}).text = v
    return a

ADJUSTMENTS = {}
top_level_objects = []
pending_responses = []

def main(src, dst):
    tree = ET.parse(src)
    root = tree.getroot()
    iface = ET.Element("interface")
    ET.SubElement(iface, "requires", {"lib": "gtk+", "version": "3.0"})
    objs = ET.Element("object", {"class": "GtkSizeGroup"})  # placeholder not used
    top_objs = []
    for w in root.findall("widget"):
        top_objs.append(convert_widget(w, None, None))
    for a in ADJUSTMENTS.values():
        top_objs.append(a)
    for t in top_level_objects:
        top_objs.append(t)
    for t in top_objs:
        iface.append(t)
    tree_out = ET.ElementTree(iface)
    ET.indent(tree_out, space="  ")
    tree_out.write(dst, encoding="unicode", xml_declaration=False)
    print("wrote", dst)

if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
