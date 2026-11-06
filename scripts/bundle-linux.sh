#!/bin/bash
# aeskulap 0.3 — portable Linux bundle. Run after a cmake install to $OUT.
# Usage: scripts/bundle-linux.sh <build-dir> <out-dir> [dcmtk-prefix]
#
# Produces a self-contained tree (tar it for distribution):
#   OUT/bin/aeskulap            compiled with rpath $ORIGIN/../lib
#   OUT/bin/aeskulap.sh         launcher (GTK env, then exec)
#   OUT/lib/                    recursive non-glibc shared deps (incl. DCMTK 3.7.1)
#   OUT/share/...               app data + compiled GSettings schemas
set -euo pipefail
BUILD="${1:?build dir}"
OUT="${2:?out dir}"
DCMTK_PREFIX="${3:-/usr/local}"

rm -rf "$OUT"; mkdir -p "$OUT/bin" "$OUT/lib"
DESTDIR= cmake --install "$BUILD" --prefix "$OUT" >/dev/null

# glibc family stays on the system (LTS keeps the ABI forward-compatible)
is_system() {
  case "$1" in
    linux-vdso.so*|ld-linux*|linux-gate*|libc.so*|libm.so*|libpthread.so*|libdl.so*|librt.so*|libnsl*|libutil*|libresolv*|libanl*|libcrypt*|libgcc_s.so.1) return 0 ;;
    *) return 1 ;;
  esac
}

todo="$OUT/bin/aeskulap"
while [ -n "$todo" ]; do
  next=""
  for f in $todo; do
    [ -f "$f" ] || continue
    for dep in $(ldd "$f" 2>/dev/null | awk '{print $3}' | grep '^/'); do
      base="$(basename "$dep")"
      is_system "$base" && continue
      if [ ! -e "$OUT/lib/$base" ]; then
        cp -L "$dep" "$OUT/lib/$base"
        next="$next $OUT/lib/$base"
      fi
    done
  done
  todo="$next"
done

# DCMTK built from source into DCMTK_PREFIX: copy ONLY the DCMTK library set
# (name-filtered, so unrelated libs in a shared prefix never ride along).
for so in "$DCMTK_PREFIX"/lib*/lib*.so*; do
  [ -e "$so" ] || continue
  real="$(readlink -f "$so")"
  base="$(basename "$real")"
  case "$base" in
    libdcm*|libof*|libsc[a-z]*|libscl*|libdim*|libdnl*|libmpr*|libsrg*|libsigsys*|libcmr*|libdcmtk*) ;;
    *) continue ;;
  esac
  is_system "$base" && continue
  [ -e "$OUT/lib/$base" ] || cp -L "$real" "$OUT/lib/$base" 2>/dev/null || true
done

# Strip symbols from everything shipped (big win on the DCMTK set)
command -v strip >/dev/null || true
if command -v strip >/dev/null; then
  strip --strip-unneeded "$OUT/lib/"*.so* 2>/dev/null || true
  strip --strip-unneeded "$OUT/lib/gdk-pixbuf-2.0/loaders/"*.so 2>/dev/null || true
  strip --strip-unneeded "$OUT/bin/aeskulap" 2>/dev/null || true
fi

# gdk-pixbuf loaders -> OUT/lib/gdk-pixbuf-2.0/{loaders,loaders.cache}
loader_root="$(find /usr/lib -maxdepth 3 -type d -name loaders -path '*gdk-pixbuf-2.0*' 2>/dev/null | head -1)"
if [ -n "$loader_root" ]; then
  mkdir -p "$OUT/lib/gdk-pixbuf-2.0/loaders"
  cp "$loader_root"/*.so "$OUT/lib/gdk-pixbuf-2.0/loaders/" 2>/dev/null || true
  cp "$(dirname "$loader_root")/loaders.cache" "$OUT/lib/gdk-pixbuf-2.0/loaders.cache" 2>/dev/null || true
fi

# GSettings schemas compiled into the bundle
mkdir -p "$OUT/share/glib-2.0/schemas"
cp /usr/share/glib-2.0/schemas/*.xml "$OUT/share/glib-2.0/schemas/" 2>/dev/null || true
command -v glib-compile-schemas >/dev/null && glib-compile-schemas "$OUT/share/glib-2.0/schemas" 2>/dev/null || true

# Launcher: lives in bin/, resolves the bundle root one level up, sets the
# runtime env, execs the real binary (finds share/ via
# /proc/self/exe on its own).
cat > "$OUT/bin/aeskulap.sh" <<'LAUNCH'
#!/bin/sh
root="$(cd "$(dirname "$(readlink -f "$0")")/.." && pwd)"
export LD_LIBRARY_PATH="$root/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
[ -f "$root/lib/gdk-pixbuf-2.0/loaders.cache" ] && \
  export GDK_PIXBUF_MODULE_FILE="$root/lib/gdk-pixbuf-2.0/loaders.cache"
[ -f "$root/share/glib-2.0/schemas/gschemas.compiled" ] && \
  export GSETTINGS_SCHEMA_DIR="$root/share/glib-2.0/schemas"
exec "$root/bin/aeskulap" "$@"
LAUNCH
chmod +x "$OUT/bin/aeskulap.sh"

echo "BUNDLE DONE: $(du -sh "$OUT" | cut -f1), libs: $(ls "$OUT/lib" | wc -l)"
