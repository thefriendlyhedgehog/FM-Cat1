# Artwork provenance

The sitting tabby and its floating notes were drawn from scratch for Cat Piano on 2026-10-16, replacing
Fart Machine One's cartoon man and puff. They are procedural drawings in `firmware/src/app/cartoon.h`:
hand-chosen ellipse, triangle, rectangle and line coordinates and colours, rendered by the firmware's
canvas. No image, sprite sheet, icon, photograph, traced artwork or downloaded illustration was used, and
no existing character was used as a reference. The code was written with an AI coding assistant (Claude).

- The drawing calls use `cv_rect`, `cv_line` and the integer ellipse and triangle helpers in `cartoon.h`.
  There are no image-file reads, embedded raster images or external sprite data.
- `ui.c` calls those drawings directly; each key press starts a note, and the cat meows while a note is
  fresh.
- `docs/images/screen-playing.png` is a render of this code by the host simulator.

The artwork code is GPL-3.0-only with the rest of the firmware. This records how it was made; it is not a
claim that no independently made cartoon cat could look similar.

## Inherited material

The hardware platform, drawing primitives, loader and plate reverb come from Fart Machine One, FoMni and
Felucca and keep their notices under GPL-3.0-only. The screen font is Terminus, by Dimitar Toshkov
Zhekov, under SIL OFL 1.1 (`assets/fonts/Terminus-LICENSE.txt`). The browser's FM-1 panel illustration is
inherited from FoMni.
