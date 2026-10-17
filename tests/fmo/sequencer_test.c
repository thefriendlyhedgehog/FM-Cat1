/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "omni.h"
static int32_t out[512];
static void run(unsigned frames) {
    while(frames) {unsigned n=frames>256?256:frames;om_render(out,n,4096);frames-=n;
        for(unsigned i=0;i<2*n;i++)assert(out[i]>=-8220836 && out[i]<=8220836);}
}
static void until_step(int step) {for(unsigned n=0;n<300000 && om_seq_step!=step;n++)run(1);assert(om_seq_step==step);}
/* frames from now until the selected track's step counter moves (or limit) */
static unsigned until_tick(unsigned limit) {unsigned n=0,t=om_seq_ticks;while(om_seq_ticks==t && n<limit){run(1);n++;}return n;}
static unsigned until_trigger(unsigned limit) {unsigned n=0,t=om_triggers;while(om_triggers==t && n<limit){run(1);n++;}return n;}
static void fresh(void) {om_init();for(int t=0;t<OM_TRACKS;t++)om_seq_erase(t,-1);run(1);}
int main(void) {
    int key,count;
    /* Transport and recording, as the one-track sequencer did them (now track 1 of 4). */
    om_init();assert(!om_seq_playing && !om_seq_recording && om_seq_bpm==120 && om_seq_selected()==0);
    om_seq_toggle_play();run(1);assert(om_seq_ticks==1 && om_seq_step==0);
    run(5512);assert(om_seq_step==0);run(1);assert(om_seq_step==1); /* first boundary frame 5513 */
    run(88201-5514);assert(om_seq_ticks==17 && om_seq_step==0); /* exact 2-second bar + first sample */
    om_init();om_seq_set_tempo(137);om_seq_toggle_play();run(2646001);
    assert(om_seq_ticks==549 && om_seq_step==4); /* no fractional drift after one minute */
    om_init();om_seq_set_tempo(240);om_seq_toggle_play();run(1);for(int i=0;i<10;i++)run(44100*60);
    assert(om_seq_ticks==9601 && om_seq_step==0); /* ten minutes at 240: 9600 sixteenths, not one more or less */
    om_init();om_seq_toggle_play();run(2756);om_seq_set_tempo(240);run(1379);
    assert(om_seq_step==0);run(1);assert(om_seq_step==1); /* tempo changes preserve fractional position */
    om_init();om_seq_toggle_record();run(1);
    until_step(15);run(3307);om_trigger(3,127);run(1);assert(om_seq_keys(0,0)==8);
    until_step(0);assert(om_triggers==1); /* end-of-bar quantization wraps and skips immediate echo */
    om_seq_toggle_record();run(1);run(88200);assert(om_triggers==2);
    om_init();om_seq_toggle_record();run(1);
    assert(om_seq_playing && om_seq_recording && om_seq_step==0);
    om_trigger(0,127);om_trigger(1,100);om_trigger(11,32);run(1);
    assert(om_seq_keys(0,0)==(3u|1u<<11) && om_triggers==3); /* a chord: up to four notes a step */
    run(3306);om_trigger(2,127);run(1);assert(om_seq_keys(0,1)==4);
    until_step(1);assert(om_triggers==4); /* quantized-ahead live hit is not doubled */
    om_seq_toggle_record();run(1);assert(!om_seq_recording && om_seq_playing);
    unsigned before=om_triggers;run(88200);assert(om_triggers==before+4);
    count=0;while(om_take_seq_hit(&key)){assert((key>=0 && key<=2) || key==11);count++;}assert(count==4);
    om_seq_toggle_play();run(1);assert(!om_seq_playing && !om_seq_recording && !om_active_keys);
    assert(om_seq_keys(0,0)==(3u|1u<<11) && om_seq_keys(0,1)==4);
    before=om_triggers;run(100000);assert(om_triggers==before); /* stopped means no sequenced audio */
    om_seq_toggle_play();run(1);assert(om_seq_step==0 && om_triggers==before+3);
    om_seq_set_tempo(500);run(1);assert(om_seq_bpm==240);
    om_seq_set_tempo(-5);run(1);assert(om_seq_bpm==40);
    om_seq_clear();run(1);assert(om_seq_playing && !om_seq_recording && om_seq_bpm==40); /* the track, not the transport */
    for(int i=0;i<OM_STEPS;i++)assert(om_seq_keys(0,i)==0);
    om_seq_toggle_play();run(1);
    assert(!om_take_seq_hit(&key)); /* old-transport animation events are discarded */
    om_trigger(7,127);run(1);assert(om_active_keys); /* live keys still work while stopped */
    om_panic();run(1);assert(!om_active_keys && !om_seq_playing);
    om_init();assert(om_seq_bpm==120);for(int t=0;t<OM_TRACKS;t++)for(int i=0;i<OM_STEPS;i++)assert(om_seq_keys(t,i)==0);
    for(int t=0;t<OM_TRACKS;t++)assert(om_seq_track(t)->len==16 && om_seq_track(t)->div==2);

    /* Note lengths: a sequenced meow holds for its length, then plays out its take (~0.37 s here). */
    fresh();om_seq_note(0,0,0,127,16);om_seq_toggle_play();run(1); /* 4 steps: 0.5 s */
    run(44100*45/100);assert(om_active_keys==1);run(44100*45/100);assert(!om_active_keys);
    fresh();om_seq_note(0,0,0,127,4);om_seq_toggle_play();run(1); /* 1 step: shorter than the take */
    run(44100*45/100);assert(!om_active_keys);
    /* A live key-up ends only the live meow of that key, never the sequenced one (and the reverse). */
    fresh();om_seq_set(0,OM_T_LEN,64);om_seq_note(0,0,5,127,64);om_seq_toggle_play();run(1000);
    om_trigger(5,127);run(100);om_release(5);run(44100*8/10);assert(om_active_keys==1u<<5); /* 2 s note sounds on */
    run(44100*18/10);assert(!om_active_keys); /* held 2 s, then its tail */
    fresh();om_seq_set(0,OM_T_LEN,64);om_trigger(5,127);run(100);om_seq_note(0,0,5,127,1);om_seq_toggle_play();run(44100);
    assert(om_active_keys==1u<<5);om_release(5);run(44100);assert(!om_active_keys); /* the live one held on */

    /* Polymeter and DIV: track 2 is 12 steps, track 3 eighths; all from one clock. */
    fresh();om_seq_set(1,OM_T_LEN,12);om_seq_set(2,OM_T_DIV,4);om_seq_toggle_play();run(1);
    run(5513*12-1);assert(om_seq_pos[0]==12 && om_seq_pos[1]==0 && om_seq_pos[2]==6);
    run(88200*3-5513*12+1);assert(om_seq_pos[0]==0 && om_seq_pos[1]==0 && om_seq_pos[2]==8); /* 48 sixteenths: together */
    /* Swing 50: odd sixteenths start a quarter of a step late (MPC 62.5 %). */
    fresh();om_seq_set(0,OM_T_SWING,50);om_seq_toggle_play();run(1);
    {unsigned a=until_tick(20000),b=until_tick(20000);assert(a==6891 && b==4134);} /* 1.25 + 0.75 steps */
    /* Nudge: step 2 a quarter early, step 3 a quarter late; ratchet: four parts of a step. */
    fresh();om_seq_note(0,1,0,127,1);om_seq_step_set(0,1,OM_S_MICRO,-16);
    om_seq_note(0,2,1,127,1);om_seq_step_set(0,2,OM_S_MICRO,16);om_seq_toggle_play();run(1);
    {unsigned a=until_trigger(20000),b=until_trigger(20000);assert(a==4135 && b==8269);}  /* 0.75 step, then 2.25 */
    fresh();om_seq_note(0,0,0,127,4);om_seq_note(0,0,3,127,4);om_seq_step_set(0,0,OM_S_RAT,4);om_seq_toggle_play();run(1);
    assert(om_triggers==2);
    {unsigned a=until_trigger(5000),b=until_trigger(5000),c=until_trigger(5000);assert(a==1379 && b==1378 && c==1378);}
    run(1000);assert(om_triggers==8);
    /* Conditions: 1:2 plays every other loop, 2:4 the second of four, FILL only when filling; mute silences. */
    fresh();om_seq_set(0,OM_T_LEN,1);om_seq_note(0,0,0,127,1);om_seq_step_set(0,0,OM_S_COND,OM_COND_AB+0);
    om_seq_toggle_play();run(1);
    run(5513*8);assert(om_triggers==5); /* loops 0, 2, 4, 6, 8 of 0..8 */
    fresh();om_seq_set(0,OM_T_LEN,1);om_seq_note(0,0,0,127,1);om_seq_step_set(0,0,OM_S_COND,OM_COND_AB+6);
    om_seq_toggle_play();run(1);run(5513*8-1);assert(om_triggers==2);  /* loops 1 and 5 of 0..8 */
    fresh();om_seq_set(0,OM_T_LEN,1);om_seq_note(0,0,0,127,1);om_seq_step_set(0,0,OM_S_COND,OM_COND_FILL);
    om_seq_toggle_play();run(5513*4);assert(om_triggers==0);om_seq_set_fill(1);run(5513*4);assert(om_triggers==4);
    om_seq_step_set(0,0,OM_S_COND,OM_COND_NOFILL);run(5513*4);assert(om_triggers==4);
    fresh();om_seq_set(0,OM_T_LEN,1);om_seq_note(0,0,0,127,1);om_seq_step_set(0,0,OM_S_COND,50);
    om_seq_toggle_play();run(1);run(5513*999);assert(om_triggers>400 && om_triggers<600); /* a 50 % chance */
    fresh();om_seq_set(0,OM_T_LEN,1);om_seq_note(0,0,0,127,1);om_seq_set(0,OM_T_MUTE,1);
    om_seq_toggle_play();run(5513*4);assert(om_triggers==0);om_seq_set(0,OM_T_MUTE,0);run(5513*4);assert(om_triggers==4);

    /* Recording: into the selected track, with the length it was held (quarter steps), in the track's bank. */
    fresh();om_seq_select(2);run(1);assert(om_seq_selected()==2 && om_bank()==om_seq_track(2)->bank);
    om_seq_toggle_record();run(1);om_trigger(4,90);run(5513*2);om_release(4);run(1);
    assert(om_seq_keys(2,0)==1u<<4 && om_seq_keys(0,0)==0);
    {const om_step_t *s=&om_seq_track(2)->step[0];int k=s->key[0]==4?0:s->key[1]==4?1:s->key[2]==4?2:3;
     assert(s->vel[k]==90 && s->len[k]==8);}
    om_set_bank(1);run(1);assert(om_seq_track(2)->bank==1 && om_bank()==1); /* PRESET: the selected track's cat */
    fresh();om_seq_toggle_record();run(3000);om_trigger(9,127);run(1);   /* past half a step, but heard 512 earlier */
    assert(om_seq_keys(0,0)==1u<<9 && om_seq_keys(0,1)==0);
    run(300);om_trigger(10,127);run(1);assert(om_seq_keys(0,1)==1u<<10);   /* 3301 - 512 is past half: the next */
    /* Voices: a track steals from itself first; 4 tracks of 2-note chords fit 8 voices. */
    fresh();for(int t=0;t<OM_TRACKS;t++){om_seq_note(t,0,t*2,127,64);om_seq_note(t,0,t*2+1,127,64);}
    om_seq_toggle_play();run(2000);assert(om_voices_active()==8);
    om_seq_note(0,1,20,127,64);om_seq_note(0,1,21,127,64);run(5600);
    assert(om_voices_active()==8 && (om_active_keys & 0xFFu)==0xFCu && (om_active_keys & 3u<<20)==3u<<20); /* keys 0, 1 went */
    puts("SEQ DSP PASS: exact sample clock (1 and 10 minutes), transport, quantized recording without doubles, chords, "
         "track clear, note lengths, live vs sequenced voices, polymeter, DIV, swing, nudge, ratchets, a:b / fill / chance, "
         "mute, recorded lengths, track banks, voice stealing");
}
