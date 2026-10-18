> Development notes, kept as written at the time (2026-10). Paths like `catpiano/` and `../*.mp3` refer to the
> original workspace: `catpiano/` is this repository, and the demos are in `docs/demos/`.

# FM-1 firmware review for a "cat piano" (2026-10-16)

Goal: custom firmware for the M-VAVE FM-1 where each of the 27 keys plays a meow tuned to its note.
Everything below was checked on 2026-10-16 by cloning the repos and reading their docs and code, except where marked **unverified**.

## The hardware budget (what any fork has to live in)

| | |
| --- | --- |
| SoC | JieLi AC791N (pi32v2), no 64-bit divide, no libgcc. Float: Dinghy's docs say "no float", but Fart Machine One's build check reports "single-precision FPU only" (no doubles), which would explain Melodee's fp32 code. Treat float as unconfirmed |
| Flash | 1 MiB total. App code and `const` data run in place (XIP) in about **568 KB**. Update staging sits at 0xE0000 and settings at 0xFC000. The other firmwares keep user data in 0x97000..0xFBFFF |
| RAM | 96 KB for `.data`/`.bss`, a **336 KB pool** for big buffers, 24 KB stack |
| Audio | 256-frame blocks, **5.8 ms budget** per block in the audio interrupt, 44.1/48 kHz stereo out |
| Panel | 27 keys (MIDI notes 53-79), 14 buttons, 7 encoders, MASTER knob, 240x240 screen |
| Install | Browser WebMIDI installer (Chrome/Edge) or `tools/fm1_install.py`. Back to stock V15 is always available |

Sources: Dinghy `docs/API.md` and `docs/RULES.md`, Felucca `BUILDING.md`, Fart Machine One `firmware/app.ld`.

## The ecosystem

Everything is new: the family starts with Felucca (created 2026-10-02) and the awesome-fm-1 list now has about 100 entries. Almost all are GPL-3.0-only forks of one platform layer written by Leo Kuroshita (Hügelton Instruments).

```
Felucca (hugelton)  ->  SLOOP (isod89)  ->  Dinghy, sloopDX, ORBIT, JIANT, winampFM, ...
       |-> X0X (charlesvestal) -> FoMni -> Fart Machine One, FiMba-1, FuMi-1, CoralFlower
       |-> Melodee -> Melodee (Tess), AMBII, FM-106's DSP
       |-> DUELL, Jangada, Skiff, Rainbow mode, ...
```

## Candidates, scored for this project

Stars/forks and dates are from `gh repo view` on 2026-10-16.

| Project | Last push | Stars / forks | What it is | Fit |
| --- | --- | --- | --- | --- |
| **Fart Machine One** (adamcharleswilsonmusic-code) | 10-12 | 18 / 0 | 27 keys play one-shot samples. 16-step loop with REC/overdub, reverb, delay. About 9k lines, FoMni-based, mono PCM bank compiled in | **Best base.** Already a "key = recorded animal noise" instrument |
| **Dinghy** (zvenson) | 10-12 | 35 / 12 | Template: a ~200-line sine app on the full Felucca/SLOOP platform. Rescue mode, USB-MIDI/audio, settings, screen, build guards, host mode | **Best clean-slate base.** Meant to be copied |
| **Rainbow mode** (joshbartlettband-web) | 10-16 | 16 / 1 | Felucca fork: toy for small children, 94 animal "friends", each with a picture and sound | Closest in spirit. Has an Alley Cat made from synth engines, not a real meow. Too big (full Felucca) to use as a base |
| **FoMni** (charlesvestal) | 10-15 | 63 / 9 | Omnichord-style chord harp. The leanest app layer, and Fart Machine One's parent | Good reference for the app/platform split |
| **Felucca** (hugelton) | 10-16 | 513 / 97 | 13 engines, 4 tracks, 64-step sequencer, SAMPLE engine, user sample upload | **Upstream, but the author has stepped back** (maintenance notice in the README). Too heavy to fork for this |
| **SLOOP** (isod89) | 10-16 | 524 / 69 | Four-track groovebox, version 2.7.1. The most active line | Not a base: the image is about **576,564 of ~581,564 bytes**, so it is full. It also merges no outside PRs |
| **Melodee** (keremimo) | 10-16 | 106 / 16 | Felucca derivative with 299 commits, very active | Feature-rich synth line, no sample focus |
| **DUELL** (oscillr) | 10-15 | 13 / 0 | Bi-timbral morphing synth, public beta. Vendored copy of Felucca | Not relevant |
| Others checked | | | April OS (sampling-focused, license mixed: listed as MIT by one index and as closed by another), FiMba-1 (kalimba model), PurpleMonkey (kids' instrument), foma (one-C-file template, one day old), Barge (platform, zero stars) | Worth a second look only if the two picks fail |

## Forkability findings

- **Licence is clean.** Everything in the useful chain is GPL-3.0-only. Fart Machine One adds a sample provenance warning: its recordings are from Pixabay with unverified terms. Use your own or CC0 meows. A distributed build must ship complete source. The Felucca-family names ("Felucca", "Hügelton Instruments") are trademarks, and Dinghy's logo is all rights reserved, so use your own name and logo.
- **Fart Machine One builds and tests natively on this Mac.** `tests/run_tests.sh` passed (exit 0, 21 "ok" lines plus an importer PASS). The firmware itself still needs the Linux x86-64 JieLi toolchain.
- **Dinghy's host mode does not compile on macOS yet.** `tests/run_host.sh` fails on `__attribute__((section(".pool")))` (Mach-O wants "segment,section"). It is a small fix, or run it on Linux.
- **The toolchain is Linux x86-64 only.** This Mac is arm64 and has no Docker installed. Options: Docker or OrbStack with `linux/amd64`, or build on the x86 Linux box (Cachy-HomePC) natively.
- **Samples need no new infrastructure.** Fart Machine One has `tools/import_samples.py` (any 1-27 WAVs to mono 22.05 kHz, DC removal, fades). The audio budget is 320 KiB, about 7.4 s of PCM. Felucca's SAMPLE engine shows the denser route: IMA ADPCM with zones, root pitch and loops, three 80 KiB user slots (about 7 s each).
- **Recovery is better than first reported.** Hold OCT- at power-on for the USB rescue. Dinghy auto-enters rescue after two boot crashes. Last resort is hardware: FM-1-transporter (RP2040), Quixotic7's unbricker, a Flipper recovery tool. The earlier "no safe recovery" warning was about the stock updater only. Still: test the "Return to official V15" path before flashing custom builds.
- **Quality signals.** Fart Machine One is "installed on our FM-1 and identity verified". Dinghy is "tested on the device". Both are AI-assisted (Felucca and the SLOOP side say so too). Treat everything as two-week-old code.

## Recommendation

**Fork Fart Machine One** and make it a cat piano:

1. Swap the 11 farts for meows (CC0 or your own recordings, via `import_samples.py`).
2. Add pitch: today the 27 keys replay fixed one-shots (11 sounds repeating). Add a per-key resample ratio (Q16 phase increment, as in Felucca's `smp_zone_t`/`pow2_q16`), so one or a few meows span two octaves. Add a key-to-sample map that tunes each cat to its note.
3. Keep its loop, delay and reverb, and drop the cartoon art for a cat.

Choose **Dinghy** instead if you want a clean app with your own key handling, screen and settings, and are happy writing the sample voice yourself.

Open design question for you: one pitched meow versus several multisampled cats (a few zones, more natural) versus a formant/synth meow (Felucca has FORMANT and GRAIN engines). Multisampling costs flash, and a single sample stretched over two octaves gets chipmunky at the top.

## Not done

- Nothing flashed to a device, and no FM-1 hardware was touched.
- Firmware not built (no toolchain here).
- Felucca-wiki release notes and the closed-source options (Baud Girl FM-1+VA, Groove OS at $29) were not reviewed.
- Float: see the SoC row. Melodee uses fp32 and Fart Machine One's build says single-precision FPU, but Dinghy says no float. Settle it on hardware before relying on it.

## Build environment (set up 2026-10-16)

The fork is in `catpiano/` (clone of adamcharleswilsonmusic-code/Fart-Machine-One, remote renamed to `upstream`, branch `main`, full history, nothing committed yet, no GitHub fork created).

| Piece | Where |
| --- | --- |
| Container runtime | Homebrew `colima` 0.10.3 + `docker` 29.9.1, VM type vz with Rosetta (4 CPU, 6 GB, 30 GB disk), mounting `AI_Projects/FM-1` read-write. I removed the missing `credsStore` entry from `~/.docker/config.json` so image pulls work |
| Toolchain | `catpiano/tooling/jieli/toolchain` (jieli-linux-toolchains-20260730.1). The clang sha256 matches `docs/build-versions.json` |
| SDK | `catpiano/tooling/ac79-sdk` at commit d179b448..., the pinned one |
| Python | `catpiano/.venv` from `requirements-build.txt` |
| Env | `. ./env.sh` in `catpiano/` (git-ignored, with `tooling/`, `.venv/`, `build/`) |

Use:

```sh
colima start --vm-type vz --vz-rosetta --cpu 4 --memory 6 --mount "$PWD/..:w"   # once per boot
cd catpiano && . ./env.sh
sh tests/run_tests.sh && sh build.sh        # -> build/fart-machine-one.fwsc
```

Verified: the host tests pass (exit 0), and `build.sh` produces the stock `fart-machine-one.fwsc` (609,657 B, identity FM-1_701). The app image is 353,008 B of the ~581 KB cap, so about 228 KB is free; RAM 25 KB of 96 KB, pool 293 KB of 336 KB. The stock build was not installed on any device.
