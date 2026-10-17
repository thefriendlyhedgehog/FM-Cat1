/* SPDX-License-Identifier: GPL-3.0-only */
/* Cat piano artwork: a sitting tabby that meows music notes. Integer canvas primitives, no bitmap assets.
 * Coordinates are local to the 240 x 120 scene strip. No DSP, delay or IRQ masking.
 * The FMO_PUFF* names are kept from Fart Machine One: a "puff" is now one floating note. */
#define FMO_PAPER RGB(251,244,222)
#define FMO_INK RGB(37,48,62)
#define CAT_FUR RGB(240,164,92)
#define CAT_STRIPE RGB(196,110,52)
#define CAT_CREAM RGB(253,226,186)
#define CAT_PINK RGB(238,138,140)
#define FMO_PUFF_MS 480u
#define FMO_ANIM_MS 40u
#define FMO_PUFFS 4u

/* Filled ellipse, scanline bounds using integers only. Sizes are small and fixed. */
static void cartoon_oval(int cx,int cy,int rx,int ry,uint16_t colour)
{
    int y,x=0;
    for(y=-ry;y<=0;y++) {
        while(x<rx && (x+1)*(x+1)*ry*ry+y*y*rx*rx<=rx*rx*ry*ry) x++;
        cv_rect(cx-x,cy+y,2*x+1,1,colour);
        if(y) cv_rect(cx-x,cy-y,2*x+1,1,colour);
    }
}
static void cartoon_outlined(int x,int y,int rx,int ry,uint16_t fill)
{
    cartoon_oval(x,y,rx,ry,FMO_INK);
    cartoon_oval(x,y,rx-2,ry-2,fill);
}
/* Filled triangle with its apex (ax,ay) above a flat base from (x0,by) to (x1,by). */
static void cartoon_ear(int ax,int ay,int x0,int x1,int by,uint16_t colour)
{
    int y;
    for(y=ay;y<=by;y++) {
        int l=ax+(x0-ax)*(y-ay)/(by-ay+1), r=ax+(x1-ax)*(y-ay)/(by-ay+1);
        cv_rect(l,y,r-l+1,1,colour);
    }
}
/* One note leaves the cat's mouth and drifts up and left, wobbling; odd variants are beamed pairs. */
static void cartoon_note(uint32_t age,uint32_t variant)
{
    static const int8_t WOBBLE[8]={0,2,3,2,0,-2,-3,-2};
    int phase=(int)(age/FMO_ANIM_MS);
    int x=104-phase*6, y=60-phase*3-(int)(variant%2u)*6+WOBBLE[(phase+(int)variant)&7];
    uint16_t ink=phase<8?(variant&1u?RGB(124,92,178):RGB(52,140,150)):RGB(196,196,206);
    cartoon_oval(x,y,4,3,ink);
    cv_rect(x+3,y-13,2,13,ink);
    if(variant&1u) {
        cartoon_oval(x+10,y-2,4,3,ink);
        cv_rect(x+13,y-15,2,13,ink);
        cv_rect(x+3,y-15,12,3,ink);
    } else {
        cv_line(x+4,y-13,x+9,y-8,ink);
        cv_line(x+5,y-13,x+10,y-8,ink);
    }
}
static void cartoon_cat(int meowing)
{
    int h=meowing?-2:0;
    cartoon_oval(146,113,42,4,RGB(225,215,187));
    /* Tail curling up behind the right side. */
    cartoon_outlined(178,104+h,12,7,CAT_FUR);
    cartoon_outlined(190,92+h,7,10,CAT_FUR);
    cartoon_outlined(193,78+h,6,8,CAT_FUR);
    cartoon_oval(194,73+h,4,3,CAT_STRIPE);
    /* Body, cream chest, side stripes, front paws. */
    cartoon_outlined(150,88+h,32,25,CAT_FUR);
    cartoon_oval(140,92+h,15,17,CAT_CREAM);
    cv_rect(168,76+h,10,3,CAT_STRIPE); cv_rect(170,86+h,9,3,CAT_STRIPE); cv_rect(168,96+h,10,3,CAT_STRIPE);
    cartoon_outlined(132,110+h,9,5,CAT_CREAM);
    cartoon_outlined(152,110+h,9,5,CAT_CREAM);
    cv_line(130,109+h,130,113+h,FMO_INK); cv_line(134,109+h,134,113+h,FMO_INK);
    cv_line(150,109+h,150,113+h,FMO_INK); cv_line(154,109+h,154,113+h,FMO_INK);
    /* Ears (outlined, pink inside), then the head over their bases. */
    cartoon_ear(114,10+h,106,128,40+h,FMO_INK);
    cartoon_ear(115,15+h,110,125,38+h,CAT_FUR);
    cartoon_ear(116,21+h,113,121,36+h,CAT_PINK);
    cartoon_ear(150,10+h,136,158,40+h,FMO_INK);
    cartoon_ear(149,15+h,139,155,38+h,CAT_FUR);
    cartoon_ear(148,21+h,143,152,36+h,CAT_PINK);
    cartoon_outlined(132,50+h,29,24,CAT_FUR);
    cartoon_oval(132,62+h,13,9,CAT_CREAM);
    cv_rect(129,28+h,2,9,CAT_STRIPE); cv_rect(124,30+h,2,7,CAT_STRIPE); cv_rect(134,30+h,2,7,CAT_STRIPE);
    /* Eyes: open and round, or happily squeezed shut mid-meow. */
    if(meowing) {
        cv_line(115,48+h,119,44+h,FMO_INK); cv_line(119,44+h,123,48+h,FMO_INK);
        cv_line(141,48+h,145,44+h,FMO_INK); cv_line(145,44+h,149,48+h,FMO_INK);
    } else {
        cartoon_oval(119,47+h,4,5,FMO_INK); cartoon_oval(145,47+h,4,5,FMO_INK);
        cartoon_oval(120,45+h,1,1,FMO_PAPER); cartoon_oval(146,45+h,1,1,FMO_PAPER);
    }
    cartoon_oval(132,56+h,3,2,CAT_PINK);
    if(meowing) {
        cartoon_oval(132,65+h,6,6,FMO_INK);
        cartoon_oval(132,68+h,4,2,CAT_PINK);
    } else {
        cv_line(132,58+h,129,62+h,FMO_INK); cv_line(132,58+h,135,62+h,FMO_INK);
    }
    /* Whiskers. */
    cv_line(118,58+h,96,54+h,FMO_INK); cv_line(118,61+h,96,63+h,FMO_INK);
    cv_line(146,58+h,168,54+h,FMO_INK); cv_line(146,61+h,168,63+h,FMO_INK);
}
