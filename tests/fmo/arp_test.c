/* SPDX-License-Identifier: GPL-3.0-only */
/* The arpeggiator: modes, octaves, timing stopped and on the transport's grid, gate, latch, recording. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "omni.h"
static int32_t out[512];
static unsigned frame;
static void run(unsigned n) {while(n){unsigned k=n>256?256:n;om_render(out,k,4096);n-=k;frame+=k;}}
/* the next n arp notes: keys (a chord's notes summed as bits when chord is set) and the frames they came at */
static void notes(int n,int *key,unsigned *at) {
    for(int i=0;i<n;i++) {
        int k; unsigned limit=200000;
        while(!om_take_seq_hit(&k) && limit--) run(1);
        assert(limit!=~0u);
        key[i]=k; at[i]=frame-1;
    }
}
static void expect(const int *want,int n) {
    int key[24]; unsigned at[24];
    notes(n,key,at);
    for(int i=0;i<n;i++) { if(key[i]!=want[i]) printf("note %d: key %d, wanted %d\n",i,key[i],want[i]); assert(key[i]==want[i]); }
}
static void fresh(int mode,int oct) {
    int k; om_init();om_set(P_REVERB,0);om_set(P_DELAY,0);
    om_arp_set(OM_A_ON,1);om_arp_set(OM_A_MODE,mode);om_arp_set(OM_A_OCT,oct);run(1);while(om_take_seq_hit(&k)){}
    frame=0;
}
static void hold(const int *keys,int n) {for(int i=0;i<n;i++)om_trigger(keys[i],100);}
int main(void) {
    static const int C[3]={4,0,7};     /* pressed in this order */
    om_init();assert(!om_arp()->on && om_arp()->rate==2 && om_arp()->mode==OM_ARP_UP && om_arp()->oct==1 && om_arp()->gate==50);
    om_trigger(3,100);run(1);assert(om_triggers==1); /* off: keys sound as they are pressed */
    /* Modes over C (keys 0 4 7), stopped at 120 BPM in sixteenths. */
    {static const int w[]={0,4,7,0,4,7};fresh(OM_ARP_UP,1);hold(C,3);expect(w,6);}
    {static const int w[]={7,4,0,7,4,0};fresh(OM_ARP_DOWN,1);hold(C,3);expect(w,6);}
    {static const int w[]={0,4,7,4,0,4,7};fresh(OM_ARP_UPDOWN,1);hold(C,3);expect(w,7);}
    {static const int w[]={7,4,0,4,7,4};fresh(OM_ARP_DOWNUP,1);hold(C,3);expect(w,6);}
    {static const int w[]={4,0,7,4,0,7};fresh(OM_ARP_PLAYED,1);hold(C,3);expect(w,6);}
    {static const int w[]={0,7,4,0,7,4};fresh(OM_ARP_CONVERGE,1);hold(C,3);expect(w,6);}
    {static const int w[]={4,7,0,4,7,0};fresh(OM_ARP_DIVERGE,1);hold(C,3);expect(w,6);}
    {static const int w[]={0,7,4,7,0,7};fresh(OM_ARP_PINKY,1);hold(C,3);expect(w,6);}
    {static const int w[]={0,4,0,7,0,4};fresh(OM_ARP_THUMB,1);hold(C,3);expect(w,6);}
    {int key[60];unsigned at[60],seen=0;fresh(OM_ARP_RANDOM,1);hold(C,3);notes(60,key,at);
     for(int i=0;i<60;i++){assert(key[i]==0||key[i]==4||key[i]==7);seen|=1u<<key[i];}assert(seen==(1u|1u<<4|1u<<7));}
    /* Octaves: 12 keys up; past the top key (26) a note is left out. */
    {static const int w[]={0,4,7,12,16,19,0};fresh(OM_ARP_UP,2);hold(C,3);expect(w,7);}
    {static const int w[]={20,24,20};static const int k[2]={20,24};fresh(OM_ARP_UP,3);hold(k,2);expect(w,3);}
    /* CHORD: all held together, an octave up each step through OCT. */
    {int key[6];unsigned at[6];fresh(OM_ARP_CHORD,2);hold(C,3);notes(6,key,at);
     assert(key[0]==0 && key[1]==4 && key[2]==7 && key[3]==12 && key[4]==16 && key[5]==19);
     assert(at[0]==at[2] && at[3]==at[5] && at[3]-at[0]==5513);}
    /* Timing, stopped: the first note at once, then every sixteenth from it; RATE 1/8T. */
    {int key[4];unsigned at[4];fresh(OM_ARP_UP,1);run(1000);frame=0;hold(C,3);notes(4,key,at);
     assert(at[0]==0 && at[1]==5513 && at[2]==11025 && at[3]==16538);}
    {int key[3];unsigned at[3];fresh(OM_ARP_UP,1);om_arp_set(OM_A_RATE,3);hold(C,3);notes(3,key,at);
     assert(at[1]-at[0]==7350 && at[2]-at[1]==7350);}
    /* Playing: on the grid. Pressed early in a step it plays at once, in its last quarter it waits for the grid. */
    {int key[2];unsigned at[2];fresh(OM_ARP_UP,1);om_seq_toggle_play();run(1);frame=1;
     run(3000);hold(C,1);notes(2,key,at);assert(at[0]==3001 && at[1]==5513);}
    {int key[2];unsigned at[2];fresh(OM_ARP_UP,1);om_seq_toggle_play();run(1);frame=1;
     run(5000);hold(C,1);notes(2,key,at);assert(at[0]==5513 && at[1]==11025);}
    /* Gate: each note held GATE % of its step, then its take plays out (~0.37 s here). */
    {static const int k[1]={0};fresh(OM_ARP_UP,1);om_arp_set(OM_A_RATE,5);om_arp_set(OM_A_GATE,100);hold(k,1);run(20000);
     assert(om_active_keys==1);
     fresh(OM_ARP_UP,1);om_arp_set(OM_A_RATE,5);om_arp_set(OM_A_GATE,10);hold(k,1);run(20000);assert(!om_active_keys);}
    /* Letting go ends it; LATCH keeps the notes, and the next chord replaces them. */
    {int k;fresh(OM_ARP_UP,1);hold(C,3);run(6000);for(int i=0;i<3;i++)om_release(C[i]);run(1);
     while(om_take_seq_hit(&k)){}run(30000);assert(!om_take_seq_hit(&k));}
    {static const int w[]={0,4,7,0},w2[]={9,12,9};static const int c2[2]={12,9};
     fresh(OM_ARP_UP,1);om_arp_set(OM_A_LATCH,1);hold(C,3);run(1);for(int i=0;i<3;i++)om_release(C[i]);expect(w,4);
     {int k;run(100);while(om_take_seq_hit(&k)){}}
     hold(c2,2);expect(w2,3);
     om_arp_set(OM_A_LATCH,0);for(int i=0;i<2;i++)om_release(c2[i]);run(100);{int k;while(om_take_seq_hit(&k)){}run(30000);assert(!om_take_seq_hit(&k));}}
    /* Recording: what the arp plays goes into the selected track, each note GATE long, never doubled. */
    {fresh(OM_ARP_UP,1);om_arp_set(OM_A_GATE,50);om_seq_toggle_record();run(1);hold(C,2);   /* keys 4 and 0 */
     run(5513*4-100);for(int i=0;i<2;i++)om_release(C[i]);om_seq_toggle_record();run(1);
     unsigned t0=om_triggers;
     assert(om_seq_keys(0,0)==1u && om_seq_keys(0,1)==1u<<4 && om_seq_keys(0,2)==1u && om_seq_keys(0,3)==1u<<4 && !om_seq_keys(0,4));
     assert(om_seq_track(0)->step[0].len[0]==2);
     om_arp_set(OM_A_ON,0);run(88200);assert(om_triggers==t0+4); /* the next pass plays them back, once each */
    }
    /* Off: keys sound again at once; the arp's notes are not cut by a key-up of the same key. */
    {fresh(OM_ARP_UP,1);om_arp_set(OM_A_RATE,5);om_arp_set(OM_A_GATE,100);hold(C,1);run(100);om_release(4);run(5000);
     assert(om_active_keys==1u<<4);
     om_arp_set(OM_A_ON,0);run(1);unsigned t=om_triggers;om_trigger(9,100);run(1);assert(om_triggers==t+1);}
    puts("ARP PASS: 10 modes + random, octaves (top key bound), chord, stopped / on-grid timing, rate, gate, release, latch, recording, off");
}
