/* SPDX-License-Identifier: GPL-3.0-only */
/* Cat piano (from Fart Machine One): banks of ADPCM meows, each key resampled from its nearest pre-rendered
 * anchor, held keys sustain on a loop, + the FoMni plate + simple echo.
 * Single producer (UI), single consumer (audio IRQ). All DSP state belongs to render. */
#include "omni.h"
#include "fastmath.h"
#ifndef FMO_SAMPLE_BANK_HEADER
#define FMO_SAMPLE_BANK_HEADER "sample_bank.h"
#endif
#include FMO_SAMPLE_BANK_HEADER
#ifdef OM_HOST
#define OM_POOL
#else
#define OM_POOL __attribute__((section(".pool")))
#endif
#include "plate.h"
static const om_param_t params[P_NPARAMS] = {
    {"REVERB",0,100,15}, {"DELAY",0,100,0}, {"TIME ms",80,500,240}, {"REPEATS",0,70,25}
};
const om_param_t *om_param_info(int p) { return p >= 0 && p < P_NPARAMS ? &params[p] : 0; }
static volatile uint8_t bank;                 /* written by the main loop only; read when a voice starts */
static volatile float bend_mul = 1.0f;        /* the same: pitch bend as a frequency ratio */
int om_bank_count(void) { return CP_NBANKS; }
int om_bank(void) { return bank; }

const char *om_bank_name(int b) { return b >= 0 && b < CP_NBANKS ? cp_banks[b].name : ""; }
int om_bank_note0(void) { return cp_banks[bank].note0; }
void om_set_bend(int cents) { bend_mul = fm_exp2f((float)(cents < -1200 ? -1200 : cents > 1200 ? 1200 : cents) * (1.0f / 1200.0f)); }
/* "C#5 KITTENS": the note a key plays in the current bank, and the bank (main loop only: one buffer) */
const char *om_sample_name(int k)
{
    static const char NOTE[12][3] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    static char label[24];
    int n, i = 0; const char *p;
    if (k < 0 || k >= FMO_KEYS) return "";
    n = cp_banks[bank].note0 + k;
    for (p = NOTE[n % 12]; *p; p++) label[i++] = *p;
    label[i++] = (char)('0' + n / 12 - 1);
    label[i++] = ' ';
    for (p = cp_banks[bank].name; *p && i < 23; p++) label[i++] = *p;
    label[i] = 0;
    return label;
}
int om_placeholder(void) { return FMO_PLACEHOLDER; }
volatile uint32_t om_active_keys, om_triggers, om_dropped;
static volatile uint8_t voices_active;
int om_voices_active(void) { return voices_active; }
#define QN 128u
#define BARRIER() __asm__ volatile("" ::: "memory")
typedef struct { int type, a, b, c, d; } command_t;
static command_t queue[QN];
static volatile uint32_t qw, qr, panic_pending;
static int par[P_NPARAMS];
static float rev_mix, echo_mix, feedback, delay_frames;
/* A voice reads one take: pos in source frames (Q16), advanced by inc per output frame. The ADPCM decoder has
 * decoded up to sample n (h1 = s1 = its value, h2 = s0 = sample n - 1's); sample n + 1 is nibble j of the frame at
 * byte fo. hold: samples it still loops its take for (HOLD_LIVE: until its key goes up; 0: it plays out).
 * owner: 0 live, 1 + n track n of the sequencer (a key up ends only live notes). */
#define HOLD_LIVE 0xFFFFFFFFu
typedef struct { const cp_take_t *t; const int16_t *c; uint32_t pos, inc, age, fo, hold; int32_t n, h1, h2; float s0, s1; int j, key, owner, active; float gain; } voice_t;
/* Decode the voice's next sample (tools/import_samples.py adpcm_decode, bit for bit). */
static void adpcm_next(voice_t *v)
{
    const uint8_t *f = &cp_adpcm[v->fo];
    int b = f[1 + (v->j >> 1)], q = ((v->j & 1 ? b & 15 : b >> 4) ^ 8) - 8, p = f[0] >> 4;
    int32_t y = ((v->c[2 * p] * v->h1 + v->c[2 * p + 1] * v->h2 + 1024) >> 11) + q * (1 << (f[0] & 15));
    if (y > 32767) y = 32767; else if (y < -32768) y = -32768;
    v->h2 = v->h1; v->h1 = y; v->n++; v->s0 = v->s1; v->s1 = (float)y;
    if (++v->j == 14) { v->j = 0; v->fo += 8u; }
}
static uint8_t rr[sizeof cp_anchors / sizeof cp_anchors[0]];   /* round robin over each anchor's takes */
static voice_t voices[FMO_VOICES];
static uint32_t age;
#define ECHO_N 22052u
static float echo[ECHO_N] OM_POOL;
static uint32_t echo_pos;
volatile uint8_t om_seq_playing, om_seq_recording, om_seq_step;
volatile uint16_t om_seq_bpm;
volatile uint32_t om_seq_ticks, om_seq_hit_dropped;
static volatile uint32_t seq_epoch;
#define HIT_Q 64u
static struct { int key; uint32_t epoch; } seq_hits[HIT_Q];
static volatile uint32_t hit_w, hit_r;
int om_take_seq_hit(int *key) {
    while(hit_r != hit_w) {
        uint32_t r=hit_r,epoch; int k;
        BARRIER(); k=seq_hits[r%HIT_Q].key; epoch=seq_hits[r%HIT_Q].epoch;
        BARRIER(); hit_r=r+1u;
        if(epoch==seq_epoch) { *key=k; return 1; }
    }
    return 0;
}
static void seq_report(int key) {
    uint32_t w=hit_w;
    if(w-hit_r>=HIT_Q) {om_seq_hit_dropped++;return;}
    seq_hits[w%HIT_Q].key=key; seq_hits[w%HIT_Q].epoch=seq_epoch;
    BARRIER();hit_w=w+1u;
}
/* a voice for a note: a free one, else the oldest of the same owner (a track steals from itself first), else the
 * oldest of all */
static void start_voice(int bk,int key,int velocity,uint32_t hold,int owner) {
    uint32_t j;int s=-1,own=-1; const cp_key_t *k=&cp_banks[bk].keys[key];
    uint32_t a=cp_banks[bk].anchor0+k->anchor; const cp_anchor_t *an=&cp_anchors[a];
    voice_t *v;
    for(j=0;j<FMO_VOICES && s<0;j++) if(!voices[j].active) s=(int)j;
    if(s<0) {
        for(j=0;j<FMO_VOICES;j++) if(voices[j].owner==owner && (own<0 || voices[j].age<voices[own].age)) own=(int)j;
        for(s=0,j=1;j<FMO_VOICES;j++) if(voices[j].age<voices[s].age) s=(int)j;
        if(own>=0 && owner) s=own;
    }
    v=&voices[s];
    {uint32_t ti=cp_take_ref[an->ref+rr[a]%an->ntakes]; v->t=&cp_takes[ti]; v->c=cp_preds[ti];} rr[a]++;
    v->pos=0; v->inc=k->inc_q16; v->age=++age; v->n=-1; v->h1=v->h2=0; v->s0=v->s1=0.0f; v->fo=v->t->off; v->j=0;
    v->hold=hold; v->key=key; v->owner=owner; v->active=1; v->gain=(float)velocity*(1.0f/127.0f); om_triggers++;
}
#include "seq.c"
static void seq_stop(void) {
    uint32_t k;om_seq_playing=om_seq_recording=0;om_seq_step=0;seq_epoch++;seq_reset();
    for(k=0;k<OM_TRACKS;k++)om_seq_pos[k]=0;
    for(k=0;k<FMO_KEYS;k++)rec[k].on=0;
}
static void seq_start(void) { seq_reset(); om_seq_playing=1; }
static void post4(int type, int a, int b, int c, int d) {
    uint32_t w = qw;
    if (w - qr >= QN) { om_dropped++; return; }
    queue[w % QN] = (command_t){type,a,b,c,d};
    BARRIER(); qw = w + 1u;
}
static void post(int type, int a, int b) { post4(type,a,b,0,0); }
void om_set(int p, int v) { if (p >= 0 && p < P_NPARAMS) post(1,p,v); }
void om_trigger(int k, int v) { if (k >= 0 && k < FMO_KEYS && v > 0) post(0,k,v > 127 ? 127 : v); }
void om_release(int k) { if (k >= 0 && k < FMO_KEYS) post(6,k,0); }
void om_seq_toggle_play(void) {post(2,0,0);}
void om_seq_toggle_record(void) {post(3,0,0);}
void om_seq_clear(void) {post(4,0,0);}
void om_seq_set_tempo(int bpm) {post(5,bpm,0);}
void om_set_bank(int b) { if (b >= 0 && b < CP_NBANKS) { bank = (uint8_t)b; post4(8,seq_sel,OM_T_BANK,b,0); } }
void om_seq_select(int t) { if (t >= 0 && t < OM_TRACKS) { bank = pat[t].bank; post(7,t,0); } }
void om_seq_set(int t, int what, int v) { if (t >= 0 && t < OM_TRACKS) post4(8,t,what,v,0); }
void om_seq_note(int t, int i, int key, int vel, int len) {
    if (t >= 0 && t < OM_TRACKS && i >= 0 && i < OM_STEPS && key >= 0 && key < FMO_KEYS) post4(9,t,i,key,(vel & 255) | len << 8);
}
void om_seq_step_set(int t, int i, int what, int v) { if (t >= 0 && t < OM_TRACKS && i >= 0 && i < OM_STEPS) post4(10,t,i,what,v); }
void om_seq_erase(int t, int i) { if (t >= 0 && t < OM_TRACKS && i < OM_STEPS) post(11,t,i); }
void om_seq_set_fill(int on) { post(12,on,0); }
void om_panic(void) { panic_pending = 1; }
static void clear_sound(void) {
    uint32_t i;
    for (i=0;i<FMO_VOICES;i++) voices[i].active=0;
    for (i=0;i<ECHO_N;i++) echo[i]=0.0f;
    echo_pos=0; plate_init(); om_active_keys=0;
}
void om_init(void) {
    int i; qw=qr=panic_pending=0; age=om_triggers=om_dropped=0;
    for(i=0;i<P_NPARAMS;i++) par[i]=params[i].def;
    rev_mix=(float)par[P_REVERB]*0.01f; echo_mix=(float)par[P_DELAY]*0.01f;
    feedback=(float)par[P_FEEDBACK]*0.01f; delay_frames=(float)par[P_TIME]*44.1f;
    seq_epoch=hit_w=hit_r=om_seq_ticks=om_seq_hit_dropped=0;bank=0;
    om_seq_bpm=120;seq_patterns_init();seq_stop();
    clear_sound();
}
static float limit(float x) { return x > 0.98f ? 0.98f : x < -0.98f ? -0.98f : x; }
void om_render(int32_t *out, uint32_t n, uint32_t gain_q12) {
    uint32_t i,j,keys=0,inc[FMO_VOICES]; float master=(float)(gain_q12 > 4096 ? 4096 : gain_q12)*(1.0f/4096.0f),bend=bend_mul;
    if(panic_pending) { qr=qw; panic_pending=0; seq_stop(); clear_sound(); }
    while(qr != qw) {
        command_t c; BARRIER(); c=queue[qr % QN]; BARRIER(); qr++;
        if(c.type==1) par[c.a]=c.b<params[c.a].lo?params[c.a].lo:c.b>params[c.a].hi?params[c.a].hi:c.b;
        else if(c.type==0) {
            start_voice(bank,c.a,c.b,HOLD_LIVE,0);
            if(om_seq_recording)seq_record_on(c.a,c.b);
        } else if(c.type==6) {           /* key up: live voices of that key leave their loop and play out */
            for(j=0;j<FMO_VOICES;j++) if(voices[j].key==c.a && !voices[j].owner) voices[j].hold=0;
            seq_record_off(c.a);
        } else if(c.type==2) {
            if(om_seq_playing) {seq_stop();clear_sound();} else seq_start();
        } else if(c.type==3) {
            om_seq_recording=(uint8_t)!om_seq_recording;
            if(om_seq_recording && !om_seq_playing)seq_start();
        } else if(c.type==4) {         /* clear: the selected track */
            track_init(seq_sel);om_seq_recording=0;
            for(j=0;j<FMO_VOICES;j++) if(voices[j].owner==seq_sel+1) voices[j].hold=0;
        } else if(c.type==5) om_seq_bpm=(uint16_t)(c.a<40?40:c.a>240?240:c.a);
        else if(c.type==7) {seq_sel=(uint8_t)c.a;om_seq_step=om_seq_pos[c.a];}
        else if(c.type==8) seq_track_set(c.a,c.b,c.c);
        else if(c.type==9) seq_note(c.a,c.b,c.c,c.d&255,c.d>>8);
        else if(c.type==10) seq_step_set(c.a,c.b,c.c,c.d);
        else if(c.type==11) {if(c.b<0) track_init((uint32_t)c.a); else for(j=0;j<OM_POLY;j++) pat[c.a].step[c.b].key[j]=0xFF;}
        else if(c.type==12) seq_fill=(uint8_t)(c.a!=0);
    }
    for(j=0;j<FMO_VOICES;j++) {       /* the bend, once per block; under 1.0 so a frame decodes at most one sample */
        float f=(float)voices[j].inc*bend; inc[j]=f>65535.0f?65535u:(uint32_t)f;
    }
    for(i=0;i<n;i++) {
        float dry=0.0f,wl,wr,delayed,in,l,r; uint32_t off,at,next;
        if(om_seq_playing) seq_sample();
        rev_mix+=0.002f*((float)par[P_REVERB]*0.01f-rev_mix);
        echo_mix+=0.002f*((float)par[P_DELAY]*0.01f-echo_mix);
        feedback+=0.002f*((float)par[P_FEEDBACK]*0.01f-feedback);
        delay_frames+=0.0002f*((float)par[P_TIME]*44.1f-delay_frames);
        for(j=0;j<FMO_VOICES;j++) if(voices[j].active) {
            voice_t *v=&voices[j]; const cp_take_t *t=v->t; uint32_t a=v->pos>>16;
            if(a+1u>=t->n) {v->active=0;continue;}
            while(v->n<(int32_t)a+1) adpcm_next(v);
            dry+=(v->s0+(v->s1-v->s0)*(float)(v->pos&0xFFFFu)*(1.0f/65536.0f))*(1.0f/32768.0f)*v->gain*0.32f;
            v->pos+=inc[j];
            if(v->hold-1u<HOLD_LIVE-1u) v->hold--;   /* a sequenced note's length runs down */
            if(v->hold && t->le && (v->pos>>16)+1u>=t->le) {   /* sustain: back to ls, decoder state from there */
                v->pos-=(uint32_t)(t->le-t->ls)<<16;
                v->n=t->ls-1; v->h1=t->lh1; v->h2=t->lh2; v->s1=(float)t->lh1;
                v->fo=t->off+t->ls/14u*8u; v->j=(int)(t->ls%14u);
            }
        }
        off=(uint32_t)delay_frames; at=(echo_pos+ECHO_N-off)%ECHO_N; next=(at+ECHO_N-1u)%ECHO_N;
        delayed=echo[at]+(echo[next]-echo[at])*(delay_frames-(float)off);
        in=limit(dry+delayed*feedback); echo[echo_pos]=fm_flush(in);
        if(++echo_pos==ECHO_N) echo_pos=0;
        reverb(dry*0.5f,&wl,&wr);
        l=limit(dry+delayed*echo_mix+wl*rev_mix)*master;
        r=limit(dry+delayed*echo_mix+wr*rev_mix)*master;
        out[2*i]=(int32_t)(l*8388607.0f); out[2*i+1]=(int32_t)(r*8388607.0f);
    }
    for(i=j=0;j<FMO_VOICES;j++) if(voices[j].active) {keys|=1u<<voices[j].key;i++;}
    om_active_keys=keys; voices_active=(uint8_t)i;
}
