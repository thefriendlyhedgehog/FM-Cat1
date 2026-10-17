# Cat Piano

A cat piano for the M-VAVE FM-1: each of the 27 keys plays a real cat's meow, tuned to its note from G4 to A6.
There is also a 16-step loop, reverb and delay. Forked from
[Fart Machine One](https://github.com/adamcharleswilsonmusic-code/Fart-Machine-One), which is built on FoMni,
X0X and Felucca.

![Cat Piano screen](docs/images/screen-playing.png)

> **Status:** builds and passes its host tests. It has **not been installed on an FM-1 yet**.

## Controls

| Control | Behaviour |
| --- | --- |
| 27 keys | Play a meow at that key's note, G4 (lowest key) to A6. Up to eight overlap; the oldest is cut first. |
| PLAY | Start the loop from step 1, or stop playback and recording and silence everything. |
| REC (tap) | Start recording from step 1 if stopped; during playback, overdub on or off. |
| REC (hold 1 s) | Clear the loop. |
| SELECT | Tempo, 40-240 BPM. |
| KNOB 1 / 2 / 3 / 4 | Reverb, delay level, delay time (80-500 ms), delay repeats (0-70%). |
| MASTER | Volume. |

One bar of 16 sixteenths, recorded to the nearest step. Each key has its own slot, so a loop remembers pitches.
The loop is kept in RAM only: switching off clears it.

MIDI in, channel 1: notes 67-93 (G4-A6) play the matching key with velocity. Note-off does not cut a meow short.
CC 120 and 123 stop the loop and silence the sound.

## The cats

Five CC0 recordings from OpenGameArt, each pitched across a few keys
(`samples/cc0-meows/manifest.json` has the crops, measured pitches and sources):

| Sound | Recording | Root | Keys |
| --- | --- | --- | --- |
| PURRY | [Kerzoven, "Cat Purr & Meow"](https://opengameart.org/content/cat-purr-meow) (cat_mewpurr2) | 445 Hz | G4-C5 |
| MEOW | [IgnasD, "Meow"](https://opengameart.org/content/meow) | 617 Hz | C#5-E5 |
| SOFTY | Kerzoven (cat_softmew, first mew) | 729 Hz | F5-G#5 |
| SQUEAK | Kerzoven (cat_softmew, second mew) | 955 Hz | A5-B5 |
| KITTEN | [AntumDeluge, "Kitten Mew"](https://opengameart.org/content/kitten-mew) | 1066 Hz | C6-A6 |

Each key takes the nearest recording and resamples it to the note. The top keys stretch the kitten up to 1.65x,
so a higher real mew would make them more natural.

## Install

```sh
python3 -m venv .venv && . .venv/bin/activate
python -m pip install -r requirements-install.txt
python tools/fm1_install.py --info                 # finds the FM-1, changes nothing
python tools/fm1_install.py build/cat-piano.fwsc   # asks before it writes
```

The package identity is **FM-1_963**. To go back, use the [FM-1 Firmware Hub](https://thefirmwarehub.com) or
M-VAVE's updater. Holding OCT- while switching on starts the USB rescue. See [INSTALL.md](INSTALL.md).
Installing firmware is at your own risk.

## Build and test

See [BUILDING.md](BUILDING.md): the JieLi toolchain runs in a `linux/amd64` container on macOS.

```sh
sh tests/run_tests.sh      # host tests: pitch of every key, DSP, sequencer, UI, importer, storage, USB
./build.sh                 # -> build/cat-piano.fwsc
```

`tests/fmo/demo.omni` renders a listening demo with the host simulator (`sh host/build_host.sh`, then
`build/host/omni_host tests/fmo/demo.omni build/demo`).

## Your own meows

1. Put WAVs (16-bit PCM) next to a manifest like `samples/cc0-meows/manifest.json`: per sound a `file`, an
   optional `start`/`end` in seconds, a `label`, and `license`, `source` and `author`.
2. `python3 tools/measure_pitch.py path/to/manifest.json --write` measures each meow's pitch (needs numpy) and
   stores it as `root_hz`. Check the result by ear; meows glide.
3. `python3 tools/import_samples.py --manifest path/to/manifest.json` writes the bank, then `./build.sh`.

The importer refuses a bank that would pitch any key more than an octave up. About 7.4 s of audio fits.

## Licence

GPL-3.0-only, as the firmwares it comes from: give the source to anyone you give the firmware to. The meow
recordings are CC0 1.0. Credits and third-party notices are in [LICENSING.md](LICENSING.md). M-VAVE and FM-1 are
trademarks of their owners; this project is not affiliated with them.
