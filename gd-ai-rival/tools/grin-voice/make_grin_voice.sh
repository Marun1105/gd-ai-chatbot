#!/bin/sh
# Builds Grin's sounds from real CC0 recordings (see README.md for sources).
# Usage: make_grin_voice.sh <downloads dir> <output dir>
set -e
SRC="$1"; OUT="$2"
DL="$SRC/x-all_demon_lord_audio_files_0"
BF="$SRC/x-sfx_breaking_and_falling"
mkdir -p "$OUT"

# Trim leading silence so every sound hits the instant it's played, then
# mono 44.1 kHz Vorbis
FINISH="silenceremove=start_periods=1:start_threshold=-50dB,aformat=sample_rates=44100:channel_layouts=mono"
# Loud: squeeze the dynamics and push everything up to just under clipping,
# so it cuts through the level music and phone speakers
LOUD="acompressor=threshold=0.06:ratio=6:attack=2:release=90:makeup=6,alimiter=limit=0.97:level=false"
enc() { ffmpeg -v error -y "$@" -c:a libvorbis -q:a 4; }

# The roar: a deep monster roar, a demon growl under it and a thin horror
# scream on top for the shriek, squeezed together so it hits hard at once
enc -i "$SRC/monster_roar.wav" -i "$SRC/scream_horror1_0.mp3" -i "$DL/DL DEMON GROWL 4.wav" -filter_complex "\
[0]atrim=0:2.6,asetpts=PTS-STARTPTS,afade=t=out:st=1.9:d=0.7[a];\
[1]atrim=1.2:3.8,asetpts=PTS-STARTPTS,highpass=f=500,volume=0.35,afade=t=in:d=0.04,afade=t=out:st=1.8:d=0.8[b];\
[2]atrim=0:2.0,asetpts=PTS-STARTPTS,volume=0.8,afade=t=out:st=1.4:d=0.6[c];\
[a][b][c]amix=inputs=3:normalize=0,acompressor=threshold=0.12:ratio=6:attack=3:release=150:makeup=4,alimiter=limit=0.97:level=false,aformat=sample_rates=44100:channel_layouts=mono" "$OUT/roar.ogg"

# In the dark, before it shows itself
enc -i "$DL/DL DEMON SNIGGER 2.wav" -af "$FINISH,loudnorm=I=-15:TP=-1" "$OUT/grin-snigger.ogg"

# Glass: the first hit cracks it, then it bursts
enc -i "$SRC/glass_breaking.wav" -af "atrim=0:0.5,afade=t=out:st=0.35:d=0.15,$FINISH,$LOUD" "$OUT/crack.ogg"
enc -i "$BF/bfh1_glass_breaking_03.ogg" -i "$SRC/glass_breaking.wav" -filter_complex "\
[1]atrim=0.15:1.3,asetpts=PTS-STARTPTS,volume=0.7[t];[0][t]amix=inputs=2:normalize=0,$LOUD,aformat=sample_rates=44100:channel_layouts=mono" "$OUT/shatter.ogg"

# The fight: your shots and where they land
enc -i "$BF/bfh1_hit_05.ogg" -af "$FINISH,atrim=0:0.18,afade=t=out:st=0.1:d=0.08,highpass=f=120,$LOUD" "$OUT/shot.ogg"
enc -i "$BF/bfh1_hit_01.ogg" -af "atrim=0:0.4,afade=t=out:st=0.25:d=0.15,$FINISH,$LOUD" "$OUT/hit.ogg"

# Grin talking
enc -i "$DL/DL NOWHERE TO RUN.wav" -af "$FINISH,$LOUD" "$OUT/grin-nowhere.ogg"
enc -i "$DL/DL GAME OVER.wav" -af "$FINISH,$LOUD" "$OUT/grin-gameover.ogg"
enc -i "$DL/DL DEMON LAUGHS.wav" -af "$FINISH,atrim=0:4.2,afade=t=out:st=3.4:d=0.8,$LOUD" "$OUT/grin-laugh.ogg"
enc -i "$DL/DL DEMON FIGHT GRUNT 1.wav" -af "$FINISH,$LOUD" "$OUT/grin-hurt1.ogg"
enc -i "$DL/DL DEMON FIGHT GRUNT 2.wav" -af "$FINISH,$LOUD" "$OUT/grin-hurt2.ogg"
enc -i "$DL/DL DEMON SNARL 2.wav" -af "$FINISH,$LOUD" "$OUT/grin-snarl.ogg"
enc -i "$DL/DL DEMON DIES.wav" -af "$FINISH,atrim=0:2.6,afade=t=out:st=1.9:d=0.7,$LOUD" "$OUT/grin-dies.ogg"
# One short sound each time it speaks in chat, instead of typing blips
enc -i "$DL/DL DEMON SNIGGER 1.wav" -af "$FINISH,loudnorm=I=-14:TP=-1" "$OUT/grin-talk-calm.ogg"
enc -i "$DL/DL DEMON GROWL 2.wav" -af "$FINISH,loudnorm=I=-13:TP=-1" "$OUT/grin-talk-angry.ogg"
