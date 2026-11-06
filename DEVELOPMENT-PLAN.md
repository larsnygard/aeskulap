# Aeskulap 0.2.9 → 0.3 Feature Program

**Read-me-first handoff plan.** If your context is fresh, start here.

## Repo / build / run
- Repo: `~/src/aeskulap` → GitHub `larsnygard/aeskulap` (master, tag `0.2.9`).
- Native C++17 gtkmm-3 + DCMTK 3.7. Build: `cmake -B build && cd build && make -j$(nproc) && make install` (prefix `~/.local`, binary `~/.local/bin/aeskulap`, ~1.3 MB).
- Run on the user's Xwayland: `DISPLAY=:0 XAUTHORITY=/run/user/1000/xauth_* ~/.local/bin/aeskulap [file.dcm]`
  (xauth filename changes per session — `ls /run/user/1000/xauth_*`).
- Smoke test: launch with `/tmp/test.dcm` (synthetic SC image; regenerate with pydicom venv `/tmp/dvenv` if missing). Expect log lines `opened file:`, `new study`, `new series`, app stays alive. Exit 124 from `timeout` = GOOD (killed while running).
- gdb: `gdb -q --batch -ex run -ex "bt 12" --args ./src/aeskulap /tmp/test.dcm`.
- Layout debug trick: `-DAESKULAP_LAYOUT_DEBUG` + `frame->foreach` allocation dump (removed from tree; re-add temporarily when geometry misbehaves).
- `gtk-builder-tool validate src/aeskulap.ui` after every .ui edit.
- Skill `gtkmm2-gtkmm3-port` (skill_view) has all porting pitfalls. Commit each stage, push to origin, per Lars' "always push" convention.

## State at plan time (done, do not redo)
- Full gtkmm3/DCMTK3.7 port builds, links, installs, opens DICOM, render threads finish. Commits `fad0d13`, `8209925` (layout packing fix). Tag 0.2.9 at fad0d13 (force-move pending, user approval needed).
- `src/glade_compat.h` = libglademm replacement. `aeskulap.ui` = converted UI.
- Config: GKeyFile backend `configuration/aconfiguration-keyfile.cpp`.

## Key integration points (verified by survey)
- **Render pipeline**: `widgets/asimpledisplay.cpp` → `render()` calls `rect_stretch8/rect_stretch16` (greyscale→pixbuf). THE hook for LUT/palettes/kernels/projections. `bitstretch()` → `render()`. `refresh()` re-runs on window/level change. `DisplayParameters` (widgets/displayparameters.h) carries per-series params (zoom/move/window/invert) — extend for LUT/kernel/slab.
- **Scroll**: `SeriesView::on_scroll_event` (seriesview.cpp:203) already scrolls slices; connect Shift+scroll → slab logic there. `m_instance` vector = instances in display order; `m_offset` = first visible index.
- **Geometry**: `ImagePool::Instance` (imagepool/poolinstance.h): `get_position()` (x,y,z mm), `get_orientation()` (row/col direction cosines), `spacing_x/y`, `pixel_value(x,y,frame)`, `pixels(frame)`, `bpp()`, depth. z-spacing = delta of sorted positions.
- **Series/Study**: imagepool/poolseries.h, poolstudy.h — `add_instance`, `signal_series_added`. MainWindow `on_study_added` (src/mainwindow.cpp:278) appends StudyView tab.
- **Window/level presets**: settings dialog tree `presets_windowlevels` + keyfile groups `windowlevel <modality>/<description>`; Aeskulap::WindowLevelToolButton (awindowleveltoolbutton.*) loads modality presets from config.
- **Toolbar buttons** in studyview.cpp: `m_toolbar->append(...)` — pattern of imagelayouttoolbutton/awindowleveltoolbutton for new toolbar widgets.
- **AboutDialog**: src/aboutdialog.{h,cpp}, loaded via `get_widget_derived("aboutdialog",...)` from mainwindow; set fields programmatically (version, GPL v2, authors, website) — GTK3 `Gtk::AboutDialog` API.
- UI file: `src/aeskulap.ui`; remember `<packing>` AFTER `<object>`, box children need explicit expand/fill.

## Stages (each: implement → build → smoke-test on DISPLAY=:0 → commit+push)
### Stage 1 — quick wins
1. Verify scroll wheel works on a loaded series (log on_scroll_event; fix EventBox mask if events missing: add GDK_SCROLL_MASK to window).
2. Shift+scroll = slab thickness (MIP slab around center slice). Store slab count in DisplayParameters; projection computed in render path: for 16-bit, min/max/mean over slab of instances (need slab data: compute in SeriesView which owns all instances — project CPU-side into a synthetic pixel buffer the Display can stretch; design: `InstanceProxy`? Simplest: SeriesView builds projected 16-bit buffer on slab change, feeds via a pseudo-Instance (ImagePool::Instance is heavy) → ALTERNATIVE: add `set_volume_overlay()`/`Display::set_projected(guint16*, w,h)` and rect_stretch16 uses it). Also menu/toolbar modes: NONE/MIP/MINIP/AIP.
3. CT presets seeded into config defaults: Bone (W1700/L400→center 400? use classic: Bone W2000 C300; Soft tissue W350 C50; Lung W1500 C-600; Mediastinum W350 C50; Abdomen W350 C40; MIP handled as mode not preset) — seed via keyfile ensure_defaults() for `windowlevel CT/<name>`.
4. About dialog: programmatic (name Aeskulap, version 0.2.9, comments GPL-2.0+, copyright 2005 A. Pipelka / 2026 fork L. Nygård, website aeskulap.nongnu.org + github), remove glade dependency.

### Stage 2 — palettes + kernels
- LUT enum + 256-entry (or 65536→256 LUT) applied in rect_stretch16/8: presets Grayscale, Inverse, Hot Iron, STET/Rainbow, (Jet, Pure red). Add palette picker toolbar button (toolbutton pattern).
- Convolution 3×3 per series: Identity, Sharpen, Edge (Sobel-ish), Blur (gauss), Noise-reduce (median 3×3 cheap, or bilateral-lite) — apply to source 16-bit before windowing in projection/stretch path; store kernel id in DisplayParameters; toolbar popup menu.
- Ensure invert + palette compose correctly (invert applies after LUT).

### Stage 3 — MPR
- New `MPRView` widget (3 panes: Axial/Coronal/Sagittal): volume = collect series instances sorted by z (positions+orientation), resample coronal (x,z) and sagittal (y,z) planes through crosshair. Use Instance::pixels raw 16-bit + spacing. Crosshair linked across panes (signal). Opens via context menu "3D → MPR" on series (aseriesmenu.cpp) / toolbar.
- Volume memory: cap (e.g. 512×512×300 ≈ 750 MB int16 — use int16 raw, 2-byte = ~150MB max; guard by dims, downsample if bigger).
- Scroll in MPR pane = move crosshair along its axis; window/level shared.

### Stage 4 — Series arithmetic
- Dialog: pick series A + series B + op (Add, Subtract A-B, Subtract B-A, Maximum, Minimum, Average; presets CT-CT (sub), CT-MRI (sub) with resample B→A grid (nearest, by position/orientation), formula bar: tokens A B + - * / min max avg, simple shunting-yard evaluator).
- Result = new tab (synthesize a study/series header copy). imagepool: Loader has file→instance path; build in-memory instances via a new `PoolCompose` that creates Instance via friend/Loader hooks (Instance ctor protected — friend Loader; add friend Compose).
- UI: Tools menu "Image arithmetic…".

### Stage 5 — Curved planar reformation (CPR)
- Interactive centerline: in axial SeriesView, Shift+left-click adds control points (world mm) along vessel; spline (Catmull-Rom) through points; sample perpendicular to tangent in the plane normal (use orientation cosines + position path frame: parallel transport).
- Result straightened image (columns=along-path, rows=cutline) shown as a new series tab. Toolbar toggle "CPR mode".

## Conventions
- Every stage: `make -j` clean, `make install`, smoke test (incl. a DICOM with multi-slice series — generate 32-slice CT-like volume in /tmp/testvol/ with pydicom, use as multi-instance test dataset), commit ("0.3: <stage>: <what>") + push. Update this file's checkboxes.
- Update `project(aeskulap VERSION ...)` + About version at program end (0.3.0).
- Tests: keep a Node-free shell test? Use `scripts/` dir: `test_smoke.sh` (launch, assert alive 8s, grep opened/new study/new series).
