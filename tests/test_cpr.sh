#!/bin/sh
# Headless CPR (spline + reformat) tests. Run from repo root: ./tests/test_cpr.sh
set -e
cd "$(dirname "$0")/.."
[ -d build ] || cmake -B build >/dev/null
make -C build -j"$(nproc)" >/dev/null
LIBS=$(ls /usr/lib/libof*.so /usr/lib/libdcm*.so /usr/lib/libdim*.so /usr/lib/libsrl*.so /usr/lib/libmpr*.so /usr/lib/libsc*.so /usr/lib/libsr*.so 2>/dev/null | sed 's|/usr/lib/lib||; s|\.so$||; s/^/-l/')
g++ -std=c++17 -w -include imagepool/compat.h \
  $(pkg-config --cflags gtkmm-3.0) \
  -Isrc -Iwidgets -Iimagepool -Iconfiguration -Ibuild/include \
  tests/test_cpr.cpp widgets/cpr.cpp \
  build/widgets/libaeskulap-widgets.a build/imagepool/libimagepool.a \
  build/configuration/libaeskulap-config.a \
  $(pkg-config --libs gtkmm-3.0) $LIBS \
  -o /tmp/aesk_test_cpr
/tmp/aesk_test_cpr
