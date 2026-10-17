/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments (Felucca: step conditions and chance)
 * Copyright (C) 2026 isod89 and the SLOOP contributors (the shared-clock grid, LEN / DIV / SWING per track,
 * nudge, ratchets, record latency) */
/* Cat Piano's sequencer: four tracks ("cats"), each up to 64 steps of up to four notes. Included by omni.c (one unit:
 * it starts voices directly) and run in the audio ISR, a sample at a time.
 *
 * Time, after SLOOP 2.7.1 (seq.c trk_grid): one transport clock counts units of a sample at 1 BPM, so a beat is
 * BEAT_U units and every sample adds the tempo. Each track walks its own grid of DIV steps a beat over that clock
 * (odd steps late by its SWING), so polymeters stay in phase and a tempo change never drifts. A track's state is
 * advanced incrementally and re-derived from the clock when its DIV or SWING changes.
 *
 * A step's notes start at its NUDGE (-32..31 64ths of the step: early ones start in the step before), its RATCHET
 * plays them 1-4 times in equal parts, and its CONDITION (after Felucca 1.6.1 seq_cond_miss) is a chance, a:b of the
 * track's loops, FILL or NOT FILL. A note holds for its own length (quarter steps), so a sustained meow can end
 * before the rest of its chord; after that it plays out its take, as a key let go. */
#define BEAT_U 2646000u                /* units a beat: 60 s x 44100 samples at 1 BPM */
#define SEQ_NONE 0xFFFFFFFFu
#define REC_LAT 512u                   /* key to ear, samples (SLOOP seq.c): a note is recorded where it was heard */
#define COND_LOOPS 12u                 /* a:b counts loops modulo a multiple of 2, 3 and 4 */
static const uint8_t DIV_DEN[OM_NDIV] = {8, 6, 4, 3, 2, 1};       /* steps a beat: 1/32 1/16T 1/16 1/8T 1/8 1/4 */
static const uint8_t COND_A[9] = {1, 2, 1, 2, 3, 1, 2, 3, 4}, COND_B[9] = {2, 2, 3, 3, 3, 4, 4, 4, 4};
static om_track_t pat[OM_TRACKS];
static struct {
    uint32_t abs, into, slen, u, sw;   /* the grid: step abs, units into it, its length; plain step, swing */
    uint32_t pabs, pel, period;        /* the step playing: its abs, units since it started, its ratchet's part */
    uint32_t skip_abs, skip_keys;      /* keys recorded ahead into step skip_abs: they sounded live already */
    uint8_t idx, nidx, hits, rat, loop, first;
} trk[OM_TRACKS];
static uint32_t clk_beat, clk_pos, clk_units, rng = 0x2545F491u;
static volatile uint8_t seq_sel, seq_fill;
volatile uint8_t om_seq_pos[OM_TRACKS];
static struct { uint8_t on, trk, idx, slot; uint32_t t0, u; } rec[FMO_KEYS];   /* notes being recorded, by key */
const om_track_t *om_seq_track(int t) { return &pat[t >= 0 && t < OM_TRACKS ? t : 0]; }
int om_seq_selected(void) { return seq_sel; }
uint32_t om_seq_keys(int t, int i)
{
    uint32_t k, m = 0; const om_step_t *s;
    if (t < 0 || t >= OM_TRACKS || i < 0 || i >= OM_STEPS) return 0;
    s = &pat[t].step[i];
    for (k = 0; k < OM_POLY; k++) if (s->key[k] < FMO_KEYS) m |= 1u << s->key[k];
    return m;
}
static uint32_t step_len(uint32_t t, uint32_t abs) { return abs & 1u ? trk[t].u - trk[t].sw : trk[t].u + trk[t].sw; }
static uint32_t trk_len(uint32_t t) { return pat[t].len ? pat[t].len : 1u; }
/* the grid from the clock (PLAY, a DIV / SWING / LEN change): SLOOP's grid_den + grid_swing */
static void grid_derive(uint32_t t)
{
    uint32_t den = DIV_DEN[pat[t].div % OM_NDIV], u = BEAT_U / den, abs = clk_beat * den + clk_pos / u, frac = clk_pos % u;
    trk[t].u = u;
    trk[t].sw = (den == 2u || den == 4u || den == 8u) ? (uint32_t)(pat[t].swing > 100 ? 100 : pat[t].swing) * u / 200u : 0u;
    if (abs & 1u) {                    /* an odd step starts sw late */
        if (frac < trk[t].sw) { abs--; frac += u; }
        else frac -= trk[t].sw;
    }
    trk[t].abs = abs; trk[t].into = frac; trk[t].slen = step_len(t, abs);
    trk[t].idx = (uint8_t)(abs % trk_len(t)); trk[t].nidx = (uint8_t)((abs + 1u) % trk_len(t));
}
static void seq_reset(void)            /* PLAY: everything from the top */
{
    uint32_t t;
    clk_beat = clk_pos = clk_units = 0;
    for (t = 0; t < OM_TRACKS; t++) {
        grid_derive(t);
        trk[t].pabs = trk[t].skip_abs = SEQ_NONE; trk[t].skip_keys = 0; trk[t].hits = trk[t].rat = 0; trk[t].loop = 0;
        trk[t].first = 1;
    }
}
static void track_init(uint32_t t)
{
    uint32_t i;
    for (i = 0; i < OM_STEPS; i++) {
        om_step_t *s = &pat[t].step[i];
        s->key[0] = s->key[1] = s->key[2] = s->key[3] = 0xFF; s->rat = 1; s->cond = 0; s->micro = 0;
    }
}
static void seq_patterns_init(void)
{
    uint32_t t;
    for (t = 0; t < OM_TRACKS; t++) {
        track_init(t);
        pat[t].len = 16; pat[t].div = 2; pat[t].swing = 0; pat[t].bank = (uint8_t)(t % CP_NBANKS); pat[t].mute = 0;
    }
    seq_sel = 0; seq_fill = 0;
}
/* Felucca's seq_miss / seq_cond_miss: the step does not play */
static int step_miss(uint32_t t, const om_step_t *s)
{
    uint32_t c = s->cond;
    if (!c) return 0;
    if (c < 100u) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng % 100u >= c; }
    if (c == OM_COND_FILL) return !seq_fill;
    if (c == OM_COND_NOFILL) return seq_fill;
    if (c < OM_COND_AB + 9u) return trk[t].loop % COND_B[c - OM_COND_AB] != COND_A[c - OM_COND_AB] - 1u;
    return 0;
}
static void step_start(uint32_t t, uint32_t abs, uint32_t slen)
{
    uint32_t idx = abs % trk_len(t); const om_step_t *s = &pat[t].step[idx];
    if (idx == 0 && !trk[t].first) trk[t].loop = (uint8_t)((trk[t].loop + 1u) % COND_LOOPS);
    trk[t].first = 0;
    trk[t].pabs = abs; trk[t].pel = 0; trk[t].hits = 0;
    trk[t].rat = (uint8_t)(pat[t].mute || step_miss(t, s) ? 0 : s->rat >= 1 && s->rat <= 4 ? s->rat : 1);
    trk[t].period = slen / (trk[t].rat ? trk[t].rat : 1u);
    om_seq_pos[t] = (uint8_t)idx;
    if (t == seq_sel) { om_seq_step = (uint8_t)idx; om_seq_ticks++; }
}
static void step_hit(uint32_t t)       /* one part of the step playing: its notes */
{
    const om_step_t *s = &pat[t].step[trk[t].pabs % trk_len(t)];
    uint32_t k, skip = trk[t].hits == 0 && trk[t].skip_abs == trk[t].pabs ? trk[t].skip_keys : 0u, bpm = om_seq_bpm;
    for (k = 0; k < OM_POLY; k++) if (s->key[k] < FMO_KEYS && !(skip & (1u << s->key[k]))) {
        uint32_t units = (uint32_t)(s->len[k] ? s->len[k] : 1u) * (trk[t].u / 4u);
        if (trk[t].rat > 1u && units > trk[t].period * 3u / 4u) units = trk[t].period * 3u / 4u;
        start_voice(pat[t].bank, s->key[k], s->vel[k] ? s->vel[k] : 100, units / bpm + 1u, (int)t + 1);
        seq_report(s->key[k]);
    }
}
/* a sample of the transport: every track's notes due now, then the clock moves on */
static void seq_sample(void)
{
    uint32_t t, bpm = om_seq_bpm;
    for (t = 0; t < OM_TRACKS; t++) {
        const om_step_t *cur = &pat[t].step[trk[t].idx], *nxt = &pat[t].step[trk[t].nidx];
        uint32_t abs = trk[t].abs;
        if (nxt->micro < 0 && trk[t].pabs != abs + 1u) {          /* the next step, nudged early into this one */
            uint32_t ns = step_len(t, abs + 1u), early = (uint32_t)(-nxt->micro) * ns / 64u;
            if (trk[t].into + early >= trk[t].slen) step_start(t, abs + 1u, ns);
        }
        if (trk[t].pabs != abs && trk[t].pabs != abs + 1u &&
            trk[t].into >= (cur->micro > 0 ? (uint32_t)cur->micro * trk[t].slen / 64u : 0u))
            step_start(t, abs, trk[t].slen);
        if (trk[t].pabs != SEQ_NONE) {
            while (trk[t].hits < trk[t].rat && trk[t].pel >= trk[t].hits * trk[t].period) { step_hit(t); trk[t].hits++; }
            trk[t].pel += bpm;
        }
        trk[t].into += bpm;
        while (trk[t].into >= trk[t].slen) {
            uint32_t len = trk_len(t);
            trk[t].into -= trk[t].slen; trk[t].abs++; trk[t].slen = step_len(t, trk[t].abs);
            trk[t].idx = trk[t].nidx; trk[t].nidx = (uint8_t)(trk[t].nidx + 1u >= len ? 0u : trk[t].nidx + 1u);
        }
    }
    clk_units += bpm;
    clk_pos += bpm;
    if (clk_pos >= BEAT_U) { clk_pos -= BEAT_U; clk_beat++; }
}
/* a note played live while recording: into the selected track's step nearest to where it was heard (REC_LAT
 * earlier). A step that has not started yet will not play it again this time round: it sounded live. */
static void seq_record_on(int key, int vel)
{
    uint32_t t = seq_sel, abs = trk[t].abs, into = trk[t].into, lat = REC_LAT * om_seq_bpm, i, k;
    om_step_t *s;
    if (into < lat) {
        if (abs) { abs--; into += step_len(t, abs); }
        into = into > lat ? into - lat : 0u;
    } else into -= lat;
    if (into >= step_len(t, abs) / 2u) abs++;
    if (trk[t].pabs == SEQ_NONE || (int32_t)(abs - trk[t].pabs) > 0) {
        if (trk[t].skip_abs != abs) { trk[t].skip_abs = abs; trk[t].skip_keys = 0; }
        trk[t].skip_keys |= 1u << key;
    }
    i = abs % trk_len(t); s = &pat[t].step[i];
    for (k = 0; k < OM_POLY && s->key[k] != key; k++) {}
    if (k == OM_POLY) for (k = 0; k < OM_POLY && s->key[k] < FMO_KEYS; k++) {}
    if (k == OM_POLY) return;          /* four notes there already */
    s->key[k] = (uint8_t)key; s->vel[k] = (uint8_t)vel; s->len[k] = 4;
    rec[key].on = 1; rec[key].trk = (uint8_t)t; rec[key].idx = (uint8_t)i; rec[key].slot = (uint8_t)k;
    rec[key].t0 = clk_units; rec[key].u = trk[t].u;
}
static void seq_record_off(int key)    /* its key let go: the note's length, to the nearest quarter step */
{
    uint32_t q;
    om_step_t *s;
    if (!rec[key].on) return;
    rec[key].on = 0; s = &pat[rec[key].trk].step[rec[key].idx];
    if (s->key[rec[key].slot] != key) return;
    q = ((clk_units - rec[key].t0) / (rec[key].u / 8u) + 1u) / 2u;
    s->len[rec[key].slot] = (uint8_t)(q < 1u ? 1u : q > 255u ? 255u : q);
}
static void seq_note(int t, int i, int key, int vel, int len)   /* step edit: add, change or (vel 0) remove a note */
{
    om_step_t *s = &pat[t].step[i]; uint32_t k;
    for (k = 0; k < OM_POLY && s->key[k] != key; k++) {}
    if (k == OM_POLY && vel) for (k = 0; k < OM_POLY && s->key[k] < FMO_KEYS; k++) {}
    if (k == OM_POLY) return;
    if (!vel) { s->key[k] = 0xFF; return; }
    s->key[k] = (uint8_t)key; s->vel[k] = (uint8_t)(vel > 127 ? 127 : vel); s->len[k] = (uint8_t)(len < 1 ? 1 : len > 255 ? 255 : len);
}
static void seq_track_set(int t, int what, int v)
{
    om_track_t *p = &pat[t];
    if (what == OM_T_LEN) { p->len = (uint8_t)(v < 1 ? 1 : v > OM_STEPS ? OM_STEPS : v); grid_derive((uint32_t)t); }
    else if (what == OM_T_DIV || what == OM_T_SWING) {
        if (what == OM_T_DIV) p->div = (uint8_t)(v < 0 ? 0 : v >= OM_NDIV ? OM_NDIV - 1 : v);
        else p->swing = (uint8_t)(v < 0 ? 0 : v > 100 ? 100 : v);
        grid_derive((uint32_t)t);
        if (trk[t].pabs != SEQ_NONE) { trk[t].pabs = trk[t].abs; trk[t].hits = trk[t].rat; }   /* the new grid's next step is next */
    } else if (what == OM_T_BANK) p->bank = (uint8_t)(v < 0 ? 0 : v >= CP_NBANKS ? CP_NBANKS - 1 : v);
    else if (what == OM_T_MUTE) p->mute = (uint8_t)(v != 0);
}
static void seq_step_set(int t, int i, int what, int v)
{
    om_step_t *s = &pat[t].step[i];
    if (what == OM_S_RAT) s->rat = (uint8_t)(v < 1 ? 1 : v > 4 ? 4 : v);
    else if (what == OM_S_COND) s->cond = (uint8_t)(v < 0 || v > OM_COND_NOFILL ? 0 : v);
    else if (what == OM_S_MICRO) s->micro = (int8_t)(v < -32 ? -32 : v > 31 ? 31 : v);
}
