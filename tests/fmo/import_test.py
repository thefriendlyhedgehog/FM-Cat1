# SPDX-License-Identifier: GPL-3.0-only
import importlib.util, tempfile, wave, struct, math, subprocess, sys, json
from pathlib import Path
root=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('bank',root/'tools/import_samples.py');bank=importlib.util.module_from_spec(spec);spec.loader.exec_module(bank)
with tempfile.TemporaryDirectory() as td:
    folder=Path(td); wav=folder/'01-example.wav'
    def write(freq,width=2):
        with wave.open(str(wav),'wb') as w:
            w.setparams((2,width,44100,4410,'NONE','not compressed'))
            for i in range(4410):
                v=int(10000*math.sin(2*math.pi*freq*i/44100))
                w.writeframesraw(struct.pack('<hh',v,v) if width==2 else bytes([128,128]))
    write(440);x=bank.read_wav(wav);assert len(x)==2205 and x[0]==x[-1]==0
    low=sum(v*v for v in x)
    write(16000);high=sum(v*v for v in bank.read_wav(wav));assert high<low*.01
    write(440)
    out=folder/'bank.h'; subprocess.run([sys.executable,str(root/'tools/import_samples.py'),str(folder),'--output',str(out)],check=True)
    before=out.read_bytes();subprocess.run([sys.executable,str(root/'tools/import_samples.py'),str(folder),'--output',str(out)],check=True);assert out.read_bytes()==before
    meta=json.loads(out.with_suffix('.json').read_text());assert meta['mapping']==[0]*27 and not meta['placeholder']
    write(440,1)
    result=subprocess.run([sys.executable,str(root/'tools/import_samples.py'),str(folder),'--output',str(out)],capture_output=True)
    assert result.returncode!=0 and out.read_bytes()==before
    # Manifest: crop, normalize, tune each key to its nearest root (1760 Hz root: key 27 = A6 plays as recorded).
    write(440)
    (folder/'m.json').write_text(json.dumps([{'file':wav.name,'start':0.02,'end':0.08,'label':'CAT','root_hz':1760.0,'license':'CC0 1.0','source':'test'}]))
    subprocess.run([sys.executable,str(root/'tools/import_samples.py'),'--manifest',str(folder/'m.json'),'--output',str(out)],check=True)
    meta=json.loads(out.with_suffix('.json').read_text());keys=meta['keys']
    assert meta['samples'][0]['frames']==1323 and meta['samples'][0]['license']=='CC0 1.0'
    assert keys[26]['note']==93 and keys[26]['inc_q16']==32768 and keys[26]['label']=='A6 CAT' and keys[0]['inc_q16']==round(32768*2**(-26/12))
    (folder/'m.json').write_text(json.dumps([{'file':wav.name,'label':'CAT','root_hz':440.0}]))  # no licence/source
    assert subprocess.run([sys.executable,str(root/'tools/import_samples.py'),'--manifest',str(folder/'m.json'),'--output',str(out)],capture_output=True).returncode!=0
print('Importer: PCM validation, stereo downmix, resampling/antialiasing, fades, deterministic output, repeated mapping, failed import preservation, manifest crop/tuning/licence PASS')
