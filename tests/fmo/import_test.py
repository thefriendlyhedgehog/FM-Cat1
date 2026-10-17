# SPDX-License-Identifier: GPL-3.0-only
import contextlib, io, importlib.util, tempfile, wave, struct, math, random, subprocess, sys, json, re
from pathlib import Path
root=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('bank',root/'tools/import_samples.py');bank=importlib.util.module_from_spec(spec);spec.loader.exec_module(bank)
def snr(a,b): return 10*math.log10(sum(v*v for v in a)/max(1,sum((u-v)**2 for u,v in zip(a,b))))

# Codec: the decoder reproduces the encoder's own reconstruction, a tone comes through clean, noise survives,
# and decoding can start mid-take (the sustain loop) from two samples of history.
rnd=random.Random(1)
tone=[round(12000*math.sin(2*math.pi*660*i/22050)*min(1,i/500)) for i in range(5000)]
noise=[round(rnd.gauss(0,4000)) for _ in range(3000)]
for x,floor in ((tone,40),(noise,15)):
    pairs=bank.adpcm_predictors(x);data,dec=bank.adpcm_encode(x,pairs)
    assert len(pairs)==16 and pairs[0]==(0,0) and len(data)==(len(x)+13)//14*8
    assert bank.adpcm_decode(data,len(x),pairs)==dec and snr(x,dec)>floor,snr(x,dec)
    ls=1234;assert bank.adpcm_decode(data,len(x),pairs,ls,dec[ls-1],dec[ls-2])==dec[ls:]
loud=[32767 if i%40<20 else -32768 for i in range(700)]    # full-scale square: clamps, never wraps
pairs=bank.adpcm_predictors(loud);_,dec=bank.adpcm_encode(loud,pairs);assert all(-32768<=v<=32767 for v in dec) and snr(loud,dec)>12

# Loop crossfade: the sample before le leads into ls the way ls - 1 does.
x=[round(8000*math.sin(i*0.37)) for i in range(3000)];y=bank.bake_loop(x,1000,2001)
c=bank.XFADE;assert y[:2001-c]==x[:2001-c] and y[2000]==x[999] and y[2001:]==x[2001:] and y[1990]!=x[1990]

with tempfile.TemporaryDirectory() as td:
    folder=Path(td)
    def write(name,freq,secs=0.3,rate=22050,ch=1):
        with wave.open(str(folder/name),'wb') as w:
            w.setparams((ch,2,rate,0,'NONE','not compressed'))
            w.writeframes(b''.join(struct.pack('<h',round(9000*math.sin(2*math.pi*freq*i/rate)))*ch for i in range(int(secs*rate))))
    write('a.wav',440);write('b.wav',523);write('stereo.wav',440,ch=2);write('fast.wav',440,rate=44100)
    for bad in ('stereo.wav','fast.wav'):
        try: bank.read_wav(folder/bad);assert False
        except ValueError: pass
    take=lambda f,loop=None:{'file':f,'source':f[0],'loop':loop}
    # One bank from note 60: anchors 61..85 every 3 semitones. a.wav sits on two anchors (stored once).
    anchors=[{'note':n,'takes':[take('a.wav',[2000,4000])] if i<2 else [take('b.wav'),take('a.wav',[2000,4000])]} for i,n in enumerate(range(61,87,3))]
    doc={'banks':[{'name':'CATS','note0':60,'anchors':anchors},{'name':'MORE','note0':48,'anchors':anchors}],
         'sources':[{'id':'a','license':'CC0 1.0'},{'id':'b','license':'CC0 1.0'}]}
    (folder/'banks.json').write_text(json.dumps(doc));out=folder/'bank.h'
    run=lambda *a:subprocess.run([sys.executable,str(root/'tools/import_samples.py'),'--banks',str(folder/'banks.json'),'--output',str(out),*a],capture_output=True,text=True)
    r=run();assert r.returncode==0,r.stderr;before=out.read_bytes()
    assert run().returncode==0 and out.read_bytes()==before            # deterministic
    h=before.decode();takes=re.search(r'cp_takes\[\] = \{\n(.*?)\n\};',h,re.S).group(1).splitlines()
    assert len(takes)==2 and '#define CP_NBANKS 2' in h                  # two distinct takes for 2 banks x 9 anchors
    assert takes[0].startswith('{0u,6615u,2000u,4000u,')                 # a.wav, its loop and its history
    meta=json.loads(out.with_suffix('.json').read_text());assert meta['banks'][0]['keys']=='C4-D6' and meta['banks'][1]['keys']=='C3-D5'
    keys=bank.key_table({'note0':60,'anchors':anchors})
    assert keys[0]==(0,round(32768*2**(-1/12))) and keys[1]==(0,32768) and keys[2]==(0,round(32768*2**(1/12))) and keys[26]==(8,round(32768*2**(1/12)))
    doc['banks'][0]['anchors'][0]['takes'][0]['loop']=[2000,2010]       # a loop under 64 samples: refused, output kept
    (folder/'banks.json').write_text(json.dumps(doc))
    assert run().returncode!=0 and out.read_bytes()==before
    doc['banks'][0]['anchors'][0]['takes'][0]['loop']=[2000,4000];doc['banks'][0]['name']='A NAME TOO LONG'
    (folder/'banks.json').write_text(json.dumps(doc));assert run().returncode!=0 and out.read_bytes()==before
    # Over budget: refused.
    doc['banks'][0]['name']='CATS';(folder/'banks.json').write_text(json.dumps(doc))
    bank.BUDGET=1000;sys.argv=['import_samples.py','--banks',str(folder/'banks.json'),'--output',str(out)]
    try:
        with contextlib.redirect_stderr(io.StringIO()): bank.main()
        assert False
    except SystemExit as e: assert e.code!=0 and out.read_bytes()==before
print('Importer: ADPCM round trip, tone/noise/clamp quality, mid-take decode, loop crossfade, WAV validation, '
      'anchors/key table, dedupe, deterministic output, refused imports keep the old bank, budget PASS')
