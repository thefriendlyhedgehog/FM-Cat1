#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Render the cat banks (authoring only; needs numpy, ffmpeg and the rubberband CLI).

samples/banks.json lists the source recordings (crop, measured root_hz, licence) and the banks: each bank plays
27 keys from note0 up, with an anchor every `anchor_step` semitones (note0+1, note0+4, ...). For every anchor this
renders `takes` takes (a bank may set its own) from the bank's pool, nearest in pitch first: the crop pitch-shifted to the anchor note with
formants and duration kept (rubberband -3 -F), 22.05 kHz mono, trimmed, loudness-matched, with a sustain loop
found in its steady middle. Output: samples/rendered/<bank>/<anchor>-<take>.wav and samples/rendered/banks.json,
which tools/import_samples.py --banks turns into the firmware's ADPCM bank. Commit the output: the firmware build
does not need rubberband.
  python3 tools/render_banks.py [samples/banks.json]
"""
import json, math, shutil, subprocess, sys, tempfile, wave
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
SR = 22050
MAX_S = 1.1            # longest take, seconds (holding a key loops the take)
TARGET_RMS = 0.12      # loudness of the voiced part (peaks limited to 0.85)

def midi(hz): return 69 + 12 * math.log2(hz / 440)
def note_name(n): return ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'][n % 12] + str(n // 12 - 1)

def read(path):
    with wave.open(str(path)) as w:
        return np.frombuffer(w.readframes(w.getnframes()), '<i2').astype(float) / 32768

def write(path, x):
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes((np.clip(x, -1, 1) * 32767).round().astype('<i2').tobytes())

def render(src, shift, tmp):
    crop, shifted, out = tmp / 'crop.wav', tmp / 'shift.wav', tmp / 'out.wav'
    args = ['ffmpeg', '-loglevel', 'error', '-y', '-i', str(ROOT / 'samples' / src['file'])]
    if 'start' in src: args += ['-ss', str(src['start'])]
    if 'end' in src: args += ['-to', str(src['end'])]
    subprocess.run(args + ['-ac', '1', '-ar', '44100', '-c:a', 'pcm_s16le', str(crop)], check=True)
    subprocess.run(['rubberband', '-q', '-3', '-F', '-p', f'{shift:.4f}', str(crop), str(shifted)], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(['ffmpeg', '-loglevel', 'error', '-y', '-i', str(shifted), '-ar', str(SR), '-c:a', 'pcm_s16le',
                    str(out)], check=True)
    return read(out)

def tidy(x):
    """Trim quiet ends, cap the length, fade the ends, match loudness."""
    env = np.sqrt(np.convolve(x * x, np.ones(220) / 220, 'same'))
    on = np.nonzero(env > 0.06 * env.max())[0]
    x = x[max(0, on[0] - 220):min(len(x), on[-1] + 660)][:int(MAX_S * SR)]
    n = len(x); f = min(220, n // 4); g = np.ones(n)
    g[:f] = np.linspace(0, 1, f); g[-4 * f:] = np.minimum(g[-4 * f:], np.linspace(1, 0, 4 * f))
    x = x * g
    env = np.sqrt(np.convolve(x * x, np.ones(441) / 441, 'same'))
    loud = env[env > 0.3 * env.max()]
    x = x * (TARGET_RMS / max(1e-6, float(np.sqrt(np.mean(loud ** 2)))))
    return x * min(1.0, 0.85 / max(1e-6, np.abs(x).max()))

def find_loop(x):
    """Sustain loop (ls, le) in the steady middle: the template around ls that best repeats 60..250 ms later.
    None when nothing repeats well (the take stays a one-shot)."""
    n = len(x); W = 132; lo, hi = int(0.06 * SR), int(0.25 * SR)
    best = (0.0, None)
    for ls in range(int(0.25 * n), int(0.6 * n), 110):
        if ls < W or ls + hi + W > int(0.88 * n): continue
        t = x[ls - W:ls + W]; tn = np.sqrt(np.dot(t, t))
        if tn < 1e-3: continue
        seg = x[ls + lo - W:ls + hi + W]
        c = np.correlate(seg, t, 'valid')
        e = np.sqrt(np.convolve(seg * seg, np.ones(2 * W), 'valid'))[:len(c)]
        score = c / (tn * e + 1e-9)
        k = int(np.argmax(score))
        if score[k] > best[0]: best = (float(score[k]), (ls, ls + lo + k))
    return best[1] if best[0] >= 0.85 else None, best[0]

def main():
    doc_path = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'samples/banks.json'
    doc = json.loads(doc_path.read_text()); src = {s['id']: s for s in doc['sources']}
    out_dir = ROOT / 'samples/rendered'
    if out_dir.exists(): shutil.rmtree(out_dir)
    step, ntakes = doc['anchor_step'], doc['takes']
    banks = []
    with tempfile.TemporaryDirectory() as td:
        tmp = Path(td)
        for b in doc['banks']:
            anchors = []
            for a in range(b['note0'] + 1, b['note0'] + 27, step):
                pool = sorted(b['pool'], key=lambda i: (abs(a - midi(src[i]['root_hz'])), i))[:b.get('takes', ntakes)]
                takes = []
                for t, sid in enumerate(pool):
                    shift = a - midi(src[sid]['root_hz'])
                    x = tidy(render(src[sid], shift, tmp))
                    loop, score = find_loop(x)
                    rel = f"{b['name'].lower()}/{a}-{t}.wav"; write(out_dir / rel, x)
                    takes.append({'file': rel, 'source': sid, 'shift': round(shift, 2), 'frames': len(x),
                                  'loop': list(loop) if loop else None, 'loop_score': round(score, 3)})
                    print(f"{b['name']:<8} {note_name(a):<4} {sid:<8} {shift:+6.2f} st  {len(x) / SR:4.2f} s  "
                          f"loop {'%.2f-%.2f s' % (loop[0] / SR, loop[1] / SR) if loop else 'none':<14} ({score:.2f})")
                anchors.append({'note': a, 'takes': takes})
            banks.append({'name': b['name'], 'note0': b['note0'], 'anchors': anchors})
    used = sorted({t['source'] for b in banks for a in b['anchors'] for t in a['takes']})
    sources = [{k: v for k, v in src[i].items()} for i in used]
    (out_dir / 'banks.json').write_text(json.dumps({'sample_rate': SR, 'banks': banks, 'sources': sources}, indent=1) + '\n')
    total = sum({(t['source'], a['note']): t['frames'] for b in banks for a in b['anchors'] for t in a['takes']}.values())
    print(f"{len(banks)} banks, {total / SR:.1f} s of distinct audio, about {total // 2 // 1024} KB as ADPCM; {len(used)} sources")

if __name__ == '__main__': main()
