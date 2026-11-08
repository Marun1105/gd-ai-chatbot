# Grin's pictures

Grin's look is the **"3D Horror Game Monster" by HorrorGameMaker.com**,
released as CC0 (public domain) on OpenGameArt:
https://opengameart.org/content/3d-horror-game-monster. Credit isn't
required, but it's given in the mod's about page anyway.

The model isn't stored here (about 120 MB). To re-render, download
`Poses.zip` and `Colors.zip` from that page, put the `.fbx` files in `oga/`
and the black skin's three textures in `oga/tex-black/`, then:

```
npm install three@0.170.0 playwright
python3 -m http.server 8765 --bind 127.0.0.1 &
./render-grin.sh            # face, roar, full body and icon
node render.js 8765 ../../mod/resources/grin   # the fight arena
```

- `fbx.html` loads the model with three.js, recolors the skin to charcoal,
  opens the jaw bone for the roar and lights it with red rim lights.
- `render-fbx.js` drives it in headless Chromium. `sheet` mode renders every
  animation as tiles for picking poses; `shots` mode renders exact frames and
  prints where bones land on the picture.
- `finish_grin.py` softens the edges of the face and roar, crops the body and
  makes the small glowing icon for the anger bar and chat.
- `scene.html` + `render.js` render the fight arena.

The hit areas in `RageFight.cpp` (head and torso, as fractions of
`grin-full.png`) come from the bone positions `render-grin.sh` prints.
