# Grin's 3D renders

`scene.html` builds Grin (an original creature) and the fight arena with three.js;
`render.js` renders them in headless Chromium into the images under
`mod/resources/grin/`.

```
npm install three@0.170.0
python3 -m http.server 8765 &
node render.js 8765 ../../mod/resources/grin
```
