# Aeskulap 0.2.9 → 0.3 Feature Program

**Read-me-first handoff plan.** If your context is fresh, start here.

## Status (updated 2026-11-06, HEAD = stage5 commit pushed to origin/master)
- **Plan Stage 1 (quick wins) — DONE** (commits in `git log`, "0.3 stage2/…" labels): CT presets
  Bone W2000/C300, Soft tissue W350/C50, Brain W80/C40, Liver W150/C60 added to
  `add_default_presets_ct()` (configuration/aconfiguration.cpp); About dialog full programmatic
  info (GPL-2.0, authors Pipelka + Nygård, website larsnygard/aeskulap) in src/aboutdialog.cpp
  (needs `#include "gettext.h"` — was a build break, fixed).
- **Plan Stage 2 (palettes + kernels) — DONE, with tests**:
  - `widgets/volume.{h,cpp}`: shared per-series Volume core — slab sum/max/min over N slices,
    projections PASS/MIP/MINIP/AVG, 3×3 kernels (identity/sharpen/edge/blur/smooth) in
    **signed HU domain** (air↔tissue edges no longer spike mid-gray; from_domain stores
    two's-complement).
  - `widgets/displaypalette.{h,cpp}`: 24-bit LUT palettes (Gray, Inverse, Hot Iron, Rainbow, Jet,
    Red) + invert-after-LUT; wired in `asimpledisplay.cpp` render path via
    `DisplayParameters::palette`.
  - `widgets/apostproctoolbutton.{h,cpp}`: toolbar item with 3 ComboBoxTexts
    (palette / kernel / projection). **Wired into StudyView** (ctor append after m_windowlevel;
    signals → on_palette_changed/on_kernel_changed/on_projection_changed → m_selected;
    sync_postproc_controls() on series select). GLOB in widgets/CMakeLists.txt picks up new files
    after `cmake .` re-run.
  - SeriesView API: `set_kernel/kernel()`, `set_palette/palette()`, `set_invert()`,
    `set_projection/projection()`, `ensure_volume()` lazy volume attach; shift+scroll = slab.
  - `tests/test_render.cpp` + `./tests/test_render.sh` — 25 headless checks, **ALL PASS**.
    (Test links against build/*.a — `make -C build` first; script does this.)
  - GUI smoke on DISPLAY=:0 with /tmp/testvol/: app runs, loads study/series, **zero GTK-CRITICALs**.
- **Plan Stage 3 (MPR) — DONE** (commit fd85228f). Implemented:
  - `Volume`: Axis enum, `set_center`, `plane(w,h)` cached resample (AXIS_Z raw; AXIS_Y coronal
    w=width h=slices; AXIS_X sagittal w=height h=slices), `is_signed()`. Headless-tested in
    tests/test_render.cpp (plane dims, content per axis, center follow) — ALL PASS.
  - `widgets/mprview.{h,cpp}`: 3 Panes (Gtk::DrawingArea, nested class MPRView::Pane) render
    plane via shared 65536*3 LUT (gray ramp signed-flip like legacy invert_lut_shape; palette
    via Palette::build_lut); scroll=crosshair axis, left-drag=W/L, right-drag/press=crosshair.
    Green crosshair + label overlays in cairo.
  - StudyView: `m_mpr_button` ToggleToolButton 'MPR' in toolbar; content area is now
    `m_content_notebook` (page 0 = tiler table, page 1 = MPRView, tabs hidden); on_mpr_toggled
    builds MPRView from m_selected->volume() (ensure_volume; needs slices>=2; W/L from selected
    display); delete on un-toggle. `SeriesView::volume()` accessor added.
  - GUI smoke: 48-file dir loads (prescan progresses per file, 49 lines, study+series created,
    app alive; 1 known stock-icon CRITICAL, pre-existing). NOTE: launching the app via a
    backgrounded bash wrapper confuses pgrep -f (matches wrapper); use `pgrep -x aeskulap`.
  - GUI MPR INTERACTION NOT YET CLICK-VERIFIED (no xdotool; ptrace_scope=1 blocks gdb/strace).
    Click-testing MPR toggle is a stage-5 loose end; code compiles, plane math unit-tested.

Old design notes:

**Plan Stage 3 (MPR) — design notes:**
  1. Extend `Volume`: `enum Axis {AXIS_Z, AXIS_Y, AXIS_X}` (plane normal), `set_axis`,
     `set_center_x/y/z` (indices), cached `const guint16* plane(int &w, int &h)` — nearest-neighbor
     resample: AXIS_Z = raw slice (w=vol.w, h=vol.h, fixed z=center_z); AXIS_Y coronal
     (w=vol.w, h=nslices, fixed y=center_y); AXIS_X sagittal (w=vol.h, h=nslices, fixed
     x=center_x). Apply window/level + palette through the SAME DisplayParameters path.
  2. `DisplayParameters`: add `Volume::Axis axis` (default AXIS_Z); `asimpledisplay.cpp`
     effective_pixels() → when axis != Z, fetch from volume->plane() instead of instance pixels.
  3. `widgets/mprview.{h,cpp}`: class MPRView : Gtk::Grid — 3 SimpleDisplay panes
     (labels Axial/Coronal/Sagittal), 3 DisplayParameters sharing window/level + palette/kernel,
     each pointing at the same Volume with its axis; per-pane scroll event → center along that
     axis → refresh all 3; crosshair lines optional (draw over Display via overlay).
  4. Entry: `aseriesmenu.cpp` add "MPR (3 planes)" → StudyView appends MPRView tab (title
     "MPR <series desc>"). MPRView lives in widgets/CMakeLists GLOB, no CMake edit needed
     after `cmake .`.
- **Plan Stage 4 (series arithmetic) — DONE** (this commit). Architecture change from the
  original note: instead of in-memory Instances (ctor is friend-protected to Loader),
  `src/seriesmath.{h,cpp}` computes in int32 modality-LUT domain and **writes real Secondary
  Capture DICOM files** (signed int16, LittleEndianExplicit, EWM_createNewMeta) into a
  mkdtemp dir; StudyView::series_subtract()/series_mip() then load them through the existing
  FileLoader via StudyView::signal_load_files (MainWindow wires it to m_fileloader.load).
  UI: "Sub" and "MIP" ToolButtons in StudyView toolbar (Sub pairs selected series with the
  first other series matching frame count + width + height).
  Headless tests: ./tests/test_seriesmath.sh — 11 checks (load_pixels LUT domain, MIP=max,
  Sub=A-B, signed negative round-trip, grid-mismatch rejected, file dims) ALL PASS.
  Results are persistent (files on disk) and exportable. GUI click-test of Sub/MIP buttons
  not yet done (no xdotool).
- **Plan Stage 5 (CPR) — DONE (engine + wiring)** (this commit).
  - `widgets/cpr.{h,cpp}` (in aeskulap-widgets; pure statics): `Cpr::spline` Catmull-Rom
    arc-length decimated ~1px (threshold walker, endpoints exact, closed-curve support);
    `Cpr::reformat(vol, pts, out, w, h)` -> straightened int16 image, columns=path arc,
    rows=slices (row0=lowest z), bilinear (x,y) / linear z, signed domain. Headless
    ./tests/test_cpr.sh 15 checks ALL PASS.
  - `SeriesMath::write_sc` (public) persists the CPR frame as SC DICOM (signed int16).
  - MPRView: CPR mode (ToggleToolButton 'CPR' in StudyView toolbar; auto-enables MPR).
    In CPR mode: axial left-clicks add spline points (orange overlay on axial + projected
    ticks at crosshair-row on coronal/sagittal), Enter fires signal_cpr_compute,
    Esc clears. StudyView::on_cpr_compute -> Cpr::reformat -> write_sc -> FileLoader
    (result = new tab). Points are pixel units (no spacing used).
  - NOT click-verified in GUI: X display capture shows black desktop (compositor/lock),
    ptrace_scope=1, no xdotool. Engine fully unit-tested; interaction needs a manual
    user click-through.

## Repo / build / run
- Repo: `~/src/aeskulap` → GitHub `larsnygard/aeskulap` (master). Tag `0.2.9` exists;
  version bump to 0.3.0 only at program end (`project(aeskulap VERSION ...)` + About).
- Native C++17 gtkmm-3 + DCMTK 3.7. Build: `cmake -B build && make -C build -j$(nproc)`;
  install: `cmake --install build` (binary `~/.local/bin/aeskulap`, ~1.4 MB).
- Run on user's Xwayland: `DISPLAY=:0 XAUTHORITY=/run/user/1000/xauth_* ~/.local/bin/aeskulap /tmp/testvol/`
  (xauth filename changes per session — `ls /run/user/1000/xauth_*`). Launch via
  terminal(background=true), NOT `&`.
- Multi-slice test dataset: `/tmp/testvol/` (synthetic CT-like volume, pydicom venv `/tmp/dvenv`).
- gdb: `gdb -q --batch -ex run -ex "bt 12" --args ./src/aeskulap /tmp/testvol/img000.dcm`.
- `gtk-builder-tool validate src/aeskulap.ui` after every .ui edit.
- Skill `gtkmm2-gtkmm3-port` has the porting pitfalls. Commit each stage, **push to origin** (Lars' rule).
- LSP/clangd shows 'gtkmm.h not found' on every widgets/*.h — that's clangd missing pkg-config
  paths; **ignore it**, the cmake build is the truth.

## Key integration points (verified)
- **Render pipeline**: `widgets/asimpledisplay.cpp` — `effective_pixels()` applies
  Volume slab/projection + signed kernels; `rect_stretch16` applies window/level → palette LUT
  → RGBA. `refresh()` re-runs on window/level change.
- **DisplayParameters** (widgets/displayparameters.h): zoom/move/window/invert/`palette`/
  `volume`/`volume_index` — add `axis` here for MPR.
- **SeriesView** (widgets/seriesview.{h,cpp}): owns instances, `m_dispparam` vector,
  `ensure_volume()`, `refresh()`, `schedule_repaint(ms)`; scroll at `on_scroll_event`.
- **StudyView**: toolbar `m_toolbar->append(...)`; `m_selected` = selected SeriesView;
  `sync_postproc_controls()`; tabs via `m_notebook`; `on_series_selected` → sync UI.
- **Series context menu**: widgets/aseriesmenu.cpp.
- **Geometry**: `ImagePool::Instance`: `get_position()` (mm), `get_orientation()` (cosines),
  `spacing_x/y`, `pixels(frame)`, `bpp()`; z = sorted positions delta. Volume already uses these.
- **Window/level presets**: keyfile groups `windowlevel <modality>/<description>`;
  defaults in `Configuration::add_default_presets_ct()`.
- **AboutDialog**: src/aboutdialog.cpp (fully programmatic fields; glade widget kept for shell).

## Stages remaining
### Stage 4 — DONE; see status block above. Possible follow-up: full ops dialog
(Add/Max/Min/Avg + formula bar, shunting-yard) on top of SeriesMath (engine already int32 domain).
### Stage 5 — DONE; see status block. Manual GUI click-through checklist (user):
open 48-slice volume -> MPR on -> CPR on -> click ~5 points on axial -> Enter ->
new CPR tab appears (straightened strip, path x slices); Esc clears points.

## Windows port + CI binary releases (2026-11-06)
- **Windows build**: MSYS2 UCRT64 (`mingw-w64-ucrt-x86_64-{toolchain,cmake,ninja,pkgconf,gtkmm3,dcmtk,zlib,libpng,libtiff,openssl}`).
  CMake needs nothing special (pkg-config finds gtkmm-3.0 + dcmtk 3.7.1). Source changes:
  `src/main.cpp` (win32 console UTF-8, datadir from exe dir via `find_datadir()` — relocatable,
  works for local installs AND AppImage mounts; win32 block sets `GDK_PIXBUF_MODULE_FILE`/
  `GSETTINGS_SCHEMA_DIR` from `<exedir>/../share` + expands RELATIVE `loaders/...` tokens in the
  bundle's loaders.cache into a tmp cache), `src/aeskulap.rc.in` + `src/aeskulap.manifest`
  (icon + UTF-8 activation-context manifest, wired in CMakeLists under `WIN32`).
- **DCMTK link narrowing**: `CMakeLists.txt` filters `DCMTK_LIBRARIES` to the 14 modules
  actually `#include`d (ofstd oflog oficonv dcmdata i2d dcmimgle dcmimage dcmjpeg ijg8/12/16
  dcmjpls dcmtkcharls dcmtls dcmnet) — drops dcmrt/dcmtract/dcmpmap/dcmseg/dcmfg/dcmiod/xml/
  sndfile/openjp2 from the closure. Verified: all 3 ctest pass, binary links.
- **Bundle scripts** (tested locally on Linux; Windows path CI-only):
  `scripts/bundle-linux.sh <build> <out> [dcmtk-prefix]` — install to out, BFS ldd closure
  excluding glibc family, DCMTK name-filter copy from prefix, pixbuf loaders + schemas,
  strips, writes `bin/aeskulap.sh` launcher. Local result: 124M tree, 0 not-found deps.
  `scripts/bundle-windows.sh <build> <out>` — MSYS2 bash: exe + ldd DLLs into bin/,
  cmake --install -> share/, schemas, pixbuf loaders + `gdk-pixbuf-query-loaders.exe` cache
  rewritten to relative `loaders/` tokens.
- **CI**: `.github/workflows/release.yml` — jobs windows (MSYS2) + linux (ubuntu-24.04,
  DCMTK 3.7.1 built from source w/ cache + synthesized dcmtk.pc) run ctest gate + bundle;
  tag `v*` also publishes GitHub Release with zip+tar.gz. NOTE repo default branch = master
  (push HEAD:master AND HEAD:main; workflows must exist on default branch to dispatch).
- **CPack**: TBZ2 binary (bin+share, relocatable via find_datadir) + source pkg; ignore-files
  exclude vendored `dcmtk/` (3.5.3 legacy, dead), `web/`, build. Source pkg 332K clean.
- **Open items**: first CI run (watch `gh run watch <id>`); smoke rc gate on linux job is a
  heuristic (124=alive-under-xvfb ok); Windows job smoke only checks ldd, no GUI start.
- **macOS** (not started): vcpkg has dcmtk port; gtkmm3 via homebrew is GTK2-era pain —
  real cost estimate in chat history (macOS = medium-hard, Windows = done).

## Conventions
- Every stage: `make -C build -j` clean → `cmake --install build` → `./tests/test_render.sh`
  ALL PASS → GUI smoke (background=true, expect study+series loaded, alive, no CRITICALs) →
  commit ("0.3: <stage>: <what>") + **push**. Update this file's status block.
- `gtk_image_set_pixel_size` CRITICAL seen in old builds is FIXED in current tree; if it
  reappears, a stock icon lookup regressed.
