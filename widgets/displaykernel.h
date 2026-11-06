/*
    Aeskulap 3x3 image kernel
    Copyright (C) 2026  Lars Christian Nygard

    GPL v2+; see volume.h header.
*/

#ifndef AESKULAP_DISPLAYKERNEL_H
#define AESKULAP_DISPLAYKERNEL_H

#include <glib.h>
#include <string>

namespace Aeskulap {
namespace Kernel {

enum Id {
	IDENTITY = 0,
	SHARPEN,
	EDGE_ENHANCE,
	BLUR,
	SMOOTH,       // noise reduction: 3x3 binomial
	MEDIAN,       // 3x3 median (edge-preserving denoise)
	ID_COUNT
};

int id_from_name(const std::string& name);
const char* name(int id);
const char* label(int id);

/**
 * Apply kernel id to a single slice (w x h native-order guint16) into dst.
 * src==dst allowed. For signed (two's-complement CT) pass is_signed=true:
 * arithmetic happens in the int16 domain so HU-space kernels don't ring
 * across the unsigned wrap boundary. Median is nonlinear (denoise).
 */
void apply(int id, const guint16* src, guint16* dst, int w, int h, bool is_signed);

} // namespace Kernel
} // namespace Aeskulap

#endif // AESKULAP_DISPLAYKERNEL_H
