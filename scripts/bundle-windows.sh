#!/bin/bash
# aeskulap 0.3 — Windows portable bundle. Run inside an MSYS2 UCRT64 shell.
# Usage: scripts/bundle-windows.sh <build-dir> <out-dir>
#
# Layout (the app relocates itself from the exe location — no registry):
#   OUT/bin/aeskulap.exe + all MinGW runtime DLLs
#   OUT/bin/gdk-pixbuf-2.0/loaders/ + loaders.cache (absolute paths rewritten
#     at CI time to the portable bin/ location)
#   OUT/share/aeskulap/{glade,images}, OUT/share/glib-2.0/schemas/
set -euo pipefail
BUILD="$(cygpath -u "${1:?build dir}")"
OUT="$(cygpath -u "${2:?out dir}")"
PREFIX="$(cygpath -u "$MINGW_PREFIX")"
QUERY="$PREFIX/bin/gdk-pixbuf-query-loaders.exe"

rm -rf "$OUT"; mkdir -p "$OUT/bin" "$OUT/share"

cp "$BUILD/src/aeskulap.exe" "$OUT/bin/"

# All MinGW shared-library dependencies; skip Windows system DLLs
ldd "$BUILD/src/aeskulap.exe" | grep -oE "$PREFIX/(bin|lib)/[^ ]+\.dll" | while read -r dll; do
  cp -n "$dll" "$OUT/bin/"
done

# Application data (aeskulap.ui + pixmaps) -> OUT/share/aeskulap/{glade,images}
cmake --install "$BUILD" --prefix "$OUT" >/dev/null

# GSettings schemas (GTK settings)
mkdir -p "$OUT/share/glib-2.0/schemas"
cp -n "$PREFIX"/share/glib-2.0/schemas/org.gtk.Settings.*.xml "$OUT/share/glib-2.0/schemas/" 2>/dev/null || true
cp -n "$PREFIX"/share/glib-2.0/schemas/gschemas.compiled "$OUT/share/glib-2.0/schemas/" 2>/dev/null || true

# gdk-pixbuf loaders: copy the DLLs into bin/gdk-pixbuf-2.0/loaders/ and
# generate a loaders.cache pointing at the COPIED location (absolute path
# inside the bundle, matching the bin/gdk-pixbuf-2.0 convention the app uses).
mkdir -p "$OUT/bin/gdk-pixbuf-2.0/loaders"
count=0
for so in "$PREFIX"/lib/gdk-pixbuf-2.0/*/loaders/libpixbufloader-*.dll; do
  [ -e "$so" ] || continue
  cp "$so" "$OUT/bin/gdk-pixbuf-2.0/loaders/"
  count=$((count+1))
done
if [ "$count" -gt 0 ]; then
  # Generate the cache via the query tool (pass the copied DLLs explicitly),
  # then rewrite paths to bundle-relative "loaders/..." tokens (forward
  # slashes) that the app expands at runtime.
  args=()
  for so in "$OUT"/bin/gdk-pixbuf-2.0/loaders/*.dll; do
    args+=("$(cygpath -w "$so")")
  done
  "$QUERY" "${args[@]}" \
    | sed -E 's/"[^"]*[\\/]loaders[\\/]/"loaders\//g' \
    > "$OUT/bin/gdk-pixbuf-2.0/loaders.cache"
  echo "pixbuf loaders: $count"
fi

echo "BUNDLE DONE: $(du -sh "$OUT" | cut -f1)"
