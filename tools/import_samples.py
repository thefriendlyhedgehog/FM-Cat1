#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Make the compiled cat banks (firmware/src/dsp/sample_bank.h + .json); source WAVs are never modified.

  import_samples.py --banks samples/rendered/banks.json     the real banks (rendered by tools/render_banks.py)
  import_samples.py --placeholder                           two small synthetic banks for the tests (CC0)

A bank plays 27 keys from note0 up. Each key takes the nearest anchor (a pre-rendered pitch every few semitones,
with one or more takes played in turn) and resamples it the last semitone or so. Takes are 22.05 kHz mono
2nd-order ADPCM (4.6 bits a sample, its own 16 predictors per take); a take may have a sustain loop [ls, le): its
crossfade is baked in here, and the decoder history at ls (samples ls - 1, ls - 2) is stored so a voice can jump back. Pure Python, no numpy: the test suite runs it.
"""
import argparse, array, hashlib, json, math, sys, wave
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
RATE = 22050
BUDGET = 460 * 1024          # ADPCM bytes; the app slot (568 KB) holds code (~75 KB) + this
XFADE = int(0.02 * RATE)     # loop crossfade, samples
NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
def note_name(n): return f'{NOTE_NAMES[n % 12]}{n // 12 - 1}'
def midi_hz(n): return 440.0 * 2 ** ((n - 69) / 12)

# ---- 2nd-order ADPCM (after the GameCube DSP's), bit-exact with omni.c ----
# Frames of 14 samples in 8 bytes: a header (predictor << 4 | scale), then 14 signed nibbles, high nibble first.
# Sample = ((c1 * h1 + c2 * h2 + 1024) >> 11) + (nibble << scale), clamped to 16 bits, with h1, h2 the previous
# two decoded samples and (c1, c2) the take's predictor pair number `predictor` (Q11; pair 0 is (0, 0)).
FRAME = 14; NPRED = 16; MAXSCALE = 12

def adpcm_predictors(x):
    """The take's 16 predictor pairs: least-squares 2nd-order predictors of its frames, clustered (k-means)."""
    pts = []
    for i in range(2, len(x) - FRAME, FRAME):
        r11 = r22 = r12 = r01 = r02 = 0.0
        for j in range(max(2, i - 7), min(len(x), i + FRAME + 7)):
            a, b, y = x[j - 1], x[j - 2], x[j]
            r11 += a * a; r22 += b * b; r12 += a * b; r01 += y * a; r02 += y * b
        det = r11 * r22 - r12 * r12
        if r11 < 1e3 * FRAME or det <= 1e-9 * r11 * r22: continue
        pts.append(tuple(max(-3.99, min(3.99, c)) for c in ((r01 * r22 - r02 * r12) / det, (r02 * r11 - r01 * r12) / det)))
    pts.sort(); cs = pts[::max(1, len(pts) // (NPRED - 1))][:NPRED - 1]     # deterministic seeds across the range
    for _ in range(20):
        acc = [[0.0, 0.0, 0] for _ in cs]
        for p in pts:
            a = acc[min(range(len(cs)), key=lambda j: (p[0] - cs[j][0]) ** 2 + (p[1] - cs[j][1]) ** 2)]
            a[0] += p[0]; a[1] += p[1]; a[2] += 1
        cs = [(a[0] / a[2], a[1] / a[2]) if a[2] else c for a, c in zip(acc, cs)]
    cs = [(0.0, 0.0)] + cs + [(0.0, 0.0)] * (NPRED - 1 - len(cs))
    return [(round(a * 2048), round(b * 2048)) for a, b in cs]

def adpcm_encode(x, pairs, keep=4):
    """Closed loop, frame by frame: of the `keep` predictors with the least open-loop residual, and the scales
    around the one that fits its peak, the choice whose decoded frame is nearest. Returns (bytes, decoded)."""
    nf = (len(x) + FRAME - 1) // FRAME; xs = list(x) + [0] * (nf * FRAME - len(x)); h1 = h2 = 0
    data = bytearray(); out = []
    for f in range(nf):
        fr = xs[f * FRAME:(f + 1) * FRAME]; rank = []
        for p, (c1, c2) in enumerate(pairs):
            a1, a2, peak, e2 = h1, h2, 0, 0
            for s in fr:
                e = s - ((c1 * a1 + c2 * a2 + 1024) >> 11); peak = max(peak, abs(e)); e2 += e * e; a2, a1 = a1, s
            rank.append((e2, p, peak))
        rank.sort(); best = None
        for _, p, peak in rank[:keep]:
            c1, c2 = pairs[p]; s0 = 0
            while s0 < MAXSCALE and 7 << s0 < peak: s0 += 1
            for sc in range(max(0, s0 - 1), min(MAXSCALE, s0 + 1) + 1):
                a1, a2, err, q, ys, half = h1, h2, 0, [], [], (1 << sc) >> 1
                for s in fr:
                    pr = (c1 * a1 + c2 * a2 + 1024) >> 11
                    v = max(-8, min(7, (s - pr + half) >> sc))
                    y = max(-32768, min(32767, pr + (v << sc)))
                    err += (y - s) ** 2; q.append(v); ys.append(y); a2, a1 = a1, y
                    if best and err >= best[0]: break
                else:
                    if not best or err < best[0]: best = (err, p, sc, q, ys)
        _, p, sc, q, ys = best
        data.append(p << 4 | sc); data += bytes((q[i] & 15) << 4 | q[i + 1] & 15 for i in range(0, FRAME, 2))
        out += ys; h1, h2 = ys[-1], ys[-2]
    return bytes(data), out[:len(x)]

def adpcm_decode(data, n, pairs, start=0, h1=0, h2=0):
    out = []
    for i in range(start, n):
        f = data[(i // FRAME) * 8:(i // FRAME) * 8 + 8]; b = f[1 + (i % FRAME >> 1)]
        q = ((b & 15 if i % FRAME & 1 else b >> 4) ^ 8) - 8; c1, c2 = pairs[f[0] >> 4]
        y = max(-32768, min(32767, ((c1 * h1 + c2 * h2 + 1024) >> 11) + (q << (f[0] & 15))))
        out.append(y); h1, h2 = y, h1
    return out

def bake_loop(x, ls, le):
    """Crossfade the end of the loop into what precedes its start, so sample le-1 leads into ls like ls-1 does."""
    x = list(x); c = min(XFADE, ls, (le - ls) // 2)
    for i in range(c):
        w = (i + 1) / c
        x[le - c + i] = round(x[le - c + i] * (1 - w) + x[ls - c + i] * w)
    return x

def read_wav(path):
    with wave.open(str(path), 'rb') as w:
        if w.getsampwidth() != 2 or w.getnchannels() != 1 or w.getframerate() != RATE:
            raise ValueError(f'{path}: expected 16-bit mono {RATE} Hz (tools/render_banks.py writes these)')
        a = array.array('h', w.readframes(w.getnframes()))
        if sys.byteorder != 'little': a.byteswap()
        return list(a)

# ---- synthetic placeholder cats (CC0): a formant-swept meow held exactly on its note from 20% to 65% ----
STEADY = (0.20, 0.65)
VOWELS = [[(350, 120, 1.0), (2300, 220, 0.55), (3100, 300, 0.30)],
          [(800, 160, 1.0), (1300, 200, 0.70), (2700, 300, 0.30)],
          [(400, 140, 1.0), (850, 180, 0.60), (2500, 300, 0.20)]]

def meow(note, seconds, variant=0):
    f0 = midi_hz(note); n = int(seconds * RATE); phi = 0.0; data = []
    for i in range(n):
        t = i / n
        if t < STEADY[0]: ratio = 0.88 + 0.12 * math.sin(0.5 * math.pi * t / STEADY[0])
        elif t < STEADY[1]: ratio = 1.0
        else: ratio = 1.0 - 0.20 * ((t - STEADY[1]) / (1 - STEADY[1])) ** 1.5
        f = f0 * ratio; phi += 2 * math.pi * f / RATE
        a, b, w = (VOWELS[0], VOWELS[1], t / 0.35) if t < 0.35 else (VOWELS[1], VOWELS[2], (t - 0.35) / 0.65)
        fmt = [(fa + (fb - fa) * w, ba + (bb - ba) * w, ga + (gb - ga) * w) for (fa, ba, ga), (fb, bb, gb) in zip(a, b)]
        env = math.sin(0.5 * math.pi * min(1.0, t / 0.08)) * (math.cos(0.5 * math.pi * (t - 0.70) / 0.30) ** 2 if t > 0.70 else 1.0)
        s = 0.0; h = 1
        while h * f < 8000:
            fh = h * f; g = sum(gf / (1 + ((fh - ff) / (0.5 * bf)) ** 2) for ff, bf, gf in fmt)
            s += (0.08 + g + 0.05 * variant * (h % 2)) / h * math.sin(h * phi); h += 1
        data.append(s * env)
    peak = max(abs(v) for v in data)
    data = [round(16000 * v / peak) for v in data]
    period = RATE / f0; ls = int(0.30 * n); le = ls + round(round(0.1 * RATE / period) * period)
    return data, (ls, le)

def placeholder():
    """Two banks: TEST A (one take per anchor, from G4) and TEST B (two takes played in turn, from G3)."""
    banks = []
    for name, note0, ntakes, secs in (('TEST A', 67, 1, 0.35), ('TEST B', 55, 2, 0.30)):
        anchors = []
        for a in range(note0 + 1, note0 + 27, 3):
            takes = []
            for v in range(ntakes):
                data, loop = meow(a, secs, v)
                takes.append({'data': data, 'loop': loop, 'source': 'synthetic'})
            anchors.append({'note': a, 'takes': takes})
        banks.append({'name': name, 'note0': note0, 'anchors': anchors})
    return banks, [{'id': 'synthetic', 'license': 'CC0 1.0', 'author': 'tools/import_samples.py', 'source': 'generated'}]

def load_banks(path):
    doc = json.loads(path.read_text())
    for b in doc['banks']:
        for a in b['anchors']:
            for t in a['takes']:
                t['data'] = read_wav(path.parent / t['file'])
                t['loop'] = tuple(t['loop']) if t.get('loop') else None
    return doc['banks'], doc['sources']

def key_table(bank):
    """Per key: (anchor index in the bank, Q16 source frames per 44.1 kHz output frame)."""
    keys = []
    for k in range(27):
        note = bank['note0'] + k
        i = min(range(len(bank['anchors'])), key=lambda i: (abs(note - bank['anchors'][i]['note']), bank['anchors'][i]['note']))
        keys.append((i, round(65536 * 0.5 * 2 ** ((note - bank['anchors'][i]['note']) / 12))))
    return keys

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--banks', type=Path, help='samples/rendered/banks.json')
    ap.add_argument('--placeholder', action='store_true')
    ap.add_argument('--output', type=Path, default=ROOT / 'firmware/src/dsp/sample_bank.h')
    a = ap.parse_args()
    if bool(a.banks) == bool(a.placeholder): ap.error('give --banks or --placeholder')
    banks, sources = placeholder() if a.placeholder else load_banks(a.banks)
    if not 1 <= len(banks) <= 8: ap.error('1-8 banks')
    for b in banks:
        if not 1 <= len(b['name']) <= 10 or any(not ' ' <= c <= '~' for c in b['name']): ap.error(f"bank name {b['name']!r}: 1-10 printable ASCII")
        if not 21 <= b['note0'] <= 101: ap.error(f"{b['name']}: note0 {b['note0']} out of range")

    # Takes: bake loops, encode, and store a take shared by several anchors or banks once.
    blob = bytearray(); takes = []; preds = []; seen = {}; ref = []; anchor_rows = []
    for b in banks:
        b['anchor0'] = len(anchor_rows)
        for an in b['anchors']:
            an['ix'] = []
            for t in an['takes']:
                x, loop = t['data'], t['loop']
                if len(x) >= 65535: ap.error(f"{t.get('file', 'take')}: under 65535 frames, please")
                if loop and not (2 <= loop[0] < loop[1] <= len(x) and loop[1] - loop[0] >= 64): ap.error(f'bad loop {loop}')
                if loop: x = bake_loop(x, *loop)
                h = hashlib.sha256(repr((x, loop)).encode()).hexdigest()
                if h not in seen:
                    pairs = adpcm_predictors(x); data, dec = adpcm_encode(x, pairs)
                    lh1, lh2 = (dec[loop[0] - 1], dec[loop[0] - 2]) if loop else (0, 0)
                    seen[h] = len(takes); preds.append(pairs)
                    takes.append((len(blob), len(x), loop or (0, 0), lh1, lh2, t.get('source', '')))
                    blob += data
                an['ix'].append(seen[h])
            anchor_rows.append((len(ref), len(an['ix']), an['note'])); ref += an['ix']
    if len(blob) > BUDGET: ap.error(f'banks are {len(blob)} bytes of ADPCM, budget {BUDGET}: fewer takes or shorter ones')

    lines = ['/* SPDX-License-Identifier: GPL-3.0-only */',
             '/* Generated by tools/import_samples.py. See sample_bank.json for audio provenance. */',
             '#pragma once', '#include <stdint.h>', f'#define FMO_PLACEHOLDER {int(a.placeholder)}',
             f'#define CP_NBANKS {len(banks)}',
             '/* A take: n samples of ADPCM from byte off (frames of 14 in 8 bytes, see tools/import_samples.py) with',
             ' * predictor pairs cp_preds[take], a sustain loop [ls, le) (le 0: none), and decoded samples ls - 1, ls - 2 */',
             'typedef struct { uint32_t off; uint16_t n, ls, le; int16_t lh1, lh2; } cp_take_t;',
             '/* An anchor: its takes (played in turn) at cp_take_ref[ref .. ref + ntakes), rendered at MIDI note */',
             'typedef struct { uint16_t ref; uint8_t ntakes, note; } cp_anchor_t;',
             '/* A key: an anchor of its bank and Q16 source frames per 44.1 kHz output frame */',
             'typedef struct { uint8_t anchor; uint32_t inc_q16; } cp_key_t;',
             'typedef struct { const char *name; uint8_t note0; uint16_t anchor0; uint8_t nanchors; cp_key_t keys[27]; } cp_bank_t;',
             'static const uint8_t cp_adpcm[] = {']
    lines += [','.join(str(v) for v in blob[i:i + 32]) + ',' for i in range(0, len(blob), 32)]
    lines += ['};', 'static const cp_take_t cp_takes[] = {']
    lines += ['{%du,%du,%du,%du,%d,%d},' % (off, n, lp[0], lp[1], lh1, lh2) for off, n, lp, lh1, lh2, _ in takes]
    lines += ['};', 'static const int16_t cp_preds[][%d] = {' % (2 * NPRED)]
    lines += ['{' + ','.join(str(c) for pair in pairs for c in pair) + '},' for pairs in preds]
    lines += ['};', 'static const uint16_t cp_take_ref[] = {' + ','.join(map(str, ref)) + '};',
              'static const cp_anchor_t cp_anchors[] = {' + ','.join('{%d,%d,%d}' % r for r in anchor_rows) + '};',
              'static const cp_bank_t cp_banks[CP_NBANKS] = {']
    for b in banks:
        lines.append('{%s,%d,%d,%d,{%s}},' % (json.dumps(b['name']), b['note0'], b['anchor0'], len(b['anchors']),
                                              ','.join('{%d,%du}' % k for k in key_table(b))))
    lines.append('};')
    a.output.parent.mkdir(parents=True, exist_ok=True)
    a.output.write_text('\n'.join(lines) + '\n')

    total = sum(t[1] for t in takes)
    manifest = {'placeholder': a.placeholder, 'sample_rate': RATE, 'adpcm_bytes': len(blob), 'budget_bytes': BUDGET,
                'seconds': round(total / RATE, 2), 'sources': sources,
                'banks': [{'name': b['name'], 'note0': b['note0'], 'keys': f"{note_name(b['note0'])}-{note_name(b['note0'] + 26)}",
                           'anchors': [{'note': note_name(an['note']), 'takes': [takes[i][5] for i in an['ix']]} for an in b['anchors']]}
                          for b in banks]}
    a.output.with_suffix('.json').write_text(json.dumps(manifest, indent=1) + '\n')
    print(f"{len(banks)} banks, {len(takes)} takes, {total / RATE:.1f} s, {len(blob)} ADPCM bytes; "
          + ('PLACEHOLDER MEOWS' if a.placeholder else str(a.banks)))

if __name__ == '__main__':
    try: main()
    except (ValueError, wave.Error, OSError) as e: sys.exit(str(e))
