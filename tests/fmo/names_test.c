/* SPDX-License-Identifier: GPL-3.0-only */
#define main scenario_main
#include "../../host/omni_host.c"
#undef main
#include <assert.h>
int main(void) {
    static const char *const notes[]={"G4","G#4","A4","A#4","B4","C5","C#5","D5","D#5","E5","F5","F#5","G5","G#5",
        "A5","A#5","B5","C6","C#6","D6","D#6","E6","F6","F#6","G6","G#6","A6"};
    boot(); run_ms(50);
    assert(text_w(&FONT_XS,"REC 240 BPM 16/16")<=224); /* compact loop status fits */
    assert(FONT_S.h<=28);
    for(int k=0;k<27;k++) {
        const char *name=om_sample_name(k);size_t n=strlen(notes[k]);
        assert(!strncmp(name,notes[k],n) && name[n]==' ' && name[n+1]); /* "<note> <cat>" */
        assert(text_w(&FONT_S,name)<=224);
        for(const char *p=name;*p;p++)assert(*p>=' ' && *p<='~');
        for(int j=0;j<k;j++)assert(strcmp(name,om_sample_name(j)));
        printf("key %d: %s, width %d / 224 pixels\n",k+1,name,text_w(&FONT_S,name));
    }
    puts("PASS: 27 key labels name their note (G4..A6) and cat, are unique and fit; loop-status text fits.");
}
