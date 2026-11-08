# Grin's 3D renders

Grin is an original creature, sculpted with signed distance fields and
ray-marched in a shader (`sdf.html`), rendered in headless Chromium by
`render-sdf.js` into `mod/resources/grin/grin-face.png`, `grin-roar.png`
and `grin-full.png`. `scene.html` (three.js) renders the fight arena,
`grin-arena.jpg`.

```
npm install three@0.170.0 playwright
python3 -m http.server 8765 &
node render-sdf.js 8765 ../../mod/resources/grin
node render.js 8765 ../../mod/resources/grin     # arena (and the old model)
```

`render-sdf.js` also prints where the head and body land in the full-body
picture; `RageFight.cpp` uses those numbers for its hit test.
