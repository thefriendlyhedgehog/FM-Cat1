#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Measure the root pitch of each sound in a sample manifest (authoring only; needs numpy).
For each entry: YIN F0 every 10 ms over [start, end], drop octave slips (more than 7 semitones from the median),
and report the median as root_hz. --write stores root_hz back into the manifest.
  python3 tools/measure_pitch.py samples/cc0-meows/manifest.json [--write]
"""
import argparse, json, math, wave
from pathlib import Path
import numpy as np

def load(path, start, end):
    with wave.open(str(path), 'rb') as w:
        if w.getsampwidth() != 2: raise SystemExit(f'{path}: use 16-bit PCM WAV')
        sr, ch = w.getframerate(), w.getnchannels()
        x = np.frombuffer(w.readframes(w.getnframes()), '<i2').astype(float).reshape(-1, ch).mean(axis=1) / 32768
    a = int((start or 0) * sr); b = int(end * sr) if end else len(x)
    return x[a:b], sr

def yin(fr, sr, fmin=150.0, fmax=2200.0, thr=0.3):
    W = len(fr) // 2; tmin = int(sr / fmax); tmax = int(sr / fmin)
    d = np.array([np.sum((fr[:W] - fr[t:t + W]) ** 2) for t in range(tmax + 1)])
    c = np.ones_like(d); c[1:] = d[1:] * np.arange(1, tmax + 1) / np.maximum(np.cumsum(d[1:]), 1e-12)
    for t in range(max(tmin, 2), tmax):
        if c[t] < thr and c[t] <= c[t + 1]:
            den = c[t - 1] - 2 * c[t] + c[t + 1]
            return sr / (t + (0.5 * (c[t - 1] - c[t + 1]) / den if den else 0.0))
    return None

def measure(x, sr):
    N = 2048; hop = sr // 100
    rms = np.array([np.sqrt(np.mean(x[i:i + N] ** 2)) for i in range(0, max(1, len(x) - N), hop)])
    f0 = [f for i, r in enumerate(rms) if r > 0.15 * rms.max() for f in [yin(x[i * hop:i * hop + N], sr)] if f]
    if len(f0) < 3: return None, 0, len(rms)
    med = float(np.median(f0)); keep = [f for f in f0 if abs(12 * math.log2(f / med)) < 7]
    return float(np.median(keep)), len(keep), len(rms)

def main():
    ap = argparse.ArgumentParser(description=__doc__); ap.add_argument('manifest', type=Path); ap.add_argument('--write', action='store_true')
    a = ap.parse_args(); entries = json.loads(a.manifest.read_text())
    for e in entries:
        x, sr = load(a.manifest.parent / e['file'], e.get('start'), e.get('end'))
        hz, n, total = measure(x, sr)
        if hz is None: raise SystemExit(f"{e['file']}: no pitch found")
        midi = 69 + 12 * math.log2(hz / 440)
        print(f"{e['label']:<10} {e['file']:<28} {hz:7.1f} Hz  MIDI {midi:5.2f}  ({n}/{total} frames voiced)")
        e['root_hz'] = round(hz, 1)
    if a.write: a.manifest.write_text(json.dumps(entries, indent=2) + '\n')

if __name__ == '__main__': main()
