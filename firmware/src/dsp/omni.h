/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
#define FMO_KEYS 27
#define FMO_VOICES 8
enum { P_REVERB, P_DELAY, P_TIME, P_FEEDBACK, P_NPARAMS };
typedef struct { const char *name; int16_t lo, hi, def; } om_param_t;
const om_param_t *om_param_info(int p);
void om_init(void);
void om_set(int p, int value);
void om_trigger(int key, int velocity);
void om_release(int key);                  /* key up: a held meow leaves its sustain loop and plays its ending */
int om_bank_count(void);
int om_bank(void);
void om_set_bank(int bank);                /* main loop: new notes come from this bank */
const char *om_bank_name(int bank);
int om_bank_note0(void);                   /* MIDI note of key 1 in the current bank */
void om_set_bend(int cents);               /* main loop: pitch bend for every voice, -1200..1200 cents */
void om_panic(void);
void om_render(int32_t *out, uint32_t n, uint32_t gain_q12);
const char *om_sample_name(int key);
int om_placeholder(void);
extern volatile uint32_t om_active_keys, om_triggers, om_dropped;
int om_voices_active(void);               /* voices sounding (the CPU meter) */

/* The sequencer (seq.c): four tracks ("cats") of up to 64 steps, each step up to four notes. Live notes record into
 * the selected track, whose bank is the one PRESET shows and the keys play. Commands are consumed by the audio ISR. */
#define OM_TRACKS 4
#define OM_STEPS 64
#define OM_POLY 4
#define OM_NDIV 6                              /* DIV: 1/32 1/16T 1/16 1/8T 1/8 1/4 */
enum { OM_T_LEN, OM_T_DIV, OM_T_SWING, OM_T_BANK, OM_T_MUTE };
enum { OM_S_RAT, OM_S_COND, OM_S_MICRO };
/* a step's condition: 0 always, 1..99 a chance in %, OM_COND_AB + 0..8 plays on loop a of every b (1:2 2:2 1:3 2:3
 * 3:3 1:4 2:4 3:4 4:4), FILL / NOT FILL */
#define OM_COND_AB 100
#define OM_COND_FILL 109
#define OM_COND_NOFILL 110
typedef struct {                               /* 16 bytes */
    uint8_t key[OM_POLY];                      /* 0..26, 0xFF = empty */
    uint8_t vel[OM_POLY];
    uint8_t len[OM_POLY];                      /* held for this many quarter steps (then the take plays out) */
    uint8_t rat;                               /* ratchet: the notes play 1..4 times in equal parts of the step */
    uint8_t cond;
    int8_t micro;                              /* nudge, -32..31 64ths of the step */
    uint8_t pad;
} om_step_t;
typedef struct { om_step_t step[OM_STEPS]; uint8_t len, div, swing, bank, mute, pad[3]; } om_track_t;
void om_seq_toggle_play(void);
void om_seq_toggle_record(void);
void om_seq_clear(void);                       /* the selected track: empty, recording off */
void om_seq_set_tempo(int bpm);
void om_seq_select(int track);                 /* main loop: record into it; the keys play its bank */
int om_seq_selected(void);
void om_seq_set(int track, int what, int value);                      /* OM_T_* */
void om_seq_note(int track, int step, int key, int vel, int len);     /* add / change a note, vel 0 removes it */
void om_seq_step_set(int track, int step, int what, int value);       /* OM_S_* */
void om_seq_erase(int track, int step);        /* a step, or (step < 0) the whole track */
void om_seq_set_fill(int on);
const om_track_t *om_seq_track(int track);     /* read only (main loop: what the ISR plays) */
uint32_t om_seq_keys(int track, int step);     /* keys on a step, a bit per key */
/* The arpeggiator (seq.c): with it on, keys and MIDI notes feed it instead of sounding. */
enum { OM_ARP_UP, OM_ARP_DOWN, OM_ARP_UPDOWN, OM_ARP_DOWNUP, OM_ARP_PLAYED, OM_ARP_RANDOM, OM_ARP_CONVERGE,
       OM_ARP_DIVERGE, OM_ARP_PINKY, OM_ARP_THUMB, OM_ARP_CHORD, OM_ARP_NMODES };
enum { OM_A_ON, OM_A_RATE, OM_A_MODE, OM_A_OCT, OM_A_GATE, OM_A_LATCH };
typedef struct { uint8_t on, rate, mode, oct, gate, latch; } om_arp_t;   /* rate: a DIV; oct 1..3; gate 5..100 % */
void om_arp_set(int what, int value);
const om_arp_t *om_arp(void);                  /* read only (main loop) */
const char *om_div_name(int div);              /* "1/16" */
const char *om_arp_mode_name(int mode);        /* "UP" */
int om_take_seq_hit(int *key); /* UI consumes playback hits; never feeds them back into audio */
extern volatile uint8_t om_seq_playing, om_seq_recording, om_seq_step, om_seq_pos[OM_TRACKS];
extern volatile uint16_t om_seq_bpm;
extern volatile uint32_t om_seq_ticks, om_seq_hit_dropped;
