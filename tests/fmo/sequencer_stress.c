/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "omni.h"
static int32_t out[512];
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (double)t.tv_sec+(double)t.tv_nsec/1e9;}
static void run(unsigned n){int k;om_render(out,n,4096);while(om_take_seq_hit(&k)) {}for(unsigned i=0;i<2*n;i++)assert(out[i]>=-8220836 && out[i]<=8220836);}
int main(void){
    om_init();om_seq_set_tempo(240);om_seq_toggle_record();run(1);
    for(int step=0;step<16;step++) {
        while(om_seq_step!=step)run(1);
        for(int k=0;k<4;k++)om_trigger(k*6,127);run(1);
    }
    om_seq_toggle_record();om_set(P_REVERB,100);om_set(P_DELAY,100);om_set(P_FEEDBACK,70);run(1);
    for(int step=0;step<16;step++)assert(om_seq_keys(0,step)==(1u|1u<<6|1u<<12|1u<<18));
    for(int t=1;t<OM_TRACKS;t++){for(int i=0;i<16;i++)for(int k=0;k<4;k++)om_seq_note(t,i,k*6+t,127,2);run(1);}
    double sum=0,max=0;unsigned frames=44100u*30u;
    while(frames){unsigned n=frames>256?256:frames;double t=now();run(n);double dt=now()-t;sum+=dt;if(dt>max)max=dt;frames-=n;}
    assert(om_dropped==0 && om_seq_hit_dropped==0);
    printf("Dense 240 BPM, 4 tracks x 4-note chords on every step, full reverb/delay, 30s: PASS; native host render %.3f%% of real time; max block %.3fms (256-frame budget 5.805ms). Not an FM-1 CPU measurement.\n",100*sum/30,1000*max);
}
