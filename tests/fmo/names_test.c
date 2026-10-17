/* SPDX-License-Identifier: GPL-3.0-only */
#define main scenario_main
#include "../../host/omni_host.c"
#undef main
#include <assert.h>
int main(void) {
    static const char NOTE[12][3]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    static char seen[27][24];
    boot(); run_ms(50);
    assert(text_w(&FONT_XS,"REC 240 BPM 16/16")<=224); /* compact loop status fits */
    assert(FONT_S.h<=28);
    for(int b=0;b<om_bank_count();b++) {
        om_set_bank(b);
        for(int k=0;k<27;k++) {
            const char *name=om_sample_name(k);char note[5];int n=om_bank_note0()+k;size_t len;
            snprintf(note,sizeof note,"%s%d",NOTE[n%12],n/12-1);len=strlen(note);
            assert(!strncmp(name,note,len) && name[len]==' ' && !strcmp(name+len+1,om_bank_name(b))); /* "<note> <bank>" */
            assert(text_w(&FONT_S,name)<=224);
            for(const char *p=name;*p;p++)assert(*p>=' ' && *p<='~');
            for(int j=0;j<k;j++)assert(strcmp(name,seen[j]));
            snprintf(seen[k],sizeof seen[k],"%s",name);
            if(k==0||k==26)printf("bank %d key %d: %s, width %d / 224 pixels\n",b+1,k+1,name,text_w(&FONT_S,name));
        }
    }
    om_set_bank(0);
    printf("PASS: %d banks x 27 key labels name their note and bank, are unique and fit; loop-status text fits.\n",om_bank_count());
}
