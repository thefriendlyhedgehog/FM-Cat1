/* SPDX-License-Identifier: GPL-3.0-only */
/* Every key of every bank sounds at its note: render it dry, find the period over the part of the take whose pitch
 * is held on its anchor (the placeholder meows hold it from 20% to 65%), compare with the equal-tempered note.
 * Then sustain: a held key keeps sounding on its loop without clicks, and a release lets it end. */
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "omni.h"
#include FMO_SAMPLE_BANK_HEADER
#define MAXF 120000
static int32_t blk[512];
static double x[MAXF];
static double corr(int s, int e, int lag)
{
    double xy=0,xx=0,yy=0;
    for(int i=s;i+lag<e;i++){xy+=x[i]*x[i+lag];xx+=x[i]*x[i];yy+=x[i+lag]*x[i+lag];}
    return xy/sqrt(xx*yy+1e-9);
}
static void dry(int bank) {om_init();om_set_bank(bank);om_set(P_REVERB,0);om_set(P_DELAY,0);for(int i=0;i<200;i++)om_render(blk,256,4096);}
static int run(int frames) {int n=0;while(n<frames && n<MAXF){om_render(blk,256,4096);for(int i=0;i<256 && n<MAXF;i++)x[n++]=blk[2*i];}return n;}
int main(void)
{
    double worst=0;
    for(int b=0;b<CP_NBANKS;b++) for(int k=0;k<FMO_KEYS;k++) {
        const cp_bank_t *bk=&cp_banks[b]; const cp_key_t *key=&bk->keys[k];
        const cp_anchor_t *an=&cp_anchors[bk->anchor0+key->anchor];
        const cp_take_t *t=&cp_takes[cp_take_ref[an->ref]];
        int n=0,s,e,periods,lo,hi,best,note=bk->note0+k;
        double want,P,c0,c1,c2,lag,got,cents;
        assert(abs(note-an->note)<=1);                            /* anchors every 3 semitones: +-1 resampled */
        dry(b);om_trigger(k,127);om_release(k);                   /* a tap: plays the take once */
        while(n<MAXF){om_render(blk,256,4096);for(int i=0;i<256 && n<MAXF;i++)x[n++]=blk[2*i];if(!om_active_keys)break;}
        assert(!om_active_keys);
        s=(int)ceil(0.22*t->n*65536.0/key->inc_q16);e=(int)(0.63*t->n*65536.0/key->inc_q16);
        want=440.0*pow(2.0,(note-69)/12.0);P=44100.0/want;
        periods=(int)((e-s)/(2*P));if(periods>4)periods=4;assert(periods>=4);   /* the formants sweep: lags of more periods decorrelate */
        lo=(int)(periods*P*0.97);hi=(int)(periods*P*1.03)+1;best=lo;
        for(int L=lo;L<=hi;L++)if(corr(s,e,L)>corr(s,e,best))best=L;
        c0=corr(s,e,best-1);c1=corr(s,e,best);c2=corr(s,e,best+1);
        if(c1<=0.9)printf("bank %d key %d: correlation %.3f at lag %d (period %.1f, %d periods, window %d-%d of %d)\n",b,k+1,c1,best,P,periods,s,e,n);
        assert(c1>0.9);                                           /* clearly periodic */
        lag=best+0.5*(c0-c2)/(c0-2*c1+c2);
        got=periods*44100.0/lag;cents=1200*log2(got/want);
        if(fabs(cents)>fabs(worst))worst=cents;
        if(fabs(cents)>=5.0)printf("bank %s key %d %s: want %.2f Hz got %.2f Hz %+.2f cents\n",bk->name,k+1,om_sample_name(k),want,got,cents);
        assert(fabs(cents)<5.0);
    }
    printf("PITCH: %d banks x 27 keys within 5 cents of their notes (worst %+.2f) PASS\n",CP_NBANKS,worst);

    /* Sustain: hold key 5 for three times the take's length, then let go. */
    {
        const cp_bank_t *bk=&cp_banks[0]; const cp_key_t *key=&bk->keys[4];
        const cp_take_t *t=&cp_takes[cp_take_ref[cp_anchors[bk->anchor0+key->anchor].ref]];
        int natural=(int)((double)t->n*65536.0/key->inc_q16), n, end=0;
        double jump=0,body=0;
        assert(t->le>t->ls && t->ls>0);
        dry(0);om_trigger(4,127);n=run(3*natural);
        assert(om_active_keys==1u<<4);                            /* still sounding: it loops */
        for(int i=natural;i<n;i++){double r=0;for(int j=i-64;j<i;j++)r+=x[j]*x[j];body+=r>0;}
        assert(body>(n-natural)*0.99);                            /* and is never silent while held */
        for(int i=natural/4;i<n-1;i++){double d=fabs(x[i+1]-x[i]);if(d>jump)jump=d;}
        {   /* the loop seam is no bigger a step than the take's own steepest moment */
            double own=0; dry(0);om_trigger(4,127);om_release(4);int m=run(natural);
            for(int i=natural/4;i<m-1;i++){double d=fabs(x[i+1]-x[i]);if(d>own)own=d;}
            printf("SUSTAIN: largest step while looping %.0f, in the plain take %.0f\n",jump,own);
            assert(jump<=own*1.5);
        }
        dry(0);om_trigger(4,127);run(3*natural);om_release(4);
        while(om_active_keys && end<natural*2){run(256);end+=256;}
        assert(!om_active_keys);                                  /* the release lets it end */
        printf("SUSTAIN: held key loops %d frames past its natural %d, ends %d frames after release PASS\n",2*natural,natural,end);
    }
    /* Round robin: TEST B's anchors have two takes, struck in turn. */
    if(CP_NBANKS>1 && cp_anchors[cp_banks[1].anchor0].ntakes==2) {
        double e1=0,e2=0; int n;
        dry(1);om_trigger(1,127);om_release(1);n=run(4000);for(int i=0;i<n;i++)e1+=x[i]*(i<n?1:0);
        double a[4000];for(int i=0;i<4000;i++)a[i]=x[i];
        while(om_active_keys)run(256);
        om_trigger(1,127);om_release(1);run(4000);for(int i=0;i<4000;i++)e2+=fabs(x[i]-a[i]);
        assert(e2>1e5);                                           /* the second strike is the other take */
        (void)e1;
        puts("ROUND ROBIN: consecutive strikes of one key play different takes PASS");
    }
    /* Bend: +100 cents on key 2 of bank 0 raises it a semitone. */
    {
        int n,s,e,best,lo,hi,periods; double P,got,want;
        const cp_bank_t *bk=&cp_banks[0]; const cp_key_t *key=&bk->keys[1];
        const cp_take_t *t=&cp_takes[cp_take_ref[cp_anchors[bk->anchor0+key->anchor].ref]];
        dry(0);om_set_bend(100);om_trigger(1,127);om_release(1);n=run(MAXF);(void)n;
        s=(int)ceil(0.22*t->n*65536.0/(key->inc_q16*1.0594631));e=(int)(0.63*t->n*65536.0/(key->inc_q16*1.0594631));
        want=440.0*pow(2.0,(bk->note0+1+1-69)/12.0);P=44100.0/want;periods=(int)((e-s)/(2*P));if(periods>4)periods=4;
        lo=(int)(periods*P*0.97);hi=(int)(periods*P*1.03)+1;best=lo;
        for(int L=lo;L<=hi;L++)if(corr(s,e,L)>corr(s,e,best))best=L;
        got=periods*44100.0/best;
        printf("BEND: +100 cents: want %.1f Hz got %.1f Hz\n",want,got);
        assert(fabs(1200*log2(got/want))<15);
        om_set_bend(0);
    }
    puts("PITCH/SUSTAIN/ROUND ROBIN/BEND PASS");
}
