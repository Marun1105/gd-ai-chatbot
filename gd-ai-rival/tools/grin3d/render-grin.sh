#!/bin/sh
# Renders Grin's pictures into ../../mod/resources/grin (see README.md).
# Needs the http server from the README running on port 8765.
set -e
cd "$(dirname "$0")"
mkdir -p raw
# Face and roar: the same pose and camera, so the scare can flip between them
# while it roars; the roar opens the jaw bone and turns the red light up
node render-fbx.js 8765 raw shots Poses.fbx black '[
{"name":"face.png","clip":0,"time":0.1458,"w":640,"h":640,"focus":63,"look":[0,0.495],"dist":0.41,"fov":30,"red":0.6,"pitch":-0.05},
{"name":"roar.png","clip":0,"time":0.1458,"w":640,"h":640,"focus":63,"look":[0,0.495],"dist":0.41,"fov":30,"red":1.0,"exposure":1.12,"pitch":-0.05,"jaw":0.65}
]' charcoal
# Full body for the fight; prints where every bone lands for the hit test
node render-fbx.js 8765 raw shots Poses.fbx black '[
{"name":"full.png","clip":0,"time":0.2187,"w":600,"h":800,"look":[0,0.5],"dist":2.6,"fov":30,"red":1.0,"exposure":1.15,"pitch":-0.08,"points":["#63","#67","#11","#10"]}
]' charcoal
python3 finish_grin.py raw ../../mod/resources/grin
