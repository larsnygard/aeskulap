/*
    Aeskulap palette (24-bit) LUT builder
    Copyright (C) 2026  Lars Christian Nygard

    GPL v2+; see volume.h header.
*/

#ifndef AESKULAP_DISPLAYPALETTE_H
#define AESKULAP_DISPLAYPALETTE_H

#include <glib.h>
#include <string>

namespace Aeskulap {
namespace Palette {

enum Id {
	GRAY = 0,
	INVERSE,
	HOT_IRON,
	COLORSTET,
	RAINBOW,
	JET,
	GREEN_AMNIOTIC,
	ID_COUNT
};

int id_from_name(const std::string& name);
const char* name(int id);
const char* label(int id);

/**
 * Fill an n-entry 24-bit LUT (3 bytes per entry, index == pixel value).
 * Ramp [center-w/2, center+w/2] in INDEX space maps over the palette ramp.
 * center/width must already carry the slope/intercept and invert-lut-shape
 * transformation (as the legacy 8-bit ramp computed it). Invert composes
 * into the output. GRAY palette reproduces the legacy ramp exactly.
 */
void build_lut(guint8* lut, int n, int palette_id, bool invert, int center, int width);

} // namespace Palette
} // namespace Aeskulap

#endif // AESKULAP_DISPLAYPALETTE_H
