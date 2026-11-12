// Explore and render the CC0 horror monster.
// node render-fbx.js <port> <outDir> sheet <file> <skin>   -> contact sheet of every clip
// node render-fbx.js <port> <outDir> shots <file> <skin> <json shots>
const { chromium } = require('playwright');
const fs = require('fs');
const path = require('path');

(async () => {
  const [port, outDir, mode, file, skin, shotsJson, tint = 'none'] = process.argv.slice(2);
  const browser = await chromium.launch({ args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', '--disable-gpu-watchdog'] });
  const page = await browser.newPage();
  page.on('pageerror', e => console.log('page error:', e.message));
  page.on('console', m => { if (m.type() === 'error' || m.type() === 'warning') console.log('console:', m.text().slice(0, 200)); });
  await page.goto(`http://127.0.0.1:${port}/fbx.html`);
  await page.waitForFunction(() => window.ready === true, null, { timeout: 60000 });
  const info = await page.evaluate(([f, s, t]) => window.loadModel(f, s, t), [file, skin, tint]);
  console.log('model', JSON.stringify(info));

  const save = (name, url) => fs.writeFileSync(path.join(outDir, name), Buffer.from(url.split(',')[1], 'base64'));

  if (mode === 'sheet') {
    // Several moments of every clip, front view, as separate tiles
    let n = 0;
    for (let c = 0; c < Math.max(1, info.clips.length); c++) {
      const duration = info.clips.length ? info.clips[c][1] : 0;
      const steps = duration > 0 ? 8 : 1;
      for (let i = 0; i < steps; i++) {
        const time = duration * i / steps;
        const r = await page.evaluate(o => window.renderFrame(o), { clip: c, time, w: 240, h: 320, look: [0, 0.5], dist: 2.4, fov: 30 });
        save(`sheet-${file.replace('.fbx', '')}-${c}-${i}.png`, r.url);
        n++;
      }
    }
    console.log('tiles', n);
  }
  else {
    for (const shot of JSON.parse(shotsJson)) {
      const r = await page.evaluate(o => window.renderFrame(o), shot);
      save(shot.name, r.url);
      const want = shot.points || [];
      const picked = want === 'all' ? r.points : Object.fromEntries(Object.entries(r.points).filter(([k]) => want.some(w => k.toLowerCase().includes(w))));
      console.log('saved', shot.name, JSON.stringify(picked));
    }
  }
  await browser.close();
})();
