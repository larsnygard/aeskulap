// E2E: load app in headless Chromium, open a study, verify the viewport
// paints actual pixel content, screenshot, and check the frame slider advances.
const { chromium } = require('playwright');
const path = require('path');
const fs = require('fs');

const EXEC = '/home/lars/.cache/ms-playwright/chromium-1234/chrome-linux64/chrome';
const OUT = '/home/lars/.hermes/cache/scratch/aeskulap-e2e';
fs.mkdirSync(OUT, { recursive: true });

(async () => {
  const browser = await chromium.launch({
    executablePath: EXEC,
    args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--no-sandbox'],
  });
  const page = await browser.newPage({ viewport: { width: 1400, height: 900 } });
  const errors = [];
  page.on('pageerror', (e) => errors.push('pageerror: ' + e.message));
  page.on('console', (m) => { if (m.type() === 'error') errors.push('console: ' + m.text().slice(0, 300)); });

  await page.goto('http://localhost:5174/', { waitUntil: 'networkidle' });
  await page.screenshot({ path: path.join(OUT, '1-browser.png') });

  // open the first study
  await page.click('.study');
  await page.click('button.open');
  await page.waitForSelector('.cell canvas', { timeout: 15000 });
  await page.waitForTimeout(3500);
  await page.screenshot({ path: path.join(OUT, '2-opened.png') });

  // Verify the canvas has real (non-black, varied) pixel content
  const pixelStats = await page.evaluate(() => {
    const canvas = document.querySelector('.cell canvas');
    if (!canvas) return { ok: false, why: 'no canvas' };
    const off = document.createElement('canvas');
    off.width = canvas.width; off.height = canvas.height;
    const ctx = off.getContext('2d');
    ctx.drawImage(canvas, 0, 0);
    const d = ctx.getImageData(0, 0, off.width, off.height).data;
    const hist = new Set();
    let sum = 0, nonzero = 0;
    for (let i = 0; i < d.length; i += 4) {
      const v = d[i];
      hist.add(v >> 4);
      sum += v;
      if (v > 0) nonzero++;
    }
    return { ok: true, uniqueBuckets: hist.size, mean: sum / (d.length / 4), nonzeroPct: (100 * nonzero) / (d.length / 4) };
  });

  // Frame slider / scroll: press End then check frame counter text
  await page.click('.cell');
  await page.keyboard.press('End');
  await page.waitForTimeout(800);
  const frameno = await page.textContent('.frameno');
  await page.screenshot({ path: path.join(OUT, '3-end-frame.png') });

  // Tool switching
  await page.click('button[title^="Ruler"]');
  await page.waitForTimeout(300);

  // Multi-viewport grid
  await page.click('button:has-text("2×1")');
  await page.waitForTimeout(3500);
  const canvases = await page.locator('.cell canvas').count();
  await page.screenshot({ path: path.join(OUT, '4-grid2.png') });

  // Upload flow: POST a fresh copy via the UI's file input
  const dcm = '/home/lars/src/aeskulap/web/public/data/MR_small.dcm';
  await page.setInputFiles('input[type=file]', dcm);
  await page.waitForTimeout(1500);
  const status = await page.textContent('.status');

  console.log(JSON.stringify({ pixelStats, frameno, canvases, uploadStatus: status, errors: errors.slice(0, 12) }, null, 2));
  await browser.close();
})().catch((e) => { console.error('FATAL', e); process.exit(1); });
