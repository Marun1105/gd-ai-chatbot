// Renders the ray-marched Grin in headless Chromium.
// Usage: node render-sdf.js <port> <outDir> [which]
const { chromium } = require('playwright');
const fs = require('fs');
const path = require('path');

const SHOTS = {
  face: { w: 640, h: 640, scale: 2, cam: [0.05, 1.5, 2.35], target: [0, 1.56, 0], fov: 34, roar: 0, tilt: -0.14 },
  roar: { w: 640, h: 640, scale: 2, cam: [0.05, 1.5, 2.35], target: [0, 1.56, 0], fov: 34, roar: 1, tilt: -0.2 },
  full: { w: 512, h: 768, scale: 2, cam: [1.25, 0.05, 7.6], target: [0.05, -0.12, 0.5], fov: 38, roar: 0, tilt: -0.26, pose: 1 },
};

(async () => {
  const [port, outDir, which] = process.argv.slice(2);
  const browser = await chromium.launch({ args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', '--disable-gpu-watchdog'] });
  const page = await browser.newPage();
  page.on('pageerror', e => console.log('page error:', e.message));
  page.on('console', m => { if (m.type() === 'error') console.log('console:', m.text()); });
  await page.goto(`http://127.0.0.1:${port}/sdf.html`);
  await page.waitForFunction(() => window.ready === true, null, { timeout: 30000 });
  for (const [name, opts] of Object.entries(SHOTS)) {
    if (which && which !== name) continue;
    const started = Date.now();
    const url = await page.evaluate(o => window.renderSdf(o), opts);
    fs.writeFileSync(path.join(outDir, `grin-${name}.png`), Buffer.from(url.split(',')[1], 'base64'));
    console.log(name, ((Date.now() - started) / 1000).toFixed(1) + 's');
    if (name === 'full') {
      const at = p => page.evaluate(([o, p]) => window.projectPoint(o, p), [opts, p]);
      const head = await at([0, 1.24, 0.74]);
      const headSide = await at([0.36, 1.24, 0.74]);
      const chest = await at([0, 0.55, 0]);
      const hips = await at([0, -0.8, 0]);
      console.log('hit areas', JSON.stringify({ head, headRadiusX: headSide[0] - head[0], chest, hips }));
    }
  }
  await browser.close();
})();
