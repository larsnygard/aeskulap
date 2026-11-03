import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

// Cornerstone3D recipe: keep the ESM packages (core/tools/loader/metadata)
// out of prebundling so Vite serves them as real modules (module web workers
// + new URL assets survive), and force their transitive CJS deps (vtk.js ->
// xmlbuilder2 -> node events, gl-matrix, ndarray, ...) through the bundler so
// interop shims apply. Mirrors OHIF's Vite settings.
export default defineConfig({
  plugins: [react()],
  base: './',
  define: {
    'process.env.NODE_ENV': JSON.stringify('development'),
    global: 'globalThis',
  },
  optimizeDeps: {
    // xmlbuilder2 requires Node's 'events'; Vite externalizes it to an empty
    // stub, so class-extends crashes. Point it at the browser polyfill.
    esbuildOptions: {
      alias: { events: 'events' },
    },
    include: [
      'fast-deep-equal', 'seedrandom', 'gl-matrix', 'ndarray', 'comlink', 'pako', 'uuid',
      'lodash.get', 'lodash.clonedeep', 'loglevel', 'browser-or-node', 'webworker-promise',
      'dicom-parser',
      'wslink', 'spark-md5', 'utif', 'fflate', 'iconv-lite', 'is-buffer', 'iota-array',
      'xmlbuilder2', 'js-yaml',
      '@cornerstonejs/codec-libjpeg-turbo-8bit/decodewasmjs',
      '@cornerstonejs/codec-libjpeg-turbo-12bit/wasmjs',
      '@cornerstonejs/codec-openjpeg/decodewasmjs',
      '@cornerstonejs/codec-charls/decodewasmjs',
      '@cornerstonejs/codec-openjph/wasmjs',
      '@cornerstonejs/codec-libjxl/decodewasmjs',
      '@cornerstonejs/codec-big-endian',
      '@cornerstonejs/codec-little-endian',
      '@cornerstonejs/jpeg-lossless-decoder-js',
    ],
    exclude: [
      '@cornerstonejs/core',
      '@cornerstonejs/tools',
      '@cornerstonejs/dicom-image-loader',
      '@cornerstonejs/metadata',
      '@cornerstonejs/utils',
      '@kitware/vtk.js',
    ],
  },
  build: { target: 'es2022', chunkSizeWarningLimit: 4000 },
  server: {
    port: 5173,
    proxy: {
      '/api': 'http://localhost:5174',
      '/dicomfile': 'http://localhost:5174',
    },
  },
});
