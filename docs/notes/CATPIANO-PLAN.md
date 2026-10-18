> Development notes, kept as written at the time (2026-10). Paths like `catpiano/` and `../*.mp3` refer to the
> original workspace: `catpiano/` is this repository, and the demos are in `docs/demos/`.

# Cat piano plan (fork: `catpiano/`, from Fart Machine One)

Written 2026-10-16 after reading `firmware/src/dsp/omni.c`, `tools/import_samples.py` and the tests.

**Status (2026-10-16): M1, real CC0 meows and M4 committed on branch `catpiano` in `catpiano/` (e750b8a, then the rebrand). Next: M5 first install, which needs your go-ahead.** M1 details: Q16 pitched voices with linear interpolation; `fmo_keys[27]` table (sample, note, increment, label); sequencer slot = key; `import_samples.py --placeholder` synthesizes 5 tuned meows (TOMCAT A4, TABBY D5, CALICO G5, SIAMESE C6, KITTEN F#6; 99 KB, CC0) and is now the firmware bank. New `tests/fmo/pitch_test.c` measures every key within 5 cents (worst 0.05; a 10-cent detune fails it). `names_test` now runs in the suite. Demo render: `tests/fmo/demo.omni` -> `../catpiano-demo.wav`. Device build OK (image 174 KB); package identity still FM-1_701. Not flashed.

**Real CC0 meows in the firmware bank (2026-10-16, committed).** Five sounds from OpenGameArt (licence pages checked: CC0), with sources in `catpiano/samples/cc0-meows/src/` and crops, roots and provenance in `samples/cc0-meows/manifest.json`:

| Label | Source | Root | Keys |
| --- | --- | --- | --- |
| PURRY | Kerzoven, cat_mewpurr2.wav 0.20-1.25 s | 445 Hz | G4-C5 |
| MEOW | IgnasD, Meow.ogg | 617 Hz | C#5-E5 |
| SOFTY | Kerzoven, cat_softmew.wav 0.33-1.05 s | 729 Hz | F5-G#5 |
| SQUEAK | Kerzoven, cat_softmew.wav 1.32-1.78 s | 955 Hz | A5-B5 |
| KITTEN | AntumDeluge, kitten_mew-1.wav | 1066 Hz | C6-A6 (up to 1.65x, chipmunky at the top) |

New: `import_samples.py --manifest` (crop, peak-normalize, root -> per-key tuning, licence carried into `sample_bank.json`), and `tools/measure_pitch.py` (YIN median F0, needs numpy; `--write` stores `root_hz`). Bank 163 KB, image 238 KB. Every rendered key's median F0 is within 2 cents of its note. The test suite still uses the synthetic bank for exact pitch checks. Demo: `../catpiano-demo-cc0.wav`. Skipped: cat_mewfood/cat_mewpurr (breathy, duplicate range) and BigSoundBank s0098 (no direct link). **Gap: nothing above 1066 Hz.** A real high kitten mew around 1400-1700 Hz would fix the top nine keys.

## What exists

- `omni.c` plays 27 fixed one-shot samples (`fmo_map[27]` points keys at 11 sounds, mono PCM16 at 22.05 kHz). A voice is a frame counter, and it upsamples to 44.1 kHz by repeating each frame with a 2-tap average. 8 voices, oldest replaced.
- The audio code already uses single-precision `float` and ships on hardware, so a float phase accumulator for pitch is fine. The build check says "single-precision FPU only": doubles are not allowed.
- The sample bank is a generated header (`sample_bank.h` + `.json` manifest with a licence field per sample). The importer is pure Python and enforces a 320 KiB budget.
- The sequencer stores one bit per *distinct sound* (`canonical` lookup in `seq_record_hit`). With 27 different pitches every key must be its own step slot.
- Only four files touch the bank: `omni.c`, `import_samples.py`, `ui.c` (label) and the tests (`tests/fmo/*`, using a generated fixture header).

## Design decisions

1. **Pitching.** Each voice gets a float read position and increment: `inc = 2^((target_note - root_note)/12) * (22050/44100)`. Read with linear interpolation first; step up to 4-point Hermite if highs sound harsh. The ratios are computed by the importer and written into the header as constants, so the device never calls `pow`.
2. **Multisampling instead of one stretched meow.** 4-6 meows with different natural pitches, each covering a ±3 semitone zone (ratio about 0.84-1.19). One meow across 27 semitones would be chipmunky at the top and a growl at the bottom.
3. **Key range.** Cats meow at roughly 400-1000 Hz (about G4-B5), so mapping the keys to their stock MIDI notes 53-79 (F3-G5) would push every sample two octaves down. Default: key 1 = G4, so the 27 keys cover G4-A6, shifted by a constant that the manifest can change. Final range decided after we measure your recordings.
4. **Root pitch.** The importer detects F0 of the sustained part of each meow (a YIN-style detector; needs `numpy` in the build venv) and records it in the manifest. You can override it by hand, because meows glide.
5. **Sample quality.** Importer additions: trim silence, band-limit above about 9 kHz before the 22.05 kHz resample (stops alias when pitching up), match loudness across samples, short fade-out, per-sample licence/source fields kept as now.
6. **Memory.** Stock bank is 278 KB inside a 353 KB image. The app cap is about 581 KB, so up to roughly 500 KB of PCM would fit (about 11 s at 22.05 kHz). IMA ADPCM (4:1, Felucca has a decoder) is the fallback if we want more.
7. **Sequencer.** Remove the `canonical` dedupe: step slot = key. Each key keeps its own velocity.
8. **Identity.** Change package identity from `FM-1_701` (Fart Machine One's) and the name, so installers can tell the builds apart. Follow Dinghy's `docs/MAKE-IT-YOURS.md` for the identity rules.

## Milestones

| # | Work | Done when |
| --- | --- | --- |
| M1 | Pitched voice in `omni.c`, key-to-(sample, ratio) table, sequencer slot = key. Placeholder bank from a script that synthesizes formant "meows" | Host tests pass. A new test measures output frequency per key within 5 cents of target |
| M2 | `tools/import_meows.py`: manifest in (file, root override, key zone, label, licence, source), header out. Pitch detection, trim, band-limit, loudness match, budget check | `import_test.py` extended; WAV render of all 27 keys sounds in tune |
| M3 | Real meows (yours, or CC0 from the list below), provenance filled in | Manifest has no "TO PROVIDE" licence entries |
| M4 | UI labels (note name + cat name), cat artwork replacing the puff cartoon, new package identity, `LICENSING.md` update | `names_test` and cartoon tests updated and passing |
| M5 | First install on the FM-1 | Plays in tune, no dropouts with 8 voices, CPU measured on the device with `tools/fm1_debug.py` |
| M6 (optional) | ADPCM bank, velocity-selected variants, pitch bend on an encoder, sustain/loop for held keys | Your call |

Before M5: confirm "Return to official V15" works from the web installer, and know the rescue path (hold OCT- at power-on). I will not flash without your go-ahead.

## Recordings I need from you (for M3)

You do not need them for M1 and M2.

- **6-10 single meows**, one per file, WAV (44.1 or 48 kHz, 16 or 24 bit, mono preferred; a phone's voice-memo app is fine, convert to WAV).
- **Pitch spread matters most.** At least three clearly different pitch levels: a low/adult meow, a mid one, and a high kitten-style mew. Two or more different cats is better still.
- 0.3-1.2 s each, clean start and end, close to the microphone (20-30 cm), quiet room. No purring, hissing, music or talking in the file.
- Nice to have: one low yowl or growl for the bottom keys, one trill/chirp, and one long drawn-out "mrrrow" (for a sustained-key variant in M6).
- Tell me the licence you want on them. Default: you own them and they go into the project as CC0, so the fork's source can stay fully redistributable under GPL-3.0.
- Budget: about 7-11 s of audio total after trimming.

## Placeholders until then

1. **Synthetic meows (no download, no licence issue).** A script in `tools/` generates formant-swept sounds of my own, which I would dedicate to CC0. Used for M1 and M2.
2. **CC0 candidates for M3 if you have no recordings yet.** From a search, not yet checked on the pages themselves. Each needs your OK before I download it, and its licence confirmed on the page:
   - OpenGameArt, ["Cat Purr & Meow"](https://opengameart.org/content/cat-purr-meow) by Kerzoven, said to be CC0 and recorded by the author, several WAVs.
   - OpenGameArt, ["Meow"](https://opengameart.org/content/meow) by IgnasD, about 10 KB, CC0.
   - OpenGameArt, ["Kitten Mew"](https://opengameart.org/content/kitten-mew), CC0, OGG and WAV.
   - BigSoundBank, ["Small mewing of a cat" s0098](https://bigsoundbank.com/small-mewing-of-a-cat-s0098.html), about 4 s, listed CC0.

## Risks

- Float timing on the device is untested for 8 interpolating voices. Stock Fart Machine One already runs float, so this is probably fine, but M5 measures it.
- Pitch-shifted meows can sound artificial at the edges of each zone. More source meows is the cure.
- Meow glides make "in tune" fuzzy. The root is the sustained part's F0, and we can tune by ear.
- Fart Machine One is two weeks old and AI-assisted. Keep its test suite green at each step.

## M4 done (2026-10-16)

Identity **FM-1_963** (FM-1_962-969 unused in thefirmwarehub.com/catalog.json), package `cat-piano.fwsc`, CAT PIANO header and MEOW splash, a procedurally drawn tabby that meows notes, MIDI in on notes 67-93. Fart Machine One's recordings removed. README, INSTALL, BUILDING, LICENSING and the artwork provenance rewritten. All host tests and scenarios pass; the OTA test runs against the FM-1_963 package. Not done: `tools/make_pages.py` (FoMni's GitHub Pages builder, still FoMni-specific and unused) and `web/omni_installer.html`.

## Banks, natural pitching, sustain, bend (2026-10-16, after the first install)

Your feedback on the installed M4 build ("needs more sounds and could be a lot better") led to all four upgrades:

- **More cats.** 17 recordings from OpenGameArt and Wikimedia Commons (CC0, public domain, one CC BY 4.0) in `samples/banks.json`, with hand-checked roots where the pitch detector was unsure.
- **Natural pitching.** `tools/render_banks.py` pre-renders an anchor every 3 semitones with Rubber Band (`-3 -F`, formants and duration kept). Each key resamples at most a semitone from its anchor, so no more chipmunks.
- **Sound banks.** MEOW (G4-A6, two takes per note in turn), KITTENS (C5-D7), TOMCATS (G3-A5) and HUNGRY (D4-E6). PRESET switches between them and the screen names the bank.
- **Playing feel.** A held key loops a crossfaded sustain region and finishes its take when you let go. ALGO bends ±2 semitones and springs back. MIDI note-off and pitch bend work.
- **Codec.** 2nd-order ADPCM with 16 predictors per take, 14 samples in 8 bytes. It replaced IMA ADPCM, which measured 19.5-37 dB SNR on the cats; the new codec measures 26.5-58 dB (median 39.7). 4 banks, 43 takes, 33.5 s in 421,808 bytes; app image 502,952 of 581,564 B.
- **Tests.** pitch_test (every key within 5 cents, sustain seams, round robin, bend), dsp_test (taps vs. holds), import_test (codec round trip and mid-take decode, loops, dedupe, budget), names_test (every bank), and the new host scenario `tests/fmo/controls.omni` (bank knob, bend, spring-back, MIDI note-off and pitch bend). Demo: `../catpiano-demo-v2.mp3`.
- **Done:** committed as 37522de and installed on the FM-1 (2026-10-16).
