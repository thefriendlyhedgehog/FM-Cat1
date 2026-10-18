# Cat Piano licensing

Cat Piano is a derivative of Fart Machine One (adamcharleswilsonmusic-code on GitHub), itself a derivative of FoMni,
licensed GPL-3.0-only. The banks, ADPCM codec, renderer, importer, pitch tool, cat artwork
and tests added for Cat Piano use the same licence. Distribute complete corresponding source alongside
any firmware distribution.

## Audio

The firmware's meows are cut from these recordings (crops, measured pitches and sources are in
`samples/banks.json`; the banks rendered from them are in `samples/rendered/`, and the list of those used travels
into `firmware/src/dsp/sample_bank.json`). The rendered takes are pitch-shifted, trimmed and level-matched.

| Recording | Author | Licence |
| --- | --- | --- |
| ["Cat Purr & Meow"](https://opengameart.org/content/cat-purr-meow) | Kerzoven | CC0 1.0 |
| ["Meow"](https://opengameart.org/content/meow) | IgnasD | CC0 1.0 |
| ["Kitten Mew"](https://opengameart.org/content/kitten-mew) | AntumDeluge | CC0 1.0 |
| [A cat's meow (2015-11-24)](https://commons.wikimedia.org/wiki/File:2015-11-24.%CE%BD%CE%B9%CE%B1%CE%BF%CF%8D%CF%81%CE%B9%CF%83%CE%BC%CE%B1.%CE%9D%CE%B9%CE%AC%CE%BF%CF%85.noise_reduced.flac) | Tsester | CC0 1.0 |
| [Meow of a Siamese cat](https://commons.wikimedia.org/wiki/File:Meow_of_a_Siamese_cat_-_freemaster2.wav) | freemaster2 | CC0 1.0 |
| [Getting Out Impatient](https://commons.wikimedia.org/wiki/File:GettingOutImpatient.ogg), [No Trip](https://commons.wikimedia.org/wiki/File:NoTrip.ogg), [Meow of a pleading cat](https://commons.wikimedia.org/wiki/File:Meow_of_a_pleading_cat.oga) | Heismark | Public domain |
| [Weibliche Britisch Kurzhaar will Futter (C1277)](https://commons.wikimedia.org/wiki/File:Weibliche_Britisch_Kurzhaar_will_Futter_C1277_MIAUEN.wav) | PantheraLeo1359531 | CC BY 4.0 |

**Attribution (CC BY 4.0):** "Weibliche Britisch Kurzhaar will Futter C1277 MIAUEN" by PantheraLeo1359531,
https://commons.wikimedia.org/wiki/File:Weibliche_Britisch_Kurzhaar_will_Futter_C1277_MIAUEN.wav, licensed
CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/); cut, pitch-shifted and re-encoded for Cat Piano.
Keep this notice with any copy of the firmware or its sample files.

Source files are in `samples/cc0-meows/` and `samples/commons-meows/` (`sources.json` there records each
Commons file's licence as the Commons API reported it). Some downloaded recordings are not used by any bank.
The synthetic test meows made by `tools/import_samples.py --placeholder` are dedicated to the public domain under
CC0 1.0. Fart Machine One's recordings are not part of Cat Piano.

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
| `firmware/src/app/{panel,plat_fm1,main_fm1}.c`, `host/omni_host.c` | adapted from X0X (itself from Felucca's `panel.c`, `main.c`, `audio.c`) | GPL-3.0-only |
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

