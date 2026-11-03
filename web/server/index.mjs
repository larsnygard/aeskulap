// Aeskulap 0.3 web — DICOMstore: ingest, index, and serve Part-10 files.
// Replaces the 2005 C++ DCMTK network loader with a local REST store.
import express from 'express';
import multer from 'multer';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import dcmjs from 'dcmjs';
const { DicomMessage, DicomMetaDictionary } = dcmjs.data;

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const DATA_DIR = process.env.AESKULAP_DATA || path.join(__dirname, '..', 'server-data');
const TRASH_DIR = path.join(DATA_DIR, '.trash');
fs.mkdirSync(TRASH_DIR, { recursive: true });

const app = express();
const upload = multer({ storage: multer.memoryStorage(), limits: { fileSize: 512 * 1024 * 1024 } });

const str = (v) => (Array.isArray(v) ? v.join('\\') : String(v ?? '')).trim();
const num = (v) => Number(str(v).split('\\')[0]) || 0;

function naturalName(v) {
  if (!v) return '';
  if (Array.isArray(v)) return naturalName(v[0]);
  if (typeof v === 'object') return str(v.Alphabetic);
  return str(v);
}

function ingest(buffer, originalName) {
  const ds = DicomMessage.readFile(
    buffer.buffer.slice(buffer.byteOffset, buffer.byteOffset + buffer.byteLength)
  );
  const nat = DicomMetaDictionary.naturalizeDataset(ds.dict);
  const studyUID = str(nat.StudyInstanceUID);
  const seriesUID = str(nat.SeriesInstanceUID);
  const sopUID = str(nat.SOPInstanceUID);
  if (!studyUID || !seriesUID || !sopUID) throw new Error('missing study/series/sop UID');

  const dir = path.join(DATA_DIR, studyUID, seriesUID);
  fs.mkdirSync(dir, { recursive: true });
  const file = path.join(dir, `${sopUID}.dcm`);
  fs.writeFileSync(file, buffer);

  return {
    studyUID,
    seriesUID,
    sopUID,
    size: buffer.length,
    originalName: originalName || '',
    meta: {
      patientName: naturalName(nat.PatientName),
      patientId: str(nat.PatientID),
      patientSex: str(nat.PatientSex),
      patientBirthDate: str(nat.PatientBirthDate),
      studyDate: str(nat.StudyDate),
      studyTime: str(nat.StudyTime),
      studyDescription: str(nat.StudyDescription),
      accessionNumber: str(nat.AccessionNumber),
      modality: str(nat.Modality),
      seriesDescription: str(nat.SeriesDescription),
      seriesNumber: num(nat.SeriesNumber),
      instanceNumber: num(nat.InstanceNumber),
      rows: num(nat.Rows),
      columns: num(nat.Columns),
      slices: nat.NumberOfFrames ? num(nat.NumberOfFrames) : 1,
      imagePositionPatient: Array.isArray(nat.ImagePositionPatient)
        ? nat.ImagePositionPatient.map(Number)
        : null,
    },
  };
}

function rebuildIndex() {
  const studies = new Map();
  for (const studyUID of fs.readdirSync(DATA_DIR)) {
    if (studyUID.startsWith('.')) continue;
    const studyDir = path.join(DATA_DIR, studyUID);
    if (!fs.statSync(studyDir).isDirectory()) continue;
    for (const seriesUID of fs.readdirSync(studyDir)) {
      const seriesDir = path.join(studyDir, seriesUID);
      if (!fs.statSync(seriesDir).isDirectory()) continue;
      for (const f of fs.readdirSync(seriesDir)) {
        if (!f.endsWith('.dcm')) continue;
        try {
          const buf = fs.readFileSync(path.join(seriesDir, f));
          const rec = ingestRecord(buf);
          addRecord(studies, rec);
        } catch (e) {
          console.warn('skip', studyUID, seriesUID, f, e.message);
        }
      }
    }
  }
  return studies;
}
function ingestRecord(buffer) {
  return ingestLite(buffer);
}
function ingestLite(buffer) {
  const ds = DicomMessage.readFile(
    buffer.buffer.slice(buffer.byteOffset, buffer.byteOffset + buffer.byteLength)
  );
  const nat = DicomMetaDictionary.naturalizeDataset(ds.dict);
  return ingestMetaOnly(nat);
}
function ingestMetaOnly(nat) {
  const studyUID = str(nat.StudyInstanceUID);
  const seriesUID = str(nat.SeriesInstanceUID);
  const sopUID = str(nat.SOPInstanceUID);
  return {
    studyUID,
    seriesUID,
    sopUID,
    meta: {
      patientName: naturalName(nat.PatientName),
      patientId: str(nat.PatientID),
      patientSex: str(nat.PatientSex),
      patientBirthDate: str(nat.PatientBirthDate),
      studyDate: str(nat.StudyDate),
      studyTime: str(nat.StudyTime),
      studyDescription: str(nat.StudyDescription),
      accessionNumber: str(nat.AccessionNumber),
      modality: str(nat.Modality),
      seriesDescription: str(nat.SeriesDescription),
      seriesNumber: num(nat.SeriesNumber),
      instanceNumber: num(nat.InstanceNumber),
      rows: num(nat.Rows),
      columns: num(nat.Columns),
      slices: nat.NumberOfFrames ? num(nat.NumberOfFrames) : 1,
      imagePositionPatient: Array.isArray(nat.ImagePositionPatient)
        ? nat.ImagePositionPatient.map(Number)
        : null,
    },
  };
}

const store = { studies: rebuildIndex() };

function addRecord(studies, rec) {
  let s = studies.get(rec.studyUID);
  if (!s) {
    s = { studyUID: rec.studyUID, meta: rec.meta, series: new Map(), instanceCount: 0 };
    studies.set(rec.studyUID, s);
  }
  let se = s.series.get(rec.seriesUID);
  if (!se) {
    se = { seriesUID: rec.seriesUID, meta: rec.meta, instances: [] };
    s.series.set(rec.seriesUID, se);
  }
  se.instances.push(rec);
  s.instanceCount++;
}

function serializeStudy(s) {
  const series = [...s.series.values()]
    .sort((a, b) => a.meta.seriesNumber - b.meta.seriesNumber)
    .map((se) => {
      const instances = se.instances
        .sort(instanceSort)
        .map((i) => ({
          sopUID: i.sopUID,
          url: `/dicomfile/${s.studyUID}/${se.seriesUID}/${i.sopUID}.dcm`,
          ...i.meta,
        }));
      return {
        seriesUID: se.seriesUID,
        meta: se.meta,
        count: instances.length,
        instances,
      };
    });
  return {
    studyUID: s.studyUID,
    meta: s.meta,
    instanceCount: s.instanceCount,
    series,
  };
}

function instanceSort(a, b) {
  const za = a.meta.imagePositionPatient ? a.meta.imagePositionPatient[2] : null;
  const zb = b.meta.imagePositionPatient ? b.meta.imagePositionPatient[2] : null;
  if (za !== null && zb !== null && za !== zb) return za - zb;
  return a.meta.instanceNumber - b.meta.instanceNumber;
}

app.get('/api/studies', (_req, res) => {
  const list = [...store.studies.values()]
    .map(serializeStudy)
    .sort((a, b) => (b.meta.studyDate || '').localeCompare(a.meta.studyDate || ''));
  res.json({ studies: list });
});

app.post('/api/upload', upload.array('files', 10000), (req, res) => {
  const accepted = [];
  const rejected = [];
  for (const f of req.files || []) {
    try {
      const rec = ingest(f.buffer, f.originalname);
      const studies = store.studies;
      addRecord(studies, rec);
      accepted.push({ name: f.originalname, ...rec.meta });
    } catch (e) {
      rejected.push({ name: f.originalname, error: e.message });
    }
  }
  res.json({ accepted: accepted.length, rejected });
});

app.delete('/api/studies/:studyUID', (req, res) => {
  const { studyUID } = req.params;
  if (!/^[0-9.]+$/.test(studyUID)) return res.status(400).json({ error: 'bad uid' });
  const src = path.join(DATA_DIR, studyUID);
  if (!fs.existsSync(src)) return res.status(404).json({ error: 'not found' });
  const dest = path.join(TRASH_DIR, `${studyUID}.${Date.now()}`);
  fs.renameSync(src, dest); // reversible: trashed, not deleted
  store.studies.delete(studyUID);
  res.json({ trashed: dest });
});

app.get('/dicomfile/:studyUID/:seriesUID/:sopUID.dcm', (req, res) => {
  const { studyUID, seriesUID, sopUID } = req.params;
  if (![studyUID, seriesUID, sopUID].every((u) => /^[0-9.]+$/.test(u))) {
    return res.status(400).end();
  }
  const file = path.join(DATA_DIR, studyUID, seriesUID, `${sopUID}.dcm`);
  if (!fs.existsSync(file)) return res.status(404).end();
  res.setHeader('Content-Type', 'application/dicom');
  fs.createReadStream(file).pipe(res);
});

// Seed bundled demo files so a fresh store has content (Aeskulap ships samples)
function seedDemo() {
  const demoDir = path.join(__dirname, '..', 'public', 'data');
  if (!fs.existsSync(demoDir)) return;
  for (const f of fs.readdirSync(demoDir)) {
    if (!f.endsWith('.dcm')) continue;
    try {
      const rec = ingest(fs.readFileSync(path.join(demoDir, f)), f);
      const se = store.studies.get(rec.studyUID)?.series.get(rec.seriesUID);
      if (se && se.instances.some((i) => i.sopUID === rec.sopUID)) continue;
      addRecord(store.studies, rec);
    } catch (e) {
      console.warn('seed skip', f, e.message);
    }
  }
}
seedDemo();

// In production, serve the built SPA directly (dist/ next to server/).
const DIST = path.join(__dirname, '..', 'dist');
if (fs.existsSync(path.join(DIST, 'index.html'))) {
  app.use(express.static(DIST));
  app.get(/^(?!\/api|\/dicomfile).*/, (_req, res) =>
    res.sendFile(path.join(DIST, 'index.html'))
  );
  console.log('serving built SPA from', DIST);
}

const PORT = Number(process.env.AESKULAP_PORT || 5174);
const HOST = process.env.AESKULAP_HOST || '0.0.0.0';
app.listen(PORT, HOST, () =>
  console.log(`aeskulap-store listening on http://${HOST}:${PORT} (data: ${DATA_DIR})`)
);
