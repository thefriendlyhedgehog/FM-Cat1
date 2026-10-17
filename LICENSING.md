# Cat Piano licensing

Cat Piano is a derivative of Fart Machine One (adamcharleswilsonmusic-code on GitHub), itself a derivative of FoMni,
licensed GPL-3.0-only. The pitched playback, per-key tuning, manifest importer, pitch tool, cat artwork
and tests added for Cat Piano use the same licence. Distribute complete corresponding source alongside
any firmware distribution.

## Audio

The firmware's meows are five CC0 1.0 recordings from OpenGameArt: Kerzoven ("Cat Purr & Meow",
https://opengameart.org/content/cat-purr-meow), IgnasD ("Meow", https://opengameart.org/content/meow) and
AntumDeluge ("Kitten Mew", https://opengameart.org/content/kitten-mew). The source files are in
`samples/cc0-meows/src/`; crops, measured pitches and sources are in `samples/cc0-meows/manifest.json`
and travel into `firmware/src/dsp/sample_bank.json`. `Meow.wav` is `Meow.ogg` converted with ffmpeg.
The synthetic test meows made by `tools/import_samples.py --placeholder` are dedicated to the public
domain under CC0 1.0. Fart Machine One's recordings are not part of Cat Piano.

## Artwork

The cat and its notes were drawn from scratch for Cat Piano; see `docs/ARTWORK-PROVENANCE.md`.

# Fart Machine One licensing

Fart Machine One is a derivative of FoMni, licensed GPL-3.0-only. Its playback engine, importer, UI and
tests are the basis of Cat Piano's. The original FoMni plate reverb is retained in
`firmware/src/dsp/plate.h`. Original platform, loader, USB, audio and hardware copyright notices remain
in place.

The official AC79 SDK files packaged into the firmware are Apache-2.0; its licence
is included in `LICENSES/AC79-SDK-LICENSE.txt`. Upstream notices follow.

# FoMni licensing

FoMni is free software under the GNU General Public License, version 3 only
(`GPL-3.0-only`, full text in `LICENSE`). It is built on Felucca's platform layer (through X0X)
and keeps Felucca's licence. If you distribute FoMni, or firmware derived from it, you must give
your recipients its complete corresponding source under the same licence.

## Where the code comes from

| What | Origin | Licence |
| --- | --- | --- |
| Platform: `firmware/hal/`, `firmware/loader/`, `firmware/src/{libc,lcd,gfx,usb,storage,ota,midi_uart}.c`, `tools/` (build, package, install, rescue), `web/fm1*.js` | [Felucca](https://github.com/hugelton/Felucca), Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments, as changed for [X0X](https://github.com/charlesvestal/fm1-x0x) and FoMni | GPL-3.0-only |
| `firmware/src/app/{panel,plat_fm1,main_fm1}.c`, `host/omni_host.c`, `web/emu/` | adapted from X0X (itself from Felucca's `panel.c`, `main.c`, `audio.c`) | GPL-3.0-only |
| Rhythm patterns, single-cycle waves, drum sounds, envelope shapes and the strum plate's voicing (`firmware/src/dsp/om_data.h`, `om_drums.h`, made by `tools/import_chordian.py`) | [Chordian](https://github.com/Jan125/pb.chordian) by Jan125, an OM-84 emulator | CC0 1.0 (public domain) |
| The idea of a browser emulator for FM-1 firmware (`web/emu/`) | [Groove OS](https://www.groove-os.com/) by Peter Gombos, whose site runs its firmware in the browser; no code from it | credit only |
| Everything else in `firmware/src/{app,dsp}`, `tests/` | FoMni | GPL-3.0-only |

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| Barlow Semi Condensed (The Barlow Project Authors), the UI face | SIL OFL 1.1 | `assets/fonts/BarlowSemiCondensed-*.ttf`, `assets/fonts/Barlow-OFL.txt` |
| Terminus (Dimitar Toshkov Zhekov), an alternative font set | SIL OFL 1.1 | `assets/fonts/ter-u*.bdf`, `assets/fonts/Terminus-LICENSE.txt` |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` are read from your SDK checkout at build time and placed in the package; no SDK files are in this tree | Apache-2.0 | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |

## Trademarks

Omnichord is a trademark of Suzuki Musical Instrument Corporation, used here only to say what
inspired FoMni. "Felucca" and "Hügelton Instruments" are names of Hügelton Instruments.
"M-VAVE" and "FM-1" are trademarks of their respective owners. FoMni is independent firmware; it is
not affiliated with, endorsed by or supported by any of them.

## Radio

FoMni never enables the Bluetooth / Wi-Fi radio of the hardware.
