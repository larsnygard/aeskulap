import { useEffect, useRef, useState, useCallback } from 'react';
import { initCornerstone, Viewer, type ToolName } from './viewer';
import './styles.css';

interface InstanceMeta {
  sopUID: string;
  url: string;
  instanceNumber: number;
  rows: number;
  columns: number;
  slices: number;
}
interface Series {
  seriesUID: string;
  meta: { modality: string; seriesDescription: string; seriesNumber: number; studyDate: string; studyDescription: string; patientName: string; patientId: string };
  count: number;
  instances: InstanceMeta[];
}
interface Study {
  studyUID: string;
  meta: { patientName: string; patientId: string; studyDate: string; studyDescription: string; accessionNumber: string };
  instanceCount: number;
  series: Series[];
}

const TOOLS: { name: ToolName; label: string; key: string }[] = [
  { name: 'WindowLevel', label: 'WW/WL', key: '1' },
  { name: 'Pan', label: 'Pan', key: '2' },
  { name: 'Zoom', label: 'Zoom', key: '3' },
  { name: 'StackScroll', label: 'Scroll', key: '4' },
  { name: 'Length', label: 'Ruler', key: '5' },
  { name: 'Probe', label: 'Probe', key: '6' },
  { name: 'Magnify', label: 'Magnify', key: '7' },
];

export default function App() {
  const [studies, setStudies] = useState<Study[]>([]);
  const [selected, setSelected] = useState<Study | null>(null);
  const [viewerStudy, setViewerStudy] = useState<Study | null>(null);
  const [tool, setTool] = useState<ToolName>('WindowLevel');
  const [invert, setInvert] = useState(false);
  const [frame, setFrame] = useState({ index: 0, count: 0 });
  const [grid, setGrid] = useState(1);
  const [status, setStatus] = useState('');
  const [ready, setReady] = useState(false);

  const viewerRef = useRef<Viewer | null>(null);
  const gridRef = useRef<HTMLDivElement>(null);
  const cellRefs = useRef<(HTMLDivElement | null)[]>([]);
  const seriesByViewport = useRef<Map<string, Series>>(new Map());

  const refresh = useCallback(() => {
    fetch('/api/studies')
      .then((r) => r.json())
      .then((d) => setStudies(d.studies as Study[]))
      .catch(() => setStatus('store offline'));
  }, []);

  useEffect(() => {
    refresh();
    initCornerstone().then(() => setReady(true), (e) => setStatus(`init failed: ${e?.message || e}`));
  }, [refresh]);

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      const t = TOOLS.find((t) => t.key === e.key);
      if (t && viewerRef.current) setToolState(t.name);
      const v = viewerRef.current;
      if (!v) return;
      if (e.key === 'ArrowUp' || e.key === 'PageUp') v.scroll(-1);
      if (e.key === 'ArrowDown' || e.key === 'PageDown') v.scroll(1);
      if (e.key === 'Home') v.scroll(-1e9);
      if (e.key === 'End') v.scroll(1e9);
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, []);

  const setToolState = (name: ToolName) => {
    setTool(name);
    viewerRef.current?.setTool(name);
  };

  // (Re)build the viewport grid whenever a study opens or the grid size changes.
  useEffect(() => {
    if (!ready || !viewerStudy) return;
    const build = () => {
      if (!viewerRef.current) viewerRef.current = new Viewer();
      const v = viewerRef.current;
      const cells = cellRefs.current.slice(0, grid).filter(Boolean) as HTMLDivElement[];
      v.setGrid(1, grid, cells);
      seriesByViewport.current.clear();
      const subs: (() => void)[] = [];
      subs.push(v.onNewImage((id, index, count) => {
        if (id === v.activeId) setFrame({ index, count });
      }));
      const loads = viewerStudy.series
        .slice(0, grid)
        .map((series, i) => {
          const id = `vp-${i}`;
          seriesByViewport.current.set(id, series);
          return v.loadSeries(id, series.instances.map((inst) => inst.url)).catch((e) =>
            console.error('load', series.seriesUID, e)
          );
        });
      Promise.all(loads).then(() => setStatus(''));
      return () => subs.forEach((s) => s());
    };
    const cleanup = build();
    return cleanup;
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [ready, viewerStudy, grid]);

  const onCellClick = (i: number) => {
    viewerRef.current?.setActive(`vp-${i}`);
  };

  const onUpload = async (files: FileList | File[]) => {
    const fd = new FormData();
    for (const f of Array.from(files)) fd.append('files', f);
    setStatus(`uploading ${files.length} file(s)…`);
    const r = await fetch('/api/upload', { method: 'POST', body: fd });
    const d = await r.json();
    setStatus(d.rejected?.length ? `${d.accepted} in, ${d.rejected.length} rejected: ${d.rejected[0].name} — ${d.rejected[0].error}` : `${d.accepted} file(s) added`);
    refresh();
  };

  const trashStudy = async (studyUID: string) => {
    await fetch(`/api/studies/${studyUID}`, { method: 'DELETE' });
    if (selected?.studyUID === studyUID) setSelected(null);
    if (viewerStudy?.studyUID === studyUID) setViewerStudy(null);
    refresh();
  };

  return (
    <div
      className="app"
      onDragOver={(e) => e.preventDefault()}
      onDrop={(e) => {
        e.preventDefault();
        if (e.dataTransfer.files.length) onUpload(e.dataTransfer.files);
      }}
    >
      <header>
        <span className="brand">Aeskulap 0.3</span>
        {viewerStudy && (
          <>
            <div className="toolbar">
              {TOOLS.map((t) => (
                <button
                  key={t.name}
                  className={tool === t.name ? 'on' : ''}
                  title={`${t.label} (${t.key})`}
                  onClick={() => setToolState(t.name)}
                >
                  {t.label}
                </button>
              ))}
              <span className="sep" />
              <button
                className={invert ? 'on' : ''}
                title="Invert (like the 2005 'invert' windowlevel)"
                onClick={() => {
                  setInvert(!invert);
                  viewerRef.current?.setInvert(!invert);
                }}
              >
                Invert
              </button>
              <button title="Reset" onClick={() => viewerRef.current?.reset()}>Reset</button>
              <span className="sep" />
              {[1, 2, 3, 4, 6].map((n) => (
                <button key={n} className={grid === n ? 'on' : ''} onClick={() => setGrid(n)}>
                  {n}×1
                </button>
              ))}
            </div>
            <div className="frameslider">
              <input
                type="range"
                min={0}
                max={Math.max(frame.count - 1, 0)}
                value={frame.index}
                onChange={(e) => {
                  const v = viewerRef.current;
                  if (!v) return;
                  const vp = v.activeViewport;
                  const idx = Number(e.target.value);
                  vp.scroll?.(idx - vp.getCurrentImageIdIndex(), false);
                }}
              />
              <span className="frameno">{frame.index + 1}/{frame.count}</span>
            </div>
          </>
        )}
        <span className="status">{status}</span>
      </header>
      <main>
        <aside className="browser">
          <div className="browser-head">
            Studies
            <label className="uploadbtn">
              + DICOM
              <input
                type="file"
                multiple
                accept=".dcm,application/dicom"
                onChange={(e) => e.target.files && onUpload(e.target.files)}
              />
            </label>
          </div>
          {studies.map((s) => (
            <div
              key={s.studyUID}
              className={`study ${selected?.studyUID === s.studyUID ? 'sel' : ''}`}
              onClick={() => setSelected(s)}
              onDoubleClick={() => setViewerStudy(s)}
            >
              <div className="patient">{s.meta.patientName || '(no name)'}</div>
              <div className="line">
                {s.meta.studyDate} · {s.meta.studyDescription || 'Study'} · {s.instanceCount} img
              </div>
              {selected?.studyUID === s.studyUID && (
                <>
                  {s.series.map((se) => (
                    <div
                      className="series"
                      key={se.seriesUID}
                      onDoubleClick={() => setViewerStudy(s)}
                    >
                      {se.meta.modality} · {se.meta.seriesDescription || `Series ${se.meta.seriesNumber}`} · {se.count}
                    </div>
                  ))}
                  <button className="open" onClick={() => setViewerStudy(s)}>Open all</button>
                  {' '}
                  <button className="danger" onClick={() => trashStudy(s.studyUID)}>Trash</button>
                </>
              )}
            </div>
          ))}
          {studies.length === 0 && <div className="empty">Drop .dcm files anywhere, or click + DICOM.</div>}
        </aside>
        <section className="viewer">
          {viewerStudy ? (
            <div ref={gridRef} className={`grid g${grid}`}>
              {Array.from({ length: grid }, (_, i) => (
                <div
                  key={`${viewerStudy.studyUID}-${grid}-${i}`}
                  ref={(el) => { cellRefs.current[i] = el; }}
                  className="cell"
                  onClick={() => onCellClick(i)}
                />
              ))}
            </div>
          ) : (
            <div className="placeholder">
              Aeskulap — open source DICOM viewer, revived for the web.<br />
              Pick a study on the left, double-click to open.
            </div>
          )}
        </section>
      </main>
    </div>
  );
}
