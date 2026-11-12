# Grin's voice and sounds

Every sound in Grin's scare and fight is a real recording, all CC0 (public
domain) from OpenGameArt. Credit isn't required but is given anyway:

| Used for | Source | Author |
|---|---|---|
| Snigger in the dark, roar layer, "Nowhere to run", "Game over", laugh, grunts, snarl, dying, chat voice | [demon voice](https://opengameart.org/content/demon-voice) | Tim Rockk |
| Roar (main layer) | [CC0 Deep Monster Roar](https://opengameart.org/content/cc0-deep-monster-roar) | trazzz123 |
| Roar (shriek layer) | [Horror scream1](https://opengameart.org/content/horror-scream1) | Vinrax |
| Glass crack and shatter | [Glass Break](https://opengameart.org/content/glass-break) | Till Behrend |
| Shatter, shots, hits | [75 CC0 breaking / falling / hit sfx](https://opengameart.org/content/75-cc0-breaking-falling-hit-sfx) | rubberduck |

To rebuild them, download `monster_roar.wav`, `scream_horror1_0.mp3`,
`glass_breaking.wav`, `all_demon_lord_audio_files_0.zip` and
`sfx_breaking_and_falling.zip` from those pages into one folder, unzip each
zip into `x-<zip name without .zip>/`, then:

```
./make_grin_voice.sh <that folder> ../../mod/resources/sounds
```

It trims, layers (the roar is three recordings mixed and compressed so it hits
at once), normalizes and encodes them as mono Ogg Vorbis with ffmpeg.
