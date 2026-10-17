#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Make the compiled sample bank; source WAVs are never modified.
PCM WAV, 16 bit, mono/stereo; output 22050 Hz mono. --placeholder makes synthetic tuned meows. Sorted files map left-to-right,
cycling when fewer than 27. Supply --map JSON with exactly 27 source basenames to override.
--names JSON maps source basenames to display labels without renaming source WAVs.
"""
import argparse, array, hashlib, json, math, re, struct, sys, wave
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
RATE=22050
BUDGET=320*1024

def read_wav(path,start=None,end=None,normalize=False):
    with wave.open(str(path),'rb') as w:
        if w.getsampwidth()!=2 or w.getnchannels() not in (1,2) or w.getcomptype()!='NONE':
            raise ValueError(f'{path.name}: use uncompressed 16-bit mono or stereo WAV')
        rate,ch=w.getframerate(),w.getnchannels()
        if not 8000<=rate<=192000 or w.getnframes()>rate*30:
            raise ValueError(f'{path.name}: expected 8–192 kHz and at most 30 seconds')
        raw=array.array('h',w.readframes(w.getnframes()))
        if sys.byteorder!='little':raw.byteswap()
    if not raw:raise ValueError(f'{path.name}: empty audio')
    x=[sum(raw[i:i+ch])/ch for i in range(0,len(raw),ch)]
    if start is not None or end is not None:
        x=x[int((start or 0)*rate):int(end*rate) if end else len(x)]
        if not x:raise ValueError(f'{path.name}: start/end leave no audio')
    # Windowed-sinc low-pass resampling: prevent downsampling aliasing.
    if rate!=RATE:
        out=[]; ratio=rate/RATE; cutoff=min(1.0,RATE/rate)*0.94; radius=24
        for j in range(max(1,round(len(x)/ratio))):
            pos=j*ratio; mid=int(pos); total=weight=0.0
            for k in range(max(0,mid-radius+1),min(len(x),mid+radius+1)):
                d=pos-k; t=d*cutoff
                sinc=1.0 if abs(t)<1e-12 else math.sin(math.pi*t)/(math.pi*t)
                win=0.5+0.5*math.cos(math.pi*d/radius)
                c=cutoff*sinc*win;total+=x[k]*c;weight+=c
            out.append(total/weight if weight else 0.0)
        x=out
    # Remove DC, cap peaks (never amplify), and fade both boundaries for click-free starts/ends.
    dc=sum(x)/len(x);x=[v-dc for v in x];peak=max(abs(v) for v in x)
    scale=28000/max(1.0,peak) if normalize else min(1.0,28000/max(1.0,peak));fade=min(110,len(x)//2)
    return [round(v*scale*(min(1.0,i/max(1,fade),(len(x)-1-i)/max(1,fade)))) for i,v in enumerate(x)]

KEY0_NOTE=67   # key 1 plays G4; the 27 keys span G4..A6
NOTE_NAMES=['C','C#','D','D#','E','F','F#','G','G#','A','A#','B']
def note_name(n): return f'{NOTE_NAMES[n%12]}{n//12-1}'
def midi_hz(n): return 440.0*2**((n-69)/12)

# Synthetic placeholder cats: (root MIDI note, label, seconds), low to high. Generated here, not recorded:
# dedicated to the public domain (CC0 1.0). The F0 is held exactly on the root from 20% to 65% of each sound.
PLACEHOLDER_CATS=[(69,'TOMCAT',0.55),(74,'TABBY',0.50),(79,'CALICO',0.45),(84,'SIAMESE',0.40),(90,'KITTEN',0.35)]
STEADY=(0.20,0.65)
# Vowel formants (Hz, bandwidth Hz, gain): "mi" -> "a" -> "ow", the usual meow shape.
VOWELS=[[(350,120,1.0),(2300,220,0.55),(3100,300,0.30)],
        [(800,160,1.0),(1300,200,0.70),(2700,300,0.30)],
        [(400,140,1.0),(850,180,0.60),(2500,300,0.20)]]

def meow(root,seconds):
    f0=midi_hz(root);n=int(seconds*RATE);phi=0.0;data=[]
    for i in range(n):
        t=i/n
        # F0: glide up into the root, hold it, sag at the end.
        if t<STEADY[0]: ratio=0.88+0.12*math.sin(0.5*math.pi*t/STEADY[0])
        elif t<STEADY[1]: ratio=1.0
        else: ratio=1.0-0.20*((t-STEADY[1])/(1-STEADY[1]))**1.5
        f=f0*ratio;phi+=2*math.pi*f/RATE
        if t<0.35: a,b,w=VOWELS[0],VOWELS[1],t/0.35
        else: a,b,w=VOWELS[1],VOWELS[2],(t-0.35)/0.65
        fmt=[(fa+(fb-fa)*w,ba+(bb-ba)*w,ga+(gb-ga)*w) for (fa,ba,ga),(fb,bb,gb) in zip(a,b)]
        env=math.sin(0.5*math.pi*min(1.0,t/0.08))*(math.cos(0.5*math.pi*(t-0.70)/0.30)**2 if t>0.70 else 1.0)
        s=0.0;h=1
        while h*f<8000:
            fh=h*f;g=sum(gf/(1+((fh-ff)/(0.5*bf))**2) for ff,bf,gf in fmt)
            s+=(0.08+g)/h*math.sin(h*phi);h+=1
        data.append(s*env)
    peak=max(abs(v) for v in data)
    return [round(20000*v/peak) for v in data]

def placeholder():
    return [(name,meow(root,sec),None,root) for root,name,sec in PLACEHOLDER_CATS]

def key_table(roots,mapping=None):
    """Per key: (sample index, target note or -1, Q16 source frames per 44.1 kHz output frame).
    Pitched samples: each key takes the nearest root (ties go to the lower one)."""
    keys=[]
    for k in range(27):
        note=KEY0_NOTE+k
        pitched=[i for i,r in enumerate(roots) if r is not None]
        if mapping is None and pitched:
            i=min(pitched,key=lambda i:(abs(note-roots[i]),roots[i]))
            inc=0.5*2**((note-roots[i])/12)
            keys.append((i,note,round(65536*inc)))
        else:
            i=mapping[k] if mapping else k%len(roots)
            keys.append((i,-1,32768))   # unpitched: plays at its recorded pitch
    return keys

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('folder',type=Path,nargs='?');ap.add_argument('--placeholder',action='store_true')
    ap.add_argument('--manifest',type=Path,help='JSON list: file, start, end (s), label, root_hz, author, source, license; peaks normalized')
    ap.add_argument('--names',type=Path,help='JSON object: source WAV basename -> display label')
    ap.add_argument('--map',type=Path);ap.add_argument('--output',type=Path,default=ROOT/'firmware/src/dsp/sample_bank.h')
    a=ap.parse_args()
    if sum(map(bool,(a.folder,a.placeholder,a.manifest)))!=1:ap.error('give a WAV folder, --manifest or --placeholder')
    entries=None
    if a.placeholder:sounds=placeholder()
    elif a.manifest:
        entries=json.loads(a.manifest.read_text())
        if not 1<=len(entries)<=27:ap.error('manifest must list 1-27 sounds')
        for e in entries:
            missing={'file','label','root_hz','license','source'}-set(e)
            if missing:ap.error(f"{e.get('file')}: manifest entry lacks {sorted(missing)}")
        sounds=[(e['label'],read_wav(a.manifest.parent/e['file'],e.get('start'),e.get('end'),True),a.manifest.parent/e['file'],69+12*math.log2(e['root_hz']/440)) for e in entries]
    else:
        files=sorted((p for p in a.folder.iterdir() if p.suffix.lower()=='.wav'),key=lambda p:[int(t) if t.isdigit() else t.casefold() for t in re.split(r"(\d+)",p.name)])
        if not 1<=len(files)<=27:ap.error('folder must contain 1–27 WAV files')
        sounds=[(p.stem,read_wav(p),p,None) for p in files]
    if any(len(x)>=65535 for _,x,_,_ in sounds):ap.error('each sound must be under 65535 frames (about 2.9 s at 22.05 kHz)')
    size=sum(len(x)*2 for _,x,_,_ in sounds)
    if size>BUDGET:ap.error(f'bank is {size} bytes, budget {BUDGET}; shorten or use fewer sounds (about 7.4 seconds total)')
    mapping=None
    if a.map:
        names=[p.name if p else n for n,_,p,_ in sounds]; wanted=json.loads(a.map.read_text())
        if not isinstance(wanted,list) or len(wanted)!=27:ap.error('mapping must be a JSON list of 27 WAV basenames')
        mapping=[names.index(n) for n in wanted]
    keys=key_table([r for _,_,_,r in sounds],mapping)
    if any(inc>=65536 for _,_,inc in keys):ap.error('a key would play its sample more than an octave up; add a higher sample')
    labels=[(f'SOUND {int(name):02d}' if name.isdigit() else name).encode('ascii','replace').decode()[:20] for name,_,_,_ in sounds]
    if a.names:
        if a.placeholder:ap.error('--names is for imported recordings, not the test fixture')
        overrides=json.loads(a.names.read_text())
        expected={p.name for _,_,p,_ in sounds}
        if not isinstance(overrides,dict) or set(overrides)!=expected:ap.error('--names must contain exactly the imported WAV basenames')
        labels=[overrides[p.name] for _,_,p,_ in sounds]
        if any(not isinstance(label,str) or not 1<=len(label)<=20 or label!=label.strip() or any(ord(c)<32 or ord(c)>126 for c in label) for label in labels):ap.error('display labels must be 1–20 printable ASCII characters, without outer spaces')
        if len(set(labels))!=len(labels):ap.error('each unique sample needs a distinct display label')
    key_labels=[f'{note_name(note)} {labels[i]}'[:20] if note>=0 else labels[i] for i,note,_ in keys]
    lines=['/* SPDX-License-Identifier: GPL-3.0-only */','/* Generated by tools/import_samples.py. See sample_bank.json for audio provenance. */','#pragma once','#include <stdint.h>',f'#define FMO_PLACEHOLDER {int(a.placeholder)}',
      'typedef struct { const int16_t *data; uint32_t length; const char *name; } fmo_sample_t;',
      '/* Per key: the sample, the note it is tuned to (MIDI, -1: played as recorded), source frames per output frame (Q16) */',
      'typedef struct { uint8_t sample; int8_t note; uint32_t inc_q16; const char *name; } fmo_key_t;']
    for i,(_,data,_,_) in enumerate(sounds):
        lines.append(f'static const int16_t sample_{i}[] = {{')
        lines.extend(','.join(str(v) for v in data[j:j+24])+',' for j in range(0,len(data),24));lines.append('};')
    lines.append('static const fmo_sample_t fmo_samples[] = {')
    for i,(name,data,_,_) in enumerate(sounds):
        lines.append('{sample_%d,%du,%s},'%(i,len(data),json.dumps(labels[i])))
    lines+=['};','static const fmo_key_t fmo_keys[27] = {']
    lines+=['{%d,%d,%du,%s},'%(i,note,inc,json.dumps(key_labels[k])) for k,(i,note,inc) in enumerate(keys)]
    lines.append('};')
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text('\n'.join(lines)+'\n')
    lic='synthetic placeholder generated by tools/import_samples.py, CC0 1.0' if a.placeholder else 'USER TO PROVIDE SOURCE/LICENCE'
    prov=[{k:e[k] for k in ('author','source','license','start','end','root_hz','note') if k in e} for e in entries] if entries else [{'license':lic}]*len(sounds)
    manifest={'placeholder':a.placeholder,'sample_rate':RATE,'pcm_bytes':size,'budget_bytes':BUDGET,'mapping':[i for i,_,_ in keys],
      'keys':[{'key':k+1,'sample':i,'note':note,'inc_q16':inc,'label':key_labels[k]} for k,(i,note,inc) in enumerate(keys)],
      'samples':[{'name':name,'display_name':labels[i],'frames':len(data),'root_note':root,'source_file':p.name if p else None,'sha256':hashlib.sha256(p.read_bytes()).hexdigest() if p else None,**prov[i]} for i,(name,data,p,root) in enumerate(sounds)]}
    a.output.with_suffix('.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(f'{len(sounds)} sounds, {size} PCM bytes, 27 keys; '+('PLACEHOLDER MEOWS' if a.placeholder else 'manifest' if a.manifest else 'imported WAVs'))
if __name__=='__main__':
    try:main()
    except (ValueError,wave.Error,OSError) as e:sys.exit(str(e))
