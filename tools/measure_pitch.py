#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Measure the root pitch of each sound in a sample manifest (authoring only; needs numpy).
For each entry: a harmonic sieve over [start, end] picks the F0 (robust to octave errors), refined by the median
of YIN frames near it, reported as root_hz. An entry with "root_checked" (a note on how it was checked) keeps
its root_hz. --write stores root_hz back into the manifest.
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

def sieve(x, sr, fmin=100.0, fmax=1600.0):
    """Harmonic sieve over the loud frames: a candidate F0 scores the spectrum at its harmonics minus the spectrum
    halfway between them. That rejects both octave errors YIN makes on cats with weak fundamentals or strong
    upper harmonics (a candidate an octave up loses its odd harmonics to the penalty, one an octave down scores
    empty harmonics). Returns (hz, frames used)."""
    N = 4096; hop = sr // 50; win = np.hanning(N)
    frames = [x[i:i + N] for i in range(0, max(1, len(x) - N), hop)]
    e = np.array([np.sqrt(np.mean(f ** 2)) for f in frames])
    loud = [f * win for f, v in zip(frames, e) if v > 0.25 * e.max()]
    if not loud: return None, 0
    cand = fmin * 2 ** (np.arange(0, int(96 * math.log2(fmax / fmin))) / 96)
    k = np.arange(1, 9)                     # the same 8 harmonics for every candidate: a fair mean
    on, off = np.outer(cand, k), np.outer(cand, k - 0.5)
    bins = np.arange(N // 2 + 1) * sr / N; picks = []
    for f in loud:                          # per frame: a gliding meow smears an averaged spectrum
        spec = np.sqrt(np.abs(np.fft.rfft(f))); spec /= spec.max()
        score = np.interp(on, bins, spec, right=0).mean(1) - 0.7 * np.interp(off, bins, spec, right=0).mean(1)
        picks.append(cand[int(np.argmax(score))])
    return float(np.median(picks)), len(loud)

def measure(x, sr):
    hz, n = sieve(x, sr)
    if hz is None: return None, 0, 0
    # refine with YIN frames near the sieve's answer (within 3 semitones): the median of those
    N = 2048; hop = sr // 100
    f0 = [f for i in range(0, max(1, len(x) - N), hop) for f in [yin(x[i:i + N], sr, fmin=hz / 1.2, fmax=hz * 1.2)]
          if f and abs(12 * math.log2(f / hz)) < 3]
    return (float(np.median(f0)) if len(f0) >= 3 else hz), n, len(f0)

def main():
    ap = argparse.ArgumentParser(description=__doc__); ap.add_argument('manifest', type=Path); ap.add_argument('--write', action='store_true')
    a = ap.parse_args(); doc = json.loads(a.manifest.read_text()); entries = doc["sources"] if isinstance(doc, dict) else doc
    for e in entries:
        if e.get('root_checked'):            # set by hand after a look at the spectrogram: keep it
            print(f"{e.get('label', e.get('id')):<10} {e['root_hz']:7.1f} Hz (checked by hand: {e['root_checked']})"); continue
        x, sr = load(a.manifest.parent / e['file'], e.get('start'), e.get('end'))
        hz, n, total = measure(x, sr)
        if hz is None: raise SystemExit(f"{e['file']}: no pitch found")
        midi = 69 + 12 * math.log2(hz / 440)
        print(f"{e.get('label', e.get('id')):<10} {e['file']:<36} {hz:7.1f} Hz  MIDI {midi:5.2f}  ({n} sieve / {total} YIN frames)")
        e['root_hz'] = round(hz, 1)
    if a.write: a.manifest.write_text(json.dumps(doc, indent=1) + "\n")

if __name__ == '__main__': main()
