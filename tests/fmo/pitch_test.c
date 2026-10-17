/* SPDX-License-Identifier: GPL-3.0-only */
/* Every key sounds at its note: render it dry, find the period over the part of the sample whose pitch is held
 * on the root (the placeholder meows hold it from 20% to 65%), compare with the equal-tempered note. */
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include "omni.h"
#include FMO_SAMPLE_BANK_HEADER
#define MAXF 60000
static int32_t blk[512];
static double x[MAXF];
static double corr(int s, int e, int lag)
{
    double xy=0,xx=0,yy=0;
    for(int i=s;i+lag<e;i++){xy+=x[i]*x[i+lag];xx+=x[i]*x[i];yy+=x[i+lag]*x[i+lag];}
    return xy/sqrt(xx*yy+1e-9);
}
int main(void)
{
    double worst=0;
    for(int k=0;k<FMO_KEYS;k++) {
        const fmo_key_t *key=&fmo_keys[k]; const fmo_sample_t *smp=&fmo_samples[key->sample];
        int n=0,s,e,periods,lo,hi,best;
        double want,P,c0,c1,c2,lag,got,cents;
        assert(key->note==67+k);                                  /* G4..A6 */
        assert(key->inc_q16>=27554u && key->inc_q16<=38968u);     /* within -3..+3 semitones of a root */
        om_init();om_set(P_REVERB,0);om_set(P_DELAY,0);
        for(int i=0;i<200;i++)om_render(blk,256,4096);            /* settle the smoothed mixes at 0 */
        om_trigger(k,127);
        while(n<MAXF){om_render(blk,256,4096);for(int i=0;i<256 && n<MAXF;i++)x[n++]=blk[2*i];if(!om_active_keys)break;}
        assert(!om_active_keys);                                  /* the whole sound fits in the buffer */
        s=(int)ceil(0.22*smp->length*65536.0/key->inc_q16);e=(int)(0.63*smp->length*65536.0/key->inc_q16);
        want=440.0*pow(2.0,(key->note-69)/12.0);P=44100.0/want;
        periods=(int)((e-s)/(2*P));if(periods>16)periods=16;assert(periods>=4);
        lo=(int)(periods*P*0.97);hi=(int)(periods*P*1.03)+1;best=lo;
        for(int L=lo;L<=hi;L++)if(corr(s,e,L)>corr(s,e,best))best=L;
        c0=corr(s,e,best-1);c1=corr(s,e,best);c2=corr(s,e,best+1);
        assert(c1>0.9);                                           /* clearly periodic */
        lag=best+0.5*(c0-c2)/(c0-2*c1+c2);
        got=periods*44100.0/lag;cents=1200*log2(got/want);
        if(fabs(cents)>fabs(worst))worst=cents;
        printf("key %2d %-12s want %7.2f Hz got %7.2f Hz %+5.2f cents\n",k+1,key->name,want,got,cents);
        assert(fabs(cents)<5.0);
    }
    printf("PITCH: 27 keys tuned G4..A6 within 5 cents (worst %+.2f) PASS\n",worst);
}
