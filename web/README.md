# Aeskulap Web — 2026 DICOM viewer

Modern rewrite of the abandoned Aeskulap (2005, C++/wxWidgets) as a web app:
React 18 + TypeScript + Vite, Cornerstone3D 5.15 rendering, and a small
Express DICOM store that plays the role of the old network loader.

## Run (dev)

```bash
npm install
node server/index.mjs   # DICOM store + API on :5174
npx vite --port 5173    # frontend (proxies /api and /dicomfile)
```

Open http://localhost:5173/ — demo studies from `public/data/` are seeded
automatically. Drag-and-drop or "+ DICOM" uploads .dcm files into the store.

## Run (production)

```bash
npm run build           # -> dist/
node server/index.mjs   # now also serves the built SPA on :5174
```

## Layout

- `server/index.mjs` — ingest (multer), dcmjs metadata index, WADO-style file
  serving, trash-preserving delete, demo seeding.
- `src/viewer.ts` — Cornerstone engine, stack viewports, tool bindings.
- `src/App.tsx` — studies sidebar, toolbar, grid layouts, frame slider.
- `e2e.cjs` — Playwright smoke test (needs a Chromium at
  `~/.cache/ms-playwright/`; run against the production server on :5174).

## Notes

- `vite.config.ts` follows the OHIF Cornerstone recipe: cornerstone ESM stays
  out of dep prebundling; transitive CJS (xmlbuilder2, gl-matrix, codecs…)
  is included so Vite applies ESM interop shims.
- Data lives in `server-data/` (override with `AESKULAP_DATA`, port with
  `AESKULAP_PORT`). Deletes move files to `server-data/.trash/`.
