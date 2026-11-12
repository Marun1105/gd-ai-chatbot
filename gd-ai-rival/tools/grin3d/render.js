// Renders the fight arena (grin-arena.png) with three.js in headless Chromium.
// Usage: node render.js <port> <outDir>
const { chromium } = require('playwright');
const fs = require('fs');
const path = require('path');

(async () => {
  const [port, outDir] = process.argv.slice(2);
  const browser = await chromium.launch({ args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] });
  const page = await browser.newPage();
  page.on('console', m => console.log('page:', m.text()));
  page.on('pageerror', e => console.log('page error:', e.message));
  await page.goto(`http://127.0.0.1:${port}/scene.html`);
  await page.waitForFunction(() => window.ready === true, null, { timeout: 30000 });

  const save = (name, dataUrl) => {
    const data = Buffer.from(dataUrl.split(',')[1], 'base64');
    fs.writeFileSync(path.join(outDir, name), data);
    console.log(name, data.length, 'bytes');
  };
  save('grin-arena.png', await page.evaluate(() => window.renderArena(1280, 720)));
  await browser.close();
})();
