> Development notes, kept as written at the time (2026-10). Paths like `catpiano/` and `../*.mp3` refer to the
> original workspace: `catpiano/` is this repository, and the demos are in `docs/demos/`.

# Cat Piano: sequencer and dual-core plan

2026-10-16. Researched from shallow clones in `FM-1/reference/` (18 FM-1 firmwares, outside the catpiano repo).
Nothing in `catpiano/` has changed yet.

## Short version

- **Sequencer: build on SLOOP's** (the deepest one around). Add Felucca's probability conditions and MIDI clock, and give each note its own length so sustained meows work. About 25-32 KB of code against 78 KB free.
- **Dual core: not yet, and not from SLOOP.** Cat Piano's render is estimated at 12-18% of one core with 8 voices, so a second core saves little today. If we need it later, take Felucca 1.6.1's copy: X0X's code plus the fixes for units that misread RAM. SLOOP's copy predates those fixes, and on SLOOP a crash on core 1 can freeze the device.
- **First step:** put the CPU meter on screen and measure on the FM-1. The meter exists (`plat_cpu_pct`, `om_dbg.max_us` in `main_fm1.c`), but nothing shows it.

## The sequencer

Almost every FM-1 groovebox sequencer is a copy of Felucca's `seq.c`: SLOOP, Skiff, Melodee, JIANT, DUELL, sloopDX, ORBIT. None of them works as a drop-in module, because each runs inside its own engine's audio interrupt. So we write our own `firmware/src/dsp/seq.c`, borrowing their algorithms and data layouts, and keep their copyright headers.

| Source | What we take |
| --- | --- |
| **SLOOP 2.7.1** (`firmware/src/seq.c`, `ui_layers.c`, `tests/seq2_test.c`, 153 checks) | 4 tracks × 64 steps, with length, division and swing per track (polymeter). The shared-clock step grid never drifts. Chords of up to 4 notes per step, with velocity and ratchet (×1-4) per note. Nudge in 1/64 of a step. Parameter locks. Fill and no-fill. Undo/redo. Sections A-D with chaining and song record. MIDI clock in and out. The hold-SEQ step-editing controls. |
| **Felucca 1.6.1** | Per-step probability with a:b conditions (`seq_cond_miss`); `midi_clock.c` |
| **Our own change** | A length for each note instead of SLOOP's whole-step TIE. With meows that sustain, one note of a chord can end before the others. |
| Rejected | fm1-t1 (unfinished, unclear provenance), fm1-polyseq (no licence), DriftSeq (uses doubles and too much RAM), X0X (built for drums and a mono 303 line) |

**How it maps onto Cat Piano**

- **Tracks.** 4 tracks ("cats"), each with its own bank. Steps store the key index (0-26), not the MIDI note, so a pattern still makes sense when a track's bank changes.
- **Locks.** Per-step locks for bank (a different cat on one step), pitch in cents and velocity.
- **Engine changes in `omni.c`:**
  - Sequenced notes sustain until their length ends.
  - Each voice records who started it (live playing or which track), so a live key-up doesn't cut a sequenced meow.
  - Voice stealing prefers the same track.
  - 8 voices may be too few for 4 tracks of chords; the CPU measurement decides whether we can raise that.
- **Controls** (only PLAY, REC and the encoders are used today):

  | Control | Job |
  | --- | --- |
  | SEQ hold | White keys are steps 1-16; black keys 1-4 are pages |
  | SEQ hold + a step held | Knobs set velocity, ratchet, nudge and length; PRESET locks the bank; ALGO sets the chance |
  | SEL hold | Choose a track |
  | GLO hold | Mute, solo, fill, tap tempo |
  | SAVE hold | Recall or store A-D, chain them |
  | EDIT hold | Erase notes as the playhead passes; undo and redo |

  PLAY and REC (tap to overdub, hold to clear), and SELECT for tempo, work as they do now.
- **Patterns in flash.** 4 slots of about 3.3 KB each, in the free A/B CRC store region 0x97000-0x9DFFF. The store is already torn-write-safe. Power-on stays predictable: nothing loads until you use SAVE.
- **RAM.** About +6.5 KB (currently 25.9 of 96 KB used).

**Milestones**

| | Contents | Done when |
| --- | --- | --- |
| S0 | CPU meter: a held key combo shows load % and the worst block in µs | Measured on the FM-1 with 8 voices, dense playing, full reverb and delay |
| S1 | Engine: `start_voice(bank, key, vel, len, owner)`, note lengths, voice owners | Host tests: a live key-up spares sequenced notes; lengths are exact to the sample |
| S2 | `seq.c`: 4 tracks with length/division/swing, 4-note chords with note lengths, live record, conditions/probability, ratchets, nudge. Port SLOOP's `seq2_test` cases. | Sequencer tests pass with no drift over 10 simulated minutes |
| S3 | UI layers (SEQ, SEL, GLO, EDIT) and a step-grid screen | Host scenarios for each layer, plus screenshots |
| S4 | Sections A-D, chaining, undo, pattern save/load in flash | Storage tests: round trip, torn writes, version and magic checks |
| S5 | MIDI clock in and out, sequenced notes sent to MIDI out | Host test against a simulated 24 ppqn clock |
| S6 | Install on the FM-1 (with your go-ahead) | Plays without dropouts; CPU measured again |

## Dual core (deferred)

What it would take, so it's ready when needed:

- **Code.** Felucca 1.6.1's `hal/fm1_cpu1.{h,S}` (4 KB core-1 stack, 4 ms timeout, a check word for units that misread RAM), its interrupts-off core-1 halt on every reset and update path, a `.c1_text` linker section, and `build.py` checks. In `omni.c`, voices 0-3 render on core 0 and voices 4-7 on core 1, split at sequencer step boundaries; echo and plate stay serial.
- **Cost.** About +2 KB of code and +5-6 KB of RAM.
- **Risks.** All of them show up only on the device:
  - Core 1 not starting, or misreading RAM on some units.
  - Both cores reading the sample bank from flash at once.
  - A crash when holding core 1 during an update. The fix exists in Felucca 1.6.1.
  - X0X raises the chip's supply voltages to keep some units stable; Felucca deliberately doesn't. That is a hardware call for you.
- **When it's worth it.** Heavy DSP: live formant-preserving pitch shifting instead of pre-rendered anchors, granular playback, per-voice filters, a bigger reverb. Or if S0 shows 4 tracks of chords need more than about 16 voices.

## Progress (2026-10-16)

- **S0 done:** hold FX for the audio load. It shows CPU now and its peak since FX went down, xruns and voices. Not yet read on the device: that needs an install.
- **S1 done:** voices carry an owner and a hold length. Live key-ups spare sequenced notes; a track steals its own oldest voice first.
- **S2 done (engine):** `firmware/src/dsp/seq.c`.
  - 4 tracks × 64 steps × 4 notes, each note with its own length.
  - SLOOP's shared-clock grid: per-track LEN, DIV and SWING, exact over 10 minutes. Nudge, ratchets, and record latency (512 samples).
  - Felucca's chance, a:b and fill conditions; mute.
  - SEL + white key 1-4 selects the track, even mid-recording. PRESET sets that track's bank. REC hold clears the selected track.
  - Tests: `sequencer_test` (exact frame timings for swing, nudge and ratchets, mutation-checked), `sequencer_ui_test`, `tracks.omni`. The demo records 4 tracks through the UI: `../catpiano-demo-seq.mp3`.
  - Size: image 507,308 B (+4.4 KB), RAM 31.1 KB (+5.2 KB). Host stress test with 4 tracks of 4-note chords at 240 BPM: 0.28% of real time.
- **Next: S3**, the panel controls for LEN, DIV, SWING, nudge, ratchet, conditions and mute, plus the step grid screen. Committed as d945847 and installed on the FM-1.
- **Arpeggiator done (2026-10-16):** in `seq.c`, after SLOOP's.
  - 10 modes plus CHORD, rates 1/32-1/4 including triplets, 1-3 octaves (12 keys up; past the top key a note is left out), gate, latch.
  - Timing: on the transport grid while playing (a new chord plays at once unless it's within a quarter step of the grid); free-running from the first key while stopped.
  - Its notes record into the selected track as heard.
  - Controls: ARP tap toggles; ARP hold + K1-K4 / ALGO edit.
  - Tests: `arp_test` (mutation-checked) and `arp.omni`. Image 509,624 B (+2.3 KB). Demo: `../catpiano-demo-arp.mp3`.
