/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal
 * FM-1 Again changes: Copyright (C) 2026 PolarPatch */
/* FM-1 Again: the app. Started from fm1-base's template (its MIDI in, settings, Bluetooth, screen reader and
 * velocity handling kept); the sound is FM-1 Again's own engine (engine/, dsp/: GPL-3.0, so the firmware as a whole
 * is GPL-3.0): a groove of synthesized drums with BUILD / DROP, chords on KEYS, VOX's chopped syllables, the PUMP
 * over all of it and SPACE after it. The keys play KEYS, VOX or the DRUMS (ALGORITHM chooses). Pages (black and
 * white, big type): HOME, GROOVE (SEQ), KEYS (EDIT), VOX (ENV), PUMP (LFO), SPACE (FX), SETUP (GLO); K1-K4 their
 * four values along the bottom, PRESET the page's choice. PLAY starts and stops; ARP builds and drops.
 * Part of the unity build (device/unity.c, host/plat_host.c), after the platform: gfx.c and announce.c are in
 * scope. */
#include "fastmath.h"
#include "gen/again_font.h"   /* TeX Gyre Heros Bold, plain and stamped (tools/gen_again_font.py) */
#include "dsp/qdsp.h"
#include "dsp/fdsp.h"
#include "dsp/space.h"
#include "dsp/echo.h"
#include "dsp/tape.h"
#include "engine/clock.h"
#include "engine/drums.h"
#include "engine/keys.h"
#include "engine/vox.h"
#include "engine/chop.h"
#include "gen/again_chop.h"   /* CHOP's factory sample (tools/again_chop.py, tests/factory_chop.txt) */
#include "engine/pump.h"
#include "engine/rise.h"
#include "engine/perform.h"
#include "engine/groove.h"
#include "engine/revecho.h"
#include "wave.h"
#include "saver.h"

/* ------------------------------------------------------------- parameters --- */
/* every value a knob turns, 0 .. its top; the settings keep them */
enum {
    PA_TEMPO, PA_SWING, PA_KICK, PA_HATS, PA_CLAP,          /* the groove */
    PA_TONE, PA_DECAY, PA_PROG, PA_RHYTHM, PA_CHORD,         /* KEYS */
    PA_VOWELS, PA_GATE, PA_FORMANT, PA_BREATH,               /* VOX */
    PA_DEPTH, PA_RELEASE, PA_SHAPE, PA_SOURCE,               /* PUMP */
    PA_SIZE, PA_DAMP, PA_MIX, PA_SHIMMER,                    /* SPACE */
    PA_FILTER, PA_KEY, PA_BUILD, PA_KEYS_LV, PA_VOX_LV, PA_PAT,
    PA_SLICE, PA_LENGTH, PA_PITCH, PA_CMODE,                 /* CHOP */
    PA_ETIME, PA_EFB, PA_EWEAR, PA_EMIX, PA_EPING,           /* ECHO */
    PA_TWOW, PA_TSAT, PA_TTONE, PA_THISS, PA_TAPE,           /* TAPE */
    PA_RLEN, PA_RBRIGHT, PA_RSUCK, PA_RAUTO,                 /* RISE */
    PA_VARY, PA_BSTYLE, PA_TEMPOUP, PA_PLAYMODE, PA_PADS, PA_RISE_LV, PA_SAVER, PA_CREV,
    NPA
};
enum { FMT_PCT, FMT_BPM, FMT_LIST, FMT_MS, FMT_KEY, FMT_BARS, FMT_FILTER, FMT_SIZE, FMT_RAW, FMT_SEMI, FMT_ONE, FMT_UP };
typedef struct {
    const char *name;                            /* on the screen (and to a screen reader) */
    uint8_t top, def, fmt;
    const char *const *list;                     /* FMT_LIST's names */
} param_t;
static const char *const BARS_NAME[] = {"2", "4", "8"};
static const char *const ETIME_NAME[6] = {"1/16", "1/8", "3/16", "1/4", "3/8", "1/2"};
static const uint8_t ETIME_16TH[6] = {1, 2, 3, 4, 6, 8};
static const char *const EMODE_NAME[3] = {"TAPE", "PING-PONG", "REVERSE"};
static const char *const PLAYMODE_NAME[2] = {"LOOP", "SONG"};
static const char *const PADS_NAME[2] = {"NOTES", "PADS"};
static const char *const SAVER_NAME[2] = {"OFF", "ON"};
static const char *const CREV_NAME[2] = {"FORWARD", "REVERSE"};
/* the black keys as pads (loaf-breaker's way): PERFORM's tricks while held, F#3 .. F#5, and their names on the strip */
#define NPAD 11
static const uint8_t PAD_TRICK[NPAD] = {PF_STUT8, PF_STUT16, PF_STUT32, PF_REVERSE, PF_TAPESTOP, PF_GATE, PF_LOWSWEEP,
                                        PF_ECHOTHROW, PF_SPACETHROW, PF_KICKOUT, PF_DRUMSOUT};
static const char *const PAD_SHORT[NPAD] = {"R8", "R16", "R32", "REV", "STP", "GAT", "FLT", "ECH", "SPC", "KIL", "DRM"};
/* TAPE's presets: WOW, WARMTH, DULL, HISS */
enum { NTP = 5 };
static const char *const TP_NAME[NTP] = {"CLEAN", "WARM", "CASSETTE", "WORN", "MEMORY"};
static const uint8_t TP_SET[NTP][4] = {{0, 0, 0, 0}, {12, 30, 20, 8}, {30, 45, 45, 22}, {60, 70, 60, 35},
                                       {100, 55, 85, 30}};
static const char *const KEY_NAME[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const char *const PAT_NAME[GR_NPAT] = {"HOUSE", "2-STEP", "GARAGE 4X4", "SHUFFLE", "HALFTIME", "BROKEN",
                                              "MINIMAL", "KICK ONLY"};
static const param_t PA[NPA] = {
    [PA_TEMPO] = {"TEMPO", 100, 44, FMT_BPM, 0},            /* 80 .. 180 BPM */
    [PA_SWING] = {"SWING", 100, 67, FMT_PCT, 0},
    [PA_KICK] = {"KICK", 100, 85, FMT_PCT, 0},
    [PA_HATS] = {"HATS", 100, 65, FMT_PCT, 0},
    [PA_CLAP] = {"CLAP", 100, 75, FMT_PCT, 0},
    [PA_TONE] = {"TONE", 100, 45, FMT_PCT, 0},
    [PA_DECAY] = {"DECAY", 100, 55, FMT_PCT, 0},
    [PA_PROG] = {"CHORDS", GR_NPROG - 1, 1, FMT_LIST, PROG_NAME},
    [PA_RHYTHM] = {"PLAYS", NCR - 1, CR_HOLD, FMT_LIST, CR_NAME},
    [PA_CHORD] = {"ONE KEY", NCH - 1, CH_MIN9, FMT_LIST, CH_NAME},
    [PA_VOWELS] = {"VOWELS", VX_NSET - 1, 0, FMT_LIST, VX_SET_NAME},
    [PA_GATE] = {"GATE", 100, 25, FMT_MS, 0},               /* 40 .. 640 ms */
    [PA_FORMANT] = {"SHIFT", 100, 50, FMT_PCT, 0},
    [PA_BREATH] = {"BREATH", 100, 20, FMT_PCT, 0},
    [PA_DEPTH] = {"PUMP", 100, 70, FMT_PCT, 0},
    [PA_RELEASE] = {"TIME", 100, 30, FMT_MS, 0},          /* 60 .. 660 ms */
    [PA_SHAPE] = {"SHAPE", 100, 50, FMT_PCT, 0},
    [PA_SOURCE] = {"SOURCE", NPS - 1, PS_KICK, FMT_LIST, PS_NAME},
    [PA_SIZE] = {"SIZE", 127, 52, FMT_SIZE, 0},
    [PA_DAMP] = {"DAMP", 127, 60, FMT_PCT, 0},
    [PA_MIX] = {"SPACE", 127, 70, FMT_PCT, 0},
    [PA_SHIMMER] = {"SHINE", 127, 0, FMT_PCT, 0},
    [PA_FILTER] = {"FILTER", 100, 50, FMT_FILTER, 0},
    [PA_KEY] = {"KEY", 11, 9, FMT_KEY, 0},
    [PA_BUILD] = {"BUILD", 2, 1, FMT_BARS, 0},
    [PA_KEYS_LV] = {"KEYS", 100, 70, FMT_PCT, 0},
    [PA_VOX_LV] = {"VOICE", 100, 70, FMT_PCT, 0},            /* VOX and CHOP */
    [PA_PAT] = {"GROOVE", GR_NPAT - 1, 1, FMT_LIST, PAT_NAME},
    [PA_SLICE] = {"SLICE", CH_MAX_SLICES - 1, 0, FMT_ONE, 0},
    [PA_LENGTH] = {"LENGTH", 100, 100, FMT_PCT, 0},
    [PA_PITCH] = {"PITCH", 24, 12, FMT_SEMI, 0},             /* -12 .. +12 */
    [PA_CMODE] = {"MODE", NCM - 1, CM_SLICES, FMT_LIST, CM_NAME},
    [PA_ETIME] = {"TIME", 5, 2, FMT_LIST, ETIME_NAME},
    [PA_EFB] = {"AGAIN", 127, 60, FMT_PCT, 0},
    [PA_EWEAR] = {"WEAR", 127, 45, FMT_PCT, 0},
    [PA_EMIX] = {"ECHO", 127, 40, FMT_PCT, 0},
    [PA_EPING] = {"MODE", 2, 1, FMT_LIST, EMODE_NAME},       /* TAPE, PING-PONG, REVERSE */
    [PA_TWOW] = {"WOW", 127, 12, FMT_PCT, 0},
    [PA_TSAT] = {"WARMTH", 127, 30, FMT_PCT, 0},
    [PA_TTONE] = {"DULL", 127, 20, FMT_PCT, 0},
    [PA_THISS] = {"HISS", 127, 8, FMT_PCT, 0},
    [PA_TAPE] = {"TAPE", NTP - 1, 1, FMT_LIST, TP_NAME},
    [PA_RLEN] = {"LENGTH", 4, 2, FMT_LIST, RS_LEN_NAME},
    [PA_RBRIGHT] = {"BRIGHT", 100, 60, FMT_PCT, 0},
    [PA_RSUCK] = {"SUCK", 100, 50, FMT_PCT, 0},
    [PA_RAUTO] = {"AUTO", NRA - 1, RA_DROP, FMT_LIST, RA_NAME},
    [PA_VARY] = {"VARY", 100, 25, FMT_PCT, 0},
    [PA_BSTYLE] = {"STYLE", NBS - 1, BS_SNARE, FMT_LIST, BS_NAME},
    [PA_TEMPOUP] = {"RUSH", 20, 0, FMT_UP, 0},          /* + 0 .. 20 BPM at the drop */
    [PA_PLAYMODE] = {"PLAY", 1, 0, FMT_LIST, PLAYMODE_NAME},
    [PA_PADS] = {"BLACK", 1, 1, FMT_LIST, PADS_NAME},
    [PA_RISE_LV] = {"RISE", 100, 80, FMT_PCT, 0},
    [PA_SAVER] = {"SAVER", 1, 1, FMT_LIST, SAVER_NAME},   /* the screensaver after a minute untouched */
    [PA_CREV] = {"REVERSE", 1, 0, FMT_LIST, CREV_NAME},   /* the slice in hand (chop_cur): its bit in set.crev */
};

/* the pages: their names, their four knobs, PRESET's value */
enum { PG_HOME, PG_GROOVE, PG_RISE, PG_BUILD, PG_KEYS, PG_VOX, PG_CHOP, PG_PUMP, PG_SPACE, PG_ECHO, PG_SETUP, PG_TAPE,
       PG_MIX, NPG };
static const struct {
    const char *title;                           /* the header's big word, as the posters set it */
    int8_t stamp;                                /* its stamped letter */
    uint8_t k[4], preset;
} PG[NPG] = {
    [PG_HOME] = {"FM-1 Again", 5, {PA_DEPTH, PA_MIX, PA_FILTER, PA_TEMPO}, PA_PAT},
    [PG_GROOVE] = {"Groove", 0, {PA_SWING, PA_VARY, PA_KICK, PA_HATS}, PA_PAT},
    [PG_RISE] = {"Rise", 0, {PA_RLEN, PA_RBRIGHT, PA_RSUCK, PA_RAUTO}, PA_RAUTO},
    [PG_BUILD] = {"Build", 0, {PA_BUILD, PA_BSTYLE, PA_TEMPOUP, PA_PLAYMODE}, PA_BSTYLE},
    [PG_KEYS] = {"Keys", 0, {PA_TONE, PA_DECAY, PA_PROG, PA_RHYTHM}, PA_CHORD},
    [PG_VOX] = {"Vox", 0, {PA_VOWELS, PA_GATE, PA_FORMANT, PA_BREATH}, PA_VOWELS},
    [PG_CHOP] = {"Chop", 0, {PA_SLICE, PA_LENGTH, PA_PITCH, PA_CMODE}, PA_CREV},
    [PG_PUMP] = {"Pump", 0, {PA_DEPTH, PA_RELEASE, PA_SHAPE, PA_SOURCE}, PA_SOURCE},
    [PG_SPACE] = {"Space", 0, {PA_SIZE, PA_DAMP, PA_MIX, PA_SHIMMER}, PA_SIZE},
    [PG_ECHO] = {"Echo", 0, {PA_ETIME, PA_EFB, PA_EWEAR, PA_EMIX}, PA_EPING},
    [PG_SETUP] = {"Setup", 0, {PA_KEY, PA_PADS, PA_KEYS_LV, PA_VOX_LV}, PA_SAVER},
    [PG_TAPE] = {"Tape", 0, {PA_TWOW, PA_TSAT, PA_TTONE, PA_THISS}, PA_TAPE},
    [PG_MIX] = {"Mix", 0, {PA_KICK, PA_HATS, PA_CLAP, PA_RISE_LV}, PA_PLAYMODE},
};
static const uint8_t PG_BUTTON[NPG] = {B_HOME, B_SEQ, B_SEQ, B_SEQ, B_EDIT, B_ENV, B_ENV, B_LFO, B_FX, B_FX, B_GLO, B_GLO,
                                       B_GLO};   /* (the same button again: the next of its group) */

/* what the keys play (ALGORITHM) */
enum { IN_KEYS, IN_VOX, IN_CHOP, IN_RISE, IN_DRUMS, NIN };
static const char *const IN_NAME[NIN] = {"KEYS", "VOX", "CHOP", "RISE", "DRUMS"};

/* the tutor's tips (draw_title), in order: each gone once done (set.tips) */
enum { TIP_PLAY, TIP_BUILD, TIP_ALGO, TIP_PADS, TIP_PAGES, TIP_SAVE, NTIP };

/* The settings record, kept from one release to the next: its parameters in PA_* order with their count (np), so a
 * release that adds parameters (at the end of PA_*, never between) loads the old record and gives the new ones their
 * defaults; a value out of its range takes its default alone. Another magic or size: all defaults. */
#define SET_MAGIC 0x41474E32u            /* "AGN2" */
#define SET_NP 96u                       /* the parameters' room */
typedef struct {
    uint32_t magic;
    uint8_t np;                          /* the parameters this record holds */
    int8_t octave;                       /* -2 .. +2 */
    uint8_t inst;                        /* IN_* */
    uint8_t ble_off;                     /* Bluetooth switched off (REC held); 0, the default: on */
    uint8_t page;
    uint8_t tips;                        /* the tips done (bit each: the tutor) */
    uint8_t rsv0[2];
    uint32_t crev;                       /* CHOP: the slices played backwards (REVERSE) */
    uint8_t p[SET_NP];                   /* the parameters */
    uint8_t rsv[16];
} settings_t;
_Static_assert(NPA <= SET_NP, "the settings record's room for parameters");
static settings_t set;
static uint8_t set_dirty;                /* changed since the last save: SAVE is lit */

/* the controls by their names in plat.h, in its order */
static const char *const BTN_NAME[NB] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ",
                                         "PLAY", "REC", "OCT-", "OCT+"};

/* the parameters into the engine (the main loop; the engine reads them a block at a time) */
static float pa01(uint32_t i) { return (float)set.p[i] / (float)PA[i].top; }
static volatile float bpm_base = 124.0f;     /* TEMPO (the clock adds the build's TEMPO UP to it) */
static void apply(void)
{
    bpm_base = 80.0f + (float)set.p[PA_TEMPO];
    ck.bpm = bpm_base + gr.bpm_add;
    ck.swing = 0.33f * pa01(PA_SWING);
    gr.pat = set.p[PA_PAT];
    gr.prog = set.p[PA_PROG];
    gr.rhythm = set.p[PA_RHYTHM];
    gr.key = set.p[PA_KEY];
    gr.build_bars = 2u << set.p[PA_BUILD];
    gr.lv[DR_KICK] = pa01(PA_KICK);
    gr.lv[DR_HAT] = gr.lv[DR_OPEN] = gr.lv[DR_SHAKER] = pa01(PA_HATS);
    gr.lv[DR_CLAP] = gr.lv[DR_SNARE] = gr.lv[DR_RIM] = pa01(PA_CLAP);
    kp.tone = pa01(PA_TONE);
    kp.decay = pa01(PA_DECAY);
    kp.level = pa01(PA_KEYS_LV);
    vx.set = set.p[PA_VOWELS];
    vx.gate_s = 0.04f + 0.6f * pa01(PA_GATE);
    vx.shift = 0.7f * fm_exp2f(pa01(PA_FORMANT));       /* 0.7 .. 1.4 */
    vx.breath = 0.6f * pa01(PA_BREATH);
    vx.level = pa01(PA_VOX_LV);
    ch.level = pa01(PA_VOX_LV);
    ch.sel = set.p[PA_SLICE];
    ch.length = 0.05f + 0.95f * pa01(PA_LENGTH);
    ch.pitch = (float)set.p[PA_PITCH] - 12.0f;
    ch.mode = set.p[PA_CMODE];
    ch.rev = set.crev;
    pm.depth = pa01(PA_DEPTH);
    pm.rel_s = 0.06f + 0.6f * pa01(PA_RELEASE);
    pm.shape = pa01(PA_SHAPE);
    pm.src = set.p[PA_SOURCE];
    rv_size = set.p[PA_SIZE];
    rv_damp = set.p[PA_DAMP];
    rv_mix = set.p[PA_MIX];
    rv_shim = set.p[PA_SHIMMER];
    rv_pre = 20;
    dl_ms = (int32_t)(15000.0f * (float)ETIME_16TH[set.p[PA_ETIME]] / bpm_base);
    dl_fb = set.p[PA_EFB];
    dl_wear = set.p[PA_EWEAR];
    dl_mix = set.p[PA_EPING] == 2u ? 0 : set.p[PA_EMIX];   /* (REVERSE: revecho.h instead) */
    dl_pp = set.p[PA_EPING] == 1u ? 110 : 0;
    rs.lenix = set.p[PA_RLEN];
    rs.bright = pa01(PA_RBRIGHT);
    rs.suck = pa01(PA_RSUCK);
    rs.autom = set.p[PA_RAUTO];
    rs.level = pa01(PA_RISE_LV);
    gr.vary = pa01(PA_VARY);
    gr.style = set.p[PA_BSTYLE];
    gr.tempo_up = (float)set.p[PA_TEMPOUP];
    gr.song = set.p[PA_PLAYMODE];
    tf_wow = set.p[PA_TWOW];
    tf_sat = set.p[PA_TSAT];
    tf_tone = set.p[PA_TTONE];
    tf_hiss = set.p[PA_THISS];
}

/* ------------------------------------------------------------------ sound --- */
static volatile uint32_t midi_last, midi_last_port, midi_count;   /* the last message in (shown), and how many */
static volatile int32_t last_note = -1;      /* the last note on (keys or MIDI), -1 after its off */
static volatile uint8_t want_stop;           /* PLAY stopped it: the interrupt lets the chords go */
static ch_sample_t ch_next;                  /* CHOP's sample from the main loop, taken at the next block */
static volatile uint8_t ch_swap;

/* a note from the keys or MIDI, to whatever the keys play (the main loop and MIDI in, both: the queues are one
 * writer each, so MIDI in posts from the audio interrupt only through kp_start directly) */
static void play_note(int32_t note, uint32_t vel, int on, int from_isr)
{
    uint32_t i;
    if (on)
        last_note = note;
    else if (last_note == note)
        last_note = -1;
    if (set.inst == IN_DRUMS) {
        uint32_t d = (uint32_t)(note % 12) * NDR / 12u;
        if (on) {
            if (from_isr)
                dr_hit(d, (float)vel / 127.0f);
            else
                dr.pend[d] = (uint8_t)(vel ? vel : 1u);
        }
        return;
    }
    if (set.inst == IN_CHOP) {
        if (from_isr) {
            if (on)
                ch_start(note, (float)vel / 127.0f);
            else
                for (i = 0; i < CH_NV; i++)
                    if (ch.v[i].note == note)
                        ch.v[i].held = 0;
        } else
            ch_post(note, vel, on);
        return;
    }
    if (set.inst == IN_RISE) {
        if (on) {
            if (from_isr)
                rs_start(note, rs_len_now(), (float)vel / 127.0f);
            else
                rs_post(note, vel);
        }
        return;
    }
    if (set.inst == IN_VOX) {
        if (from_isr) {
            if (on)
                vx_start(note, (float)vel / 127.0f);
            else
                for (i = 0; i < VX_NV; i++)
                    if (vx.v[i].note == note)
                        vx.v[i].held = 0;
        } else
            vx_post(note, vel, on);
        return;
    }
    for (i = 0; i < 5u; i++) {                   /* KEYS: the one-finger chord */
        int32_t iv = CH_IV[set.p[PA_CHORD]][i];
        if (iv < 0)
            break;
        if (from_isr) {
            if (on)
                kp_start(note + iv, (float)vel / 127.0f, 0);
            else
                kp_stop(note + iv, 0);
        } else
            kp_post(note + iv, vel, 0, on);
    }
}

static void midi_in(void)                    /* audio interrupt: the packets that came in */
{
    uint32_t pkt;
    int port;
    while (plat_midi_in(&pkt, &port)) {
        uint32_t st = (pkt >> 8) & 0xFFu, d1 = (pkt >> 16) & 0x7Fu, d2 = pkt >> 24;
        if (st != 0xF8u && st != 0xFEu) {    /* (clock and active sensing would flood the screen) */
            midi_last = pkt;
            midi_last_port = (uint32_t)port;
            midi_count++;
        }
        if ((st & 0xF0u) == 0x90u && d2)
            play_note((int32_t)d1, d2, 1, 1);
        else if ((st & 0xF0u) == 0x80u || (st & 0xF0u) == 0x90u)
            play_note((int32_t)d1, 0, 0, 1);
    }
}

/* the mix's buses, -1 .. 1: the kick (never pumped), the drums, the pumped (keys, voice), the reverb's send and
 * return */
static float b_kl[APP_BLOCK], b_kr[APP_BLOCK], b_dl[APP_BLOCK], b_dr[APP_BLOCK], b_pl[APP_BLOCK], b_pr[APP_BLOCK];
static float b_send[APP_BLOCK], b_rl[APP_BLOCK], b_rr[APP_BLOCK];
static svf_t f_hp[2], f_lp[2], f_riser;
static float gain_now;                           /* glides to MASTER: no step at a block's edge */
static float kick_g = 1.0f, drums_g = 1.0f;      /* PERFORM's KICK OUT and DRUMS OUT, gliding */
static uint32_t riser_rnd = 99u;

static void render_seg(uint32_t a, uint32_t b)
{
    if (b <= a)
        return;
    dr_render(b_kl + a, b_kr + a, b_dl + a, b_dr + a, b_send + a, gr.lv, b - a);
    kp_render(b_pl + a, b_pr + a, b_send + a, b - a);
    vx_render(b_pl + a, b_pr + a, b_send + a, b - a);
    ch_render(b_pl + a, b_pr + a, b_send + a, b - a);
    rs_render(b_dl + a, b_dr + a, b_send + a, b - a);
}

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t i, a = 0;
    float gain = (float)master_q12 / 4096.0f * 8300000.0f;   /* MASTER, read once a block */
    float pos0, pos1, hp_fc, lp_fc, flt = (float)set.p[PA_FILTER], kick_to, drums_to;
    if (n > APP_BLOCK)
        n = APP_BLOCK;
    midi_in();
    ck.bpm = bpm_base + gr.bpm_add;              /* (BUILD's TEMPO UP) */
    if (ch_swap) {                               /* a new sample: every voice of the old one off first */
        for (i = 0; i < CH_NV; i++)
            ch.v[i].note = -1;
        ch.s = ch_next;
        ch_swap = 0;
    }
    if (want_stop) {
        want_stop = 0;
        gr_stop();
    }
    for (i = 0; i < n; i++)
        b_kl[i] = b_kr[i] = b_dl[i] = b_dr[i] = b_pl[i] = b_pr[i] = b_send[i] = b_rl[i] = b_rr[i] = 0.0f;
    /* the steps, each where it falls in the block: the engines rendered up to it, then the step played */
    pos0 = ck_pos256();
    for (i = 0; i < n; i++)
        if (ck_frame()) {
            render_seg(a, i);
            a = i;
            if (ck.run)
                gr_step(ck.step);
        }
    render_seg(a, n);
    pos1 = ck_pos256();
    if (pos1 < pos0)
        pos1 += pos1 < 1.0f && pos0 > 255.0f ? 256.0f : 0.0f;
    if (pos1 < pos0)                             /* PLAY: the page starts again */
        pos0 = 0.0f;
    if (pfx.held & 1u << PF_ECHOTHROW)            /* PERFORM's throws: the keys and voices full into ECHO, SPACE */
        for (i = 0; i < n; i++)
            b_send[i] += 0.9f * (b_pl[i] + b_pr[i]);
    if (set.p[PA_EPING] == 2u)                   /* ECHO's REVERSE: the send backwards, a TIME later */
        re_render(b_send, b_rl, b_rr, b_send, n, (float)ETIME_16TH[set.p[PA_ETIME]], 1.2f * (float)set.p[PA_EFB] / 127.0f,
                  (float)set.p[PA_EWEAR] / 127.0f, 1.6f * (float)set.p[PA_EMIX] / 127.0f);
    echo_render(b_send, b_rl, b_rr, b_send, n);     /* ECHO: the send in; the echoes out (pumped) and into SPACE */
    if (pfx.held & 1u << PF_SPACETHROW)
        for (i = 0; i < n; i++)
            b_send[i] += 0.9f * (b_pl[i] + b_pr[i]);
    space_render(b_send, b_rl, b_rr, n);
    /* the filters: the build's high-pass on all but the kick; FILTER (HOME K3) on everything */
    hp_fc = gr.hp_fc;
    lp_fc = 20000.0f;
    if (flt > 52.0f)
        hp_fc = fm_maxf(hp_fc, 20.0f * fm_exp2f((flt - 52.0f) / 48.0f * 6.6f));
    else if (flt < 48.0f)
        lp_fc = 250.0f * fm_exp2f(flt / 48.0f * 6.3f);
    lp_fc = fm_minf(lp_fc, gr.song_lp);          /* SONG: the intro opening, the outro closing */
    svf_set(&f_hp[0], hp_fc, 0.75f);
    svf_set(&f_hp[1], hp_fc, 0.75f);
    svf_set(&f_lp[0], lp_fc, 0.8f);
    svf_set(&f_lp[1], lp_fc, 0.8f);
    svf_set(&f_riser, 300.0f * fm_exp2f(4.5f * gr.bd_p), 3.0f);
    kick_to = pfx.held & (1u << PF_KICKOUT | 1u << PF_DRUMSOUT) ? 0.0f : 1.0f;
    drums_to = pfx.held & 1u << PF_DRUMSOUT ? 0.0f : 1.0f;
    for (i = 0; i < n; i++) {
        float g = pm_gain(), l, r, hp, bp;
        kick_g += (kick_to - kick_g) * 0.004f;   /* PERFORM's KICK OUT, DRUMS OUT: ~5 ms, no click */
        drums_g += (drums_to - drums_g) * 0.004f;
        b_kl[i] *= kick_g;
        b_kr[i] *= kick_g;
        l = b_dl[i] * drums_g + (b_pl[i] + b_rl[i]) * g;
        r = b_dr[i] * drums_g + (b_pr[i] + b_rr[i]) * g;
        if (gr.riser > 0.0f) {                   /* the build's riser: noise, its band rising */
            svf_run(&f_riser, noise_f(&riser_rnd), &bp, 0);
            l += bp * gr.riser * 0.5f;
            r += bp * gr.riser * 0.5f;
        }
        if (hp_fc > 21.0f) {
            svf_run(&f_hp[0], l, 0, &hp);
            l = hp;
            svf_run(&f_hp[1], r, 0, &hp);
            r = hp;
        }
        l += b_kl[i];
        r += b_kr[i];
        if (lp_fc < 19000.0f) {
            l = svf_run(&f_lp[0], l, 0, 0);
            r = svf_run(&f_lp[1], r, 0, 0);
        }
        b_kl[i] = l;
        b_kr[i] = r;
    }
    pf_render(b_kl, b_kr, n, gr.pf_auto);        /* PERFORM's tricks (the pads, SEL + a white key; the build's) */
    tape_render(b_kl, b_kr, n);                  /* TAPE over everything */
    for (i = 0; i < n; i++) {
        b_kl[i] = fm_tanhf(1.3f * b_kl[i]);      /* a soft glue, never past full scale */
        b_kr[i] = fm_tanhf(1.3f * b_kr[i]);
    }
    wv_feed(b_kl, b_kr, n, pos0, (pos1 - pos0) / (float)n);
    for (i = 0; i < n; i++) {
        gain_now += (gain - gain_now) * 0.004f;                  /* ~5 ms: turning MASTER does not click */
        lr[2 * i] = (int32_t)(b_kl[i] * gain_now);
        lr[2 * i + 1] = (int32_t)(b_kr[i] * gain_now);
    }
}

void app_panic(void)
{
    uint32_t i;
    ck.run = 0;
    for (i = 0; i < KP_NV; i++)
        kp.v[i].note = -1;
    for (i = 0; i < VX_NV; i++)
        vx.v[i].note = -1;
    for (i = 0; i < CH_NV; i++)
        ch.v[i].note = -1;
    gr.chord[0] = gr.chord[1] = gr.chord[2] = gr.chord[3] = gr.chord[4] = -1;
}

/* --------------------------------------------------------------- settings --- */
static void settings_defaults(void)
{
    uint32_t i;
    memset(&set, 0, sizeof set);
    set.magic = SET_MAGIC;
    set.np = NPA;
    set.inst = IN_KEYS;
    set.page = PG_HOME;
    for (i = 0; i < NPA; i++)
        set.p[i] = PA[i].def;
}

static void settings_load(void)          /* the saved settings (what of them is right), or the defaults */
{
    uint32_t i, np;
    if (plat_store_load(OBJ_SETTINGS, &set, sizeof set) != (int)sizeof set || set.magic != SET_MAGIC) {
        settings_defaults();
        apply();
        return;
    }
    np = set.np;                         /* (an older release's record: fewer; the rest take their defaults) */
    for (i = 0; i < NPA; i++)
        if (i >= np || set.p[i] > PA[i].top)
            set.p[i] = PA[i].def;
    set.np = NPA;
    if (set.octave < -2 || set.octave > 2)
        set.octave = 0;
    if (set.inst >= NIN)
        set.inst = IN_KEYS;
    if (set.ble_off > 1u)
        set.ble_off = 0;
    if (set.page >= NPG)
        set.page = PG_HOME;
    apply();
}

/* CHOP's sample: the one sent over USB (the companion's file "chop"), else the factory's. One copy (A is B), in
 * XDATA1 (0xE5000 .. 0xFBFFF, 92 KiB: 8.2 s): the data area is the screensaver's film. A cut mid-write: the factory
 * phrase until it is sent again. (XDATA1's rule, plat.h: tools/again_chop.py keeps "AT" from each sector's 0xF06) */
static const plat_file_t CH_FILE = {PLAT_XDATA1_LO, PLAT_XDATA1_LO, PLAT_XDATA1_HI - PLAT_XDATA1_LO};
_Static_assert(PLAT_XDATA1_LO == 0xE5000u && PLAT_XDATA1_HI - PLAT_XDATA1_LO == 0x17000u, "CHOP's file: XDATA1");
static char ch_name[16];
static uint32_t ch_gen;                      /* a new sample each time (the screen draws it again) */
static void chop_factory(void)                /* the factory phrase now (its flash about to be written) */
{
    ch_sample_t s;
    ch_parse(CHOP_FACTORY, sizeof CHOP_FACTORY, &s);
    ch_swap = 0;
    ch_next = s;
    ch_swap = 1;                             /* (the next block lets go of the old one before it reads a byte) */
}
static void chop_load(void)
{
    ch_sample_t s;
    int n = plat_file_size(&CH_FILE);
    if (!(n > 0 && ch_parse(plat_file_data(&CH_FILE), (uint32_t)n, &s)))
        ch_parse(CHOP_FACTORY, sizeof CHOP_FACTORY, &s);
    memcpy(ch_name, s.name, sizeof ch_name);
    wv_overview(&s);
    ch_next = s;
    ch_gen++;
    ch_swap = 1;
}

void app_init(void)
{
    dr_init();
    settings_load();
    chop_load();
    ch.s = ch_next;
    ch_swap = 0;
}

/* --------------------------------------------------------------------- UI --- */
static struct {
    char msg[2][24];                     /* a short message over the header */
    uint32_t msg_until;
    uint32_t keys, btn;                  /* held at the last input */
    uint8_t vel[NKEYS], prs[NKEYS];      /* each key's last velocity, and its pressure while held */
    int32_t last_key;
    uint8_t looking;                     /* Bluetooth MIDI devices: REC pressed, the scan's list up (lib/btdev) */
    uint32_t rec_t;                      /* REC went down (0: not held, or its hold already acted) */
    int32_t turns[NE];                   /* detents since boot, per encoder */
    uint32_t lit_until[4];               /* a knob just turned: its cell lit */
    uint32_t trick_keys;                 /* the keys holding a PERFORM trick */
    uint8_t trick_of[NKEYS];             /* .. which */
    uint8_t sel_used;                    /* SEL held for a trick (not a tap) */
    uint8_t redraw, redraw_row, redraw_title;
    uint8_t full;                        /* everything drawn again (the screensaver gone) */
} ui;

/* key k (0: F3) among the white keys: 0, 1, .. (-1: a black key) */
static int32_t white_index(uint32_t k)
{
    static const int8_t W[12] = {0, -1, 1, -1, 2, -1, 3, 4, -1, 5, -1, 6};   /* F F# G G# A A# B C C# D D# E */
    return W[k % 12u] < 0 ? -1 : (int32_t)(k / 12u) * 7 + W[k % 12u];
}
/* .. among the black keys: 0 (F#3), 1, .. (-1: a white key) */
static int32_t black_index(uint32_t k)
{
    static const int8_t B[12] = {-1, 0, -1, 1, -1, 2, -1, -1, 3, -1, 4, -1};
    return B[k % 12u] < 0 ? -1 : (int32_t)(k / 12u) * 5 + B[k % 12u];
}
/* the trick key k holds (-1: none): a pad (a black key, PADS on) or, SEL held, a white key */
static int32_t key_trick(uint32_t k, int sel)
{
    int32_t b = black_index(k), w = white_index(k);
    if (set.p[PA_PADS] && b >= 0 && b < NPAD)
        return PAD_TRICK[b];
    if (sel && w >= 0 && w < NPF)
        return w;
    return -1;
}

static void text_cat(char *d, const char *s)
{
    while (*d)
        d++;
    while (*s)
        *d++ = *s++;
    *d = 0;
}

static void int_text(int32_t v, char *b)     /* signed, no padding */
{
    char t[12];
    int n = 0;
    uint32_t u = (uint32_t)(v < 0 ? -v : v);
    do
        t[n++] = (char)('0' + u % 10u);
    while ((u /= 10u) && n < 11);
    if (v < 0)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
}

void app_say(const char *a, const char *b)
{
    uint32_t i;
    char t[48];
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
    ui.redraw = 1;
    if (ann_on()) {                          /* and to a screen reader (announce.c, fm1-reader) */
        t[0] = 0;
        text_cat(t, ui.msg[0]);
        if (ui.msg[1][0]) {
            text_cat(t, " ");
            text_cat(t, ui.msg[1]);
        }
        ann_say(t, 0, ANN_NOW);
    }
}

/* a parameter's value as the screen shows it (and a screen reader says it) */
static void value_text(uint32_t i, char *t)
{
    int32_t v = set.p[i];
    t[0] = 0;
    switch (PA[i].fmt) {
    case FMT_PCT:
        int_text(v * 100 / PA[i].top, t);
        text_cat(t, "%");
        break;
    case FMT_BPM:
        int_text(80 + v, t);
        break;
    case FMT_LIST:
        text_cat(t, PA[i].list[v]);
        break;
    case FMT_MS:
        int_text(i == PA_GATE ? 40 + v * 6 : 60 + v * 6, t);
        text_cat(t, "MS");
        break;
    case FMT_KEY:
        text_cat(t, KEY_NAME[v]);
        break;
    case FMT_BARS:
        text_cat(t, BARS_NAME[v]);
        text_cat(t, " BARS");
        break;
    case FMT_FILTER:
        if (v > 52)
            text_cat(t, "HP "), int_text((v - 52) * 100 / 48, t + 3);
        else if (v < 48)
            text_cat(t, "LP "), int_text((48 - v) * 100 / 48, t + 3);
        else
            text_cat(t, "OPEN");
        break;
    case FMT_SIZE:
        if (v >= 127)
            text_cat(t, "INF");
        else {
            int32_t d = RV_RT60_X10[v];
            int_text(d / 10, t);
            text_cat(t, ".");
            int_text(d % 10, t + 0 + (d / 10 >= 10 ? 3 : 2));
            text_cat(t, "S");
        }
        break;
    case FMT_SEMI:
        if (v > 12)
            text_cat(t, "+");
        int_text(v - 12, t + (v > 12));
        break;
    case FMT_ONE:
        int_text(v + 1, t);
        break;
    case FMT_UP:                                 /* OFF, or +N BPM */
        if (!v) {
            text_cat(t, "OFF");
            break;
        }
        text_cat(t, "+");
        int_text(v, t + 1);
        text_cat(t, " BPM");
        break;
    default:
        int_text(v, t);
    }
}

static void octave_text(char *t)
{
    t[0] = set.octave < 0 ? '-' : '+';
    t[1] = (char)('0' + (set.octave < 0 ? -set.octave : set.octave));
    t[2] = 0;
}

/* a screen reader that has just started listening (or asks) hears the whole screen */
static void speak(void)
{
    char t[160], v[24];
    uint32_t k;
    if (ann_on() && ann_where()) {
        t[0] = 0;
        text_cat(t, set.page == PG_HOME ? APP_NAME : PG[set.page].title);
        text_cat(t, ck.run ? ". PLAYING. " : ". ");
        for (k = 0; k < 4u; k++) {
            uint32_t p = PG[set.page].k[k];
            value_text(p, v);
            text_cat(t, PA[p].name);
            text_cat(t, " ");
            text_cat(t, v);
            text_cat(t, ". ");
        }
        text_cat(t, "KEYS PLAY ");
        text_cat(t, IN_NAME[set.inst]);
        ann_say(t, 0, ANN_NOW);
    }
    ann_tick(plat_ms());
}

static void octave_step(int32_t d)
{
    int32_t o = clamp(set.octave + d, -2, 2);
    char t[4];
    if (o == set.octave)
        return;
    set.octave = (int8_t)o;
    set_dirty = 1;
    octave_text(t);
    app_say("OCTAVE", t);
}

int again_ble_off(void) { return set.ble_off; }

/* REC held a second: Bluetooth off or on (kept with the settings, so it stays so at the next power-on) */
static void ble_toggle(void)
{
    set.ble_off = (uint8_t)!set.ble_off;
    set_dirty = 1;
    plat_ble_enable(!set.ble_off);
    app_say("BLUETOOTH", set.ble_off ? "OFF" : "ON");
}

/* REC: a Bluetooth MIDI device (FM1B_BLE_CENTRAL, lib/btdev): look for devices, then connect to the one ALGORITHM
 * picks, then let it go */
static void ble_device_say(void)
{
    char name[21];
    if (btdev_found(btdev_pick(), name, 0))
        app_say("DEVICE", name);
    else
        app_say("BLE", "SCANNING");
}
static void ble_device_button(void)
{
    int ev;
    if (!btdev_busy() && !ui.looking) {
        ui.looking = 1;
        app_say("BLE", "SCANNING");
        return;
    }
    ev = btdev_save();
    ui.looking = 0;
    app_say(ev == BTDEV_CONNECTING ? btdev_word(ev) : "BLE", ev == BTDEV_CONNECTING ? btdev_name() : btdev_word(ev));
}

/* a parameter turned by d detents */
/* CHOP: the slice in hand (PITCHED: SLICE; SLICES: the one played last) */
static uint32_t chop_cur(void)
{
    uint32_t n = ch_next.nslices ? ch_next.nslices : 1u;
    return (set.p[PA_CMODE] == CM_PITCHED ? set.p[PA_SLICE] : ch.last_slice) % n;
}

/* parameter i to v (0 .. its top): what goes with it, the engine, the screen and a screen reader */
static void param_set(uint32_t i, int32_t v)
{
    char t[24];
    v = clamp(v, 0, PA[i].top);
    if (v == set.p[i])
        return;
    set.p[i] = (uint8_t)v;
    if (i == PA_PAT)                             /* a new groove brings its own swing */
        set.p[PA_SWING] = (uint8_t)(GR_PAT[v].swing / 0.33f * 100.0f + 0.5f);
    if (i == PA_TAPE) {                          /* a tape: its four */
        set.p[PA_TWOW] = TP_SET[v][0];
        set.p[PA_TSAT] = TP_SET[v][1];
        set.p[PA_TTONE] = TP_SET[v][2];
        set.p[PA_THISS] = TP_SET[v][3];
    }
    if (i == PA_CREV)                            /* REVERSE: the slice in hand's bit */
        set.crev = v ? set.crev | 1u << chop_cur() : set.crev & ~(1u << chop_cur());
    set_dirty = 1;
    apply();
    value_text(i, t);
    if (i == PA_CREV) {                          /* "SLICE 3" "REVERSE" */
        char nm[12] = "SLICE ";
        int_text((int32_t)chop_cur() + 1, nm + 6);
        app_say(nm, t);
    } else {
        app_say(PA[i].name, t);
    }
    ui.redraw_row = ui.redraw_title = 1;
}
static void param_turn(uint32_t i, int32_t d) { param_set(i, (int32_t)set.p[i] + d); }

static void play_toggle(void)
{
    if (ck.run) {
        ck.run = 0;
        want_stop = 1;
        app_say("STOPPED", "");
    } else {
        ck_start();
        app_say("PLAYING", PAT_NAME[set.p[PA_PAT]]);
    }
}

static void input(void)
{
    uint32_t keys, btn = plat_buttons(), k, pg;
    int32_t e;
    touch_poll();                            /* the sensed keys' presses, velocities and pressure */
    keys = touch_keys();
    if (keys || btn || keys != ui.keys || btn != ui.btn)
        sv.touch = plat_ms();
    if (sv.on) {                             /* the screensaver: the first touch only wakes the screen */
        int any = keys != ui.keys || btn != ui.btn || keys || btn;
        for (k = 0; k < NE; k++)
            any |= plat_enc((int)k) != 0;
        if (any) {
            sv.on = 0;
            sv.touch = plat_ms();
            ui.full = 1;
            ui.keys = keys;                      /* (what is held now plays nothing; let go, nothing either) */
            ui.trick_keys &= keys;
            ui.btn = btn;
        }
        return;
    }
    for (k = 0; k < NKEYS; k++) {            /* key 0 is F3 (MIDI 53) */
        uint32_t bit = 1u << k, v;
        int32_t note = 53 + (int32_t)k + 12 * set.octave, w;
        if ((keys & bit) && !(ui.keys & bit) && (w = key_trick(k, btn >> B_SEL & 1u)) >= 0) {
            ui.trick_keys |= bit;                /* a pad, or SEL and a white key: a trick while held */
            ui.trick_of[k] = (uint8_t)w;
            ui.sel_used |= (uint8_t)(btn >> B_SEL & 1u);
            pfx.held |= 1u << w;
            set.tips |= 1u << TIP_PADS;
            app_say(PF_NAME[w], "");
            continue;
        }
        if (ui.trick_keys & bit) {               /* a trick's key: let go, the trick ends */
            if (!(keys & bit)) {
                ui.trick_keys &= ~bit;
                pfx.held &= ~(1u << ui.trick_of[k]);
            }
            continue;
        }
        if ((keys & bit) && !(ui.keys & bit) && (btn >> B_SEL & 1u))
            continue;                            /* (SEL and a key that is no trick: nothing) */
        if ((keys & bit) && !(ui.keys & bit)) {
            v = touch_vel(k);
            ui.vel[k] = (uint8_t)v;
            ui.prs[k] = 0;
            ui.last_key = (int32_t)k;
            play_note(note, v, 1, 0);
            plat_midi_send(0x09u | 0x90u << 8 | (uint32_t)note << 16 | v << 24, TO_USB | TO_BLE);
        } else if (!(keys & bit) && (ui.keys & bit)) {
            ui.prs[k] = 0;
            play_note(note, 0, 0, 0);
            plat_midi_send(0x08u | 0x80u << 8 | (uint32_t)note << 16, TO_USB | TO_BLE);
        } else if ((keys & bit) && touch_pressure(k) != ui.prs[k]) {   /* pressure: poly aftertouch */
            ui.prs[k] = touch_pressure(k);
            plat_midi_send(0x0Au | 0xA0u << 8 | (uint32_t)note << 16 | (uint32_t)ui.prs[k] << 24, TO_USB | TO_BLE);
        }
    }
    for (k = 0; k < NB; k++) {               /* a button pressed */
        uint32_t bit = 1u << k;
        if (!(btn & bit) || (ui.btn & bit))
            continue;
        for (pg = 0; pg < NPG && PG_BUTTON[pg] != k; pg++)
            ;
        if (pg < NPG && PG_BUTTON[set.page] == k && set.page + 1u < NPG && PG_BUTTON[set.page + 1u] == k)
            pg = set.page + 1u;                  /* the button again: its next page */
        if (pg < NPG) {                      /* a page's button: that page */
            if (set.page != pg) {
                set.page = (uint8_t)pg;
                ui.redraw_row = ui.redraw_title = 1;
                set.tips |= 1u << TIP_PAGES;
            }
            app_say(pg == PG_HOME ? "HOME" : PG[pg].title, "");
            ui.msg_until = 0;                    /* (heard, not shown: the header names the page) */
        } else if (k == B_OCTDN)
            octave_step(-1);
        else if (k == B_OCTUP)
            octave_step(1);
        else if (k == B_REC && FM1B_BLE)
            ui.rec_t = plat_ms();                /* a press: on release (a hold switches Bluetooth) */
        else if (k == B_SAVE) {
            set.tips |= 1u << TIP_SAVE;
            if (plat_store_save(OBJ_SETTINGS, &set, sizeof set) == 0) {
                set_dirty = 0;
                app_say("SAVED", "");
            } else {
                app_say("SAVE", "FAILED");
            }
        } else if (k == B_PLAY)
            play_toggle();
        else if (k == B_ARP) {                   /* BUILD, then DROP */
            if (!ck.run)
                play_toggle();
            if (gr.bd == BD_BUILD) {
                gr_build_button();
                app_say("DROP", "AT THE BAR");
            } else {
                gr_build_button();
                app_say("BUILD", BARS_NAME[set.p[PA_BUILD]]);
                set.tips |= 1u << TIP_BUILD;
            }
        } else if (k == B_SEL) {
            ui.sel_used = 0;                     /* (said on its release, unless a trick was played) */
        }
    }
    if (!(btn >> B_SEL & 1u) && (ui.btn >> B_SEL & 1u) && !ui.sel_used)
        app_say("KEYS PLAY", IN_NAME[set.inst]);  /* SEL tapped: what the keys play */
    if (FM1B_BLE && ui.rec_t) {             /* REC: held a second, Bluetooth on / off; a press, a device */
        if (btn >> B_REC & 1u) {
            if (plat_ms() - ui.rec_t >= 1000u) {
                ui.rec_t = 0;
                ble_toggle();
            }
        } else {
            ui.rec_t = 0;
            if (FM1B_BLE_CENTRAL && !set.ble_off)
                ble_device_button();
            else
                app_say("BUTTON", "REC");
        }
    }
    for (k = 0; k < NE; k++) {               /* an encoder turned */
        if ((e = plat_enc((int)k)) == 0)
            continue;
        sv.touch = plat_ms();
        ui.turns[k] += e;
        if (k == EN_ALGO && FM1B_BLE_CENTRAL && ui.looking) {
            btdev_choose(btdev_pick() + e);
            ble_device_say();
        } else if (k == EN_ALGO) {           /* what the keys play */
            set.inst = (uint8_t)clamp((int32_t)set.inst + e, 0, NIN - 1);
            set_dirty = 1;
            set.tips |= 1u << TIP_ALGO;
            app_say("KEYS PLAY", IN_NAME[set.inst]);
        } else if (k == EN_SELECT) {         /* the pages in turn */
            set.page = (uint8_t)clamp((int32_t)set.page + e, 0, NPG - 1);
            ui.redraw_row = ui.redraw_title = 1;
            app_say(set.page == PG_HOME ? "HOME" : PG[set.page].title, "");
            ui.msg_until = 0;
        } else if (k == EN_PRESET) {
            param_turn(PG[set.page].preset, e);
        } else {
            uint32_t c = k - EN_K1;
            param_turn(PG[set.page].k[c], e);
            ui.lit_until[c] = plat_ms() + 900u;
        }
    }
    ui.keys = keys;
    ui.btn = btn;
    plat_glow(1);                            /* the unlit buttons glow, readable in the dark */
    plat_leds(btn | (set_dirty ? 1u << B_SAVE : 0u) | (ck.run ? 1u << B_PLAY : 0u) |
                  (gr.bd != BD_NONE ? 1u << B_ARP : 0u) | 1u << PG_BUTTON[set.page],
              keys);
}

void app_input_only(void) { input(); }

/* ----------------------------------------------------------------- screen --- */
/* the poster's big type: s in the big face, its letter at "stamp" (-1: none) in the stamped one */
static int32_t poster_text(int32_t x, int32_t y, const char *s, int32_t stamp)
{
    char one[2] = {0, 0};
    int32_t i;
    for (i = 0; s[i]; i++) {
        one[0] = s[i];
        x = cv_text(x, y, i == stamp ? &AG_LS : &AG_L, one, C_WHITE);
    }
    return x;
}

/* a name in capitals as the poster sets it: "2-STEP" -> "2-Step" */
static void title_case(const char *s, char *t)
{
    uint32_t i, first = 1;
    for (i = 0; s[i] && i < 23u; i++) {
        char c = s[i];
        t[i] = !first && c >= 'A' && c <= 'Z' ? (char)(c + 32) : c;
        first = c == ' ' || c == '-';
    }
    t[i] = 0;
}

static void draw_header(void)
{
    char t[32], n[24];
    uint32_t step = ck.step & 255u, bar = step / 16u;
    cv_begin(240, WV_TOP, C_BLACK);
    /* line 1: the page; HOME: the bar, the build, the drop */
    uint32_t held = pfx.held | gr.pf_auto;
    if (held) {                                  /* a trick held: its name, big (loaf-breaker's pads show theirs) */
        uint32_t k;
        for (k = 0; k < NPF && !(held >> k & 1u); k++)
            ;
        title_case(PF_NAME[k], t);
        poster_text(8, 2, t, 0);
    } else if (set.page != PG_HOME)
        poster_text(8, 2, PG[set.page].title, PG[set.page].stamp);
    else if (ck.run && gr.drops && ck.step - gr.drop_step < 8u)
        poster_text(8, 2, "Drop!", 0);
    else if (ck.run && gr.bd == BD_BUILD) {
        uint32_t left = gr.bd_bars - (ck.step - gr.bd_from) / 16u;
        t[0] = 0;
        text_cat(t, "Drop in ");
        int_text((int32_t)left, n);
        text_cat(t, n);
        poster_text(8, 2, t, 0);
    } else if (ck.run && gr.bd == BD_ARMED)
        poster_text(8, 2, "Build...", 0);
    else if (ck.run && gr.song) {                /* SONG: the section and its bar */
        t[0] = 0;
        text_cat(t, GR_SONG[gr.sec % GR_NSEC].name);
        text_cat(t, " ");
        int_text((int32_t)gr.sec_bar + 1, n);
        text_cat(t, n);
        text_cat(t, "/");
        int_text((int32_t)GR_SONG[gr.sec % GR_NSEC].bars, n);
        text_cat(t, n);
        poster_text(8, 2, t, 0);
    } else if (ck.run) {
        t[0] = 0;
        text_cat(t, "Bar ");
        int_text((int32_t)bar + 1, n);
        text_cat(t, n);
        text_cat(t, ".");
        int_text((int32_t)(step % 16u) / 4 + 1, n);
        text_cat(t, n);
        poster_text(8, 2, t, 0);
    } else
        poster_text(8, 2, PG[PG_HOME].title, PG[PG_HOME].stamp);
    /* line 2: a message for a moment; else the page's choice (HOME: the tempo) */
    if (plat_ms() < ui.msg_until) {
        int32_t x = cv_text(8, 42, &AG_M, ui.msg[0], C_WHITE);
        cv_text(x + 8, 42, &AG_M, ui.msg[1], C_WHITE);
    } else if (set.page == PG_HOME) {
        int_text((int32_t)(ck.bpm + 0.5f), t);     /* (TEMPO UP: the tempo now) */
        text_cat(t, " BPM");
        poster_text(8, 36, t, 4);
    } else {
        char w[32];
        uint32_t pr = PG[set.page].preset;
        w[0] = 0;
        value_text(pr, n);
        text_cat(w, n);
        title_case(w, t);
        poster_text(8, 36, t, -1);
    }
    cv_blit(0, 0);
}

/* the bottom row: K1-K4, each its name small and its value bigger; one just turned is lit (white, black type) */
#define ROW_Y 190
static void draw_row(void)
{
    uint32_t k, now = plat_ms();
    char v[24];
    for (k = 0; k < 4u; k++) {                   /* a canvas each: a long word is cut at its cell's edge */
        uint32_t p = PG[set.page].k[k];
        int32_t lit = now < ui.lit_until[k];
        uint16_t ink = lit ? C_BLACK : C_WHITE;
        cv_begin(60, 240 - ROW_Y, lit ? C_WHITE : C_BLACK);
        cv_rect(0, 0, 60, 1, C_WHITE);
        if (k && !lit)
            cv_rect(0, 1, 1, 49, RGB(90, 90, 90));
        cv_text(4, 4, &AG_S, PA[p].name, ink);
        value_text(p, v);
        if (text_w(&AG_M, v) > 55)               /* a long word: the small face */
            cv_text(4, 27, &AG_S, v, ink);
        else
            cv_text(4, 22, &AG_M, v, ink);
        cv_blit((int32_t)k * 60, ROW_Y);
    }
}

/* the tutor (loaf-breaker's idea): the first thing not done yet, on HOME's title bar, until it is done */
static const char *const TIP_TEXT[NTIP] = {"TIP: PRESS PLAY", "TIP: ARP BUILDS TO THE DROP", "TIP: ALGO - WHAT THE KEYS PLAY",
                                           "TIP: HOLD A BLACK KEY - A TRICK", "TIP: SEQ EDIT ENV FX - THE PAGES",
                                           "TIP: SAVE KEEPS IT ALL"};
static int32_t tip_now(void)
{
    uint32_t i;
    if (ck.run)
        set.tips |= 1u << TIP_PLAY;
    for (i = 0; i < NTIP; i++)
        if (!(set.tips >> i & 1u))
            return (int32_t)i;
    return -1;
}

/* the panel's title bar: off HOME the page's group as tabs (loaf-breaker's), the one shown inverted, and a word on
 * the right; on HOME the groove, or a tip */
static void draw_title(void)
{
    char t[48], n[12];
    int32_t tip = set.page == PG_HOME ? tip_now() : -1;
    t[0] = 0;
    if (tip >= 0) {
        cv_begin(240, WV_TITLE_H, WV_INK);
        cv_text(6, 1, &AG_S, TIP_TEXT[tip], WV_BAR);
        cv_blit(0, WV_TOP);
        return;
    }
    if (set.page != PG_HOME) {
        uint32_t g, x = 2;
        cv_begin(240, WV_TITLE_H, WV_FRAME);
        cv_rect(1, 1, 238, WV_TITLE_H - 1, WV_BAR);
        for (g = 0; g < NPG; g++) {
            if (g == PG_HOME || PG_BUTTON[g] != PG_BUTTON[set.page])
                continue;
            n[0] = 0;
            text_cat(n, PG[g].title);
            {
                uint32_t i;
                for (i = 0; n[i]; i++)
                    n[i] = n[i] >= 'a' && n[i] <= 'z' ? (char)(n[i] - 32) : n[i];
            }
            if (g == set.page) {
                cv_rect((int32_t)x, 2, text_w(&AG_T, n) + 6, WV_TITLE_H - 3, WV_INK);
                cv_text((int32_t)x + 3, 2, &AG_T, n, WV_BAR);
            } else
                cv_text((int32_t)x + 3, 2, &AG_T, n, WV_INK);
            x += (uint32_t)text_w(&AG_T, n) + 8u;
        }
        t[0] = 0;
        if (set.page == PG_CHOP) {               /* on the right: CHOP its sample, SETUP the load, else the groove */
            title_case(ch_name, t);
        } else if (set.page == PG_SETUP) {
            text_cat(t, "CPU ");
            int_text((int32_t)plat_cpu_pct(), n);
            text_cat(t, n);
            text_cat(t, "%");
        } else {
            title_case(PAT_NAME[set.p[PA_PAT]], t);
            text_cat(t, " ");
            int_text(80 + set.p[PA_TEMPO], n);
            text_cat(t, n);
        }
        if (text_w(&AG_T, t) < 236 - (int32_t)x)
            cv_text(236 - text_w(&AG_T, t), 2, &AG_T, t, WV_INK);
        cv_blit(0, WV_TOP);
        return;
    }
    title_case(PAT_NAME[set.p[PA_PAT]], t);
    text_cat(t, " - ");
    int_text(80 + set.p[PA_TEMPO], n);
    text_cat(t, n);
    text_cat(t, " - ");
    text_cat(t, KEY_NAME[set.p[PA_KEY]]);
    text_cat(t, " - ");
    title_case(IN_NAME[set.inst], n);
    text_cat(t, n);
    if (gr.song)
        text_cat(t, " - Song");
    wv_draw_title(t);
}

/* the pads' strip (loaf-breaker's): the black keys' tricks, the ones playing inverted; SEL's when PADS are off */
#define PADS_Y (WV_R_Y + WV_LANE_H)
static void draw_pads(void)
{
    uint32_t k, held = pfx.held | gr.pf_auto;
    cv_begin(240, ROW_Y - PADS_Y, C_BLACK);
    if (!set.p[PA_PADS])
        cv_text(4, 0, &AG_T, "SEL + A WHITE KEY: A TRICK", RGB(150, 150, 150));
    else
        for (k = 0; k < NPAD; k++) {
            int32_t x = (int32_t)(k * 240u / NPAD), w = (int32_t)((k + 1u) * 240u / NPAD) - x - 1;
            int on = held >> PAD_TRICK[k] & 1u;
            if (on)
                cv_rect(x, 0, w, ROW_Y - PADS_Y, C_WHITE);
            else if (k)
                cv_rect(x - 1, 2, 1, ROW_Y - PADS_Y - 4, RGB(70, 70, 70));
            cv_text(x + (w - text_w(&AG_T, PAD_SHORT[k])) / 2, 0, &AG_T, PAD_SHORT[k], on ? C_BLACK : RGB(170, 170, 170));
        }
    cv_blit(0, PADS_Y);
}

void app_ui_init(void)
{
    touch_on(1);                             /* velocity and pressure: the keys' readings on */
    sv_init();                               /* the screensaver's film, if the resources hold it */
    sv.touch = plat_ms();
    lcd_fill(0, 0, 240, 240, C_BLACK);
    ui.redraw = ui.redraw_row = ui.redraw_title = 1;
}

void app_frame(void)
{
    static uint32_t last_step = 0xFFFFFFFFu, last_run, last_inst = 99u, lit_any, last_bd;
    uint32_t i, now = plat_ms(), lit = 0;
    input();
    speak();
    tapefx_poll();                               /* TAPE's saturation curve, made here when it changes */
    if (FM1B_BLE_CENTRAL) {                  /* looking: a device found is named at once (REC connects to it) */
        static int found;
        int n = ui.looking ? btdev_count() : 0, ev;
        btdev_page(ui.looking && !set.ble_off);
        if (n > found && btdev_pick() < n)
            ble_device_say();
        found = n;
        while ((ev = btdev_event()) != BTDEV_NONE)   /* a device connected, given up or lost */
            if (ev == BTDEV_CONNECTED || ev == BTDEV_NO_ANSWER || ev == BTDEV_LOST)
                app_say("DEVICE", btdev_word(ev));
    }
    if (!sv.on && sv.p && set.p[PA_SAVER] && now - sv.touch >= SV_IDLE_MS) {
        sv.on = 1;                               /* untouched a minute: the film */
        sv.at = 0;
        sv.t = now - sv.ms;
    }
    if (sv.on) {
        wv_held = 1;                             /* (the waveform's columns wait meanwhile) */
        sv_frame(now);
        return;
    }
    if (ui.full) {                               /* back from the screensaver: everything */
        lcd_fill(0, 0, 240, 240, C_BLACK);
        ui.redraw = ui.redraw_row = ui.redraw_title = 1;
    }
    if (ui.msg_until && now >= ui.msg_until) {
        ui.msg_until = 0;
        ui.redraw = 1;
    }
    if (set.page == PG_HOME && ck.run && (ck.step / 4u != last_step || gr.bd != last_bd)) {
        last_step = ck.step / 4u;                /* HOME counts the beats */
        last_bd = gr.bd;
        ui.redraw = 1;
    }
    if (ck.run != last_run || set.inst != last_inst) {
        last_run = ck.run;
        last_inst = set.inst;
        ui.redraw = ui.redraw_title = 1;
    }
    for (i = 0; i < 4u; i++)
        lit |= (uint32_t)(now < ui.lit_until[i]) << i;
    {                                            /* the CPU short (the audio past 85 % of its budget): KEYS plays fewer voices */
        uint32_t cpu = plat_cpu_pct();
        kp.nv = cpu > 85u ? 6u : cpu < 70u ? KP_NV : kp.nv;
    }
    if (set.page == PG_SETUP) {                  /* the load, once a second */
        static uint32_t cpu_at;
        if (now - cpu_at >= 1000u) {
            cpu_at = now;
            ui.redraw_title = 1;
        }
    }
    if (lit != lit_any) {                        /* a cell's light goes out */
        lit_any = lit;
        ui.redraw_row = 1;
    }
    {                                            /* the pads' strip, a trick's name in the header, the tutor */
        static uint32_t last_held = 0xFFFFFFFFu, last_pads = 99u, last_tip = 99u, last_song;
        uint32_t held = pfx.held | gr.pf_auto, tip = (uint32_t)(tip_now() + 1);
        if (held != last_held || set.p[PA_PADS] != last_pads || ui.full) {
            if ((held != 0) != (last_held != 0) || held != last_held)
                ui.redraw = 1;
            last_held = held;
            last_pads = set.p[PA_PADS];
            draw_pads();
        }
        if (tip != last_tip || (uint32_t)gr.song != last_song) {
            last_tip = tip;
            last_song = gr.song;
            ui.redraw_title = 1;
        }
        if (gr.song && ck.run && set.page == PG_HOME) {   /* SONG: the section changes */
            static uint32_t last_sec = 99u;
            if (gr.sec != last_sec) {
                last_sec = gr.sec;
                ui.redraw = 1;
            }
        }
    }
    if (ui.redraw_title) {
        static uint8_t panel;
        ui.redraw_title = 0;
        ui.redraw = 1;
        draw_title();
        if (!panel) {                            /* the panel once; then its columns as they come */
            panel = 1;
            wv_draw_all();
        }
    }
    {                                            /* CHOP's REVERSE shows the slice in hand's */
        uint8_t r = (uint8_t)(set.crev >> chop_cur() & 1u);
        if (set.p[PA_CREV] != r) {
            set.p[PA_CREV] = r;
            ui.redraw = 1;
        }
    }
    {                                            /* CHOP's page: the sample in the panel; leaving it, the mix again */
        static uint32_t shown = 0xFFFFFFFFu;
        uint32_t lit = set.p[PA_CMODE] == CM_PITCHED ? set.p[PA_SLICE] % ch_next.nslices : ch.last_slice;
        uint32_t want = set.page == PG_CHOP ? (lit | ch_gen << 8) ^ set.crev * 2654435761u : 0xFFFFFFFFu;
        if (want != shown || ui.full) {
            if (set.page == PG_CHOP) {
                wv_held = 1;
                wv_draw_sample(&ch_next, lit, set.crev);
            } else {
                wv_held = 0;
                wv_draw_all();
            }
            shown = want;
        }
    }
    if (ui.redraw) {
        ui.redraw = 0;
        draw_header();
    }
    if (ui.redraw_row) {
        ui.redraw_row = 0;
        draw_row();
    }
    wv_draw_new();
    ui.full = 0;
}

/* ------------------------------------------------------------ companion --- */
/* the computer's side (fm1.toml companion = true: docs/COMPANION.md): the saved settings are a file a page can
 * download and send back (app.h APP_FILE_OBJS); the platform checks a file whole before it saves it */
int app_file_busy(void) { return 0; }   /* (a sample may come while it plays: the copy written is not the one played) */
int app_file(uint32_t i, plat_file_info_t *f)
{
    int n;
    if (i)
        return -1;
    n = plat_file_size(&CH_FILE);
    f->name = "chop";
    f->kind = PLAT_FK_SAMPLE;
    f->flags = PLAT_FF_READ | PLAT_FF_WRITE | PLAT_FF_DELETE | PLAT_FF_PLAYING;
    f->size = n > 0 ? (uint32_t)n : 0u;
    f->room = CH_FILE.n - PLAT_FILE_HEAD;
    return 0;
}
int app_file_read(uint32_t i, uint32_t off, uint8_t *dst, uint32_t n) { return i ? -1 : plat_file_read(&CH_FILE, off, dst, n); }
int app_file_begin(uint32_t i, uint32_t size)
{
    if (i)
        return -1;
    chop_factory();                          /* one copy: nothing plays from it while it is written */
    return plat_file_begin(&CH_FILE, size);
}
int app_file_write(uint32_t i, uint32_t off, const uint8_t *src, uint32_t n)
{
    return i ? -1 : plat_file_write(&CH_FILE, off, src, n);
}
int app_file_commit(uint32_t i, uint32_t size, uint32_t crc)
{
    int rc = i ? -1 : plat_file_commit(&CH_FILE, size, crc);
    if (!rc) {
        chop_load();
        set.crev = 0;                            /* (new slices: none backwards) */
        apply();
        app_say("CHOP", ch_name);
        ui.redraw_title = 1;
    }
    return rc;
}
int app_file_delete(uint32_t i)
{
    int rc = i ? -1 : plat_file_delete(&CH_FILE);
    if (!rc) {
        chop_load();
        app_say("CHOP", "FACTORY");
    }
    return rc;
}
/* the parameters for a computer (APP_PARAMS: the editor page, fm1_companion.py): every PA_*, its id its index (kept
 * from one release to the next: PA_* only grows at its end), grouped by the page that shows it, in the values the
 * screen shows (TEMPO in BPM, PITCH in semitones, SLICE from 1) */
static int32_t pa_off(uint32_t i)
{
    return PA[i].fmt == FMT_BPM ? 80 : PA[i].fmt == FMT_SEMI ? -12 : PA[i].fmt == FMT_ONE ? 1 : 0;
}
static uint32_t pa_page(uint32_t i)
{
    uint32_t g, k;
    for (g = 0; g < NPG; g++)
        for (k = 0; k < 5u; k++)
            if ((k < 4u ? PG[g].k[k] : PG[g].preset) == i)
                return g;
    return PG_SETUP;
}
int app_param(uint32_t i, plat_param_t *p)
{
    static const char *const UNIT[] = {[FMT_PCT] = "%", [FMT_BPM] = "BPM", [FMT_MS] = "", [FMT_UP] = "BPM",
                                       [FMT_SEMI] = "st"};
    uint32_t f;
    if (i >= NPA)
        return -1;
    f = PA[i].fmt;
    p->id = (uint16_t)i;
    p->group = (uint8_t)pa_page(i);
    p->flags = 0;
    p->name = PA[i].name;
    p->unit = f < sizeof UNIT / sizeof UNIT[0] && UNIT[f] && !(f == FMT_PCT && PA[i].top != 100) ? UNIT[f] : "";
    p->type = f == FMT_LIST || f == FMT_KEY || f == FMT_BARS ? PLAT_PT_ENUM : PLAT_PT_NUMBER;
    p->labels = f == FMT_LIST ? PA[i].list : f == FMT_KEY ? KEY_NAME : f == FMT_BARS ? BARS_NAME : 0;
    p->min = pa_off(i);
    p->max = PA[i].top + pa_off(i);
    p->def = PA[i].def + pa_off(i);
    return 0;
}
const char *app_param_group(uint32_t g)
{
    return g < NPG ? (g == PG_HOME ? "Home" : PG[g].title) : 0;
}
int32_t app_param_get(uint32_t i) { return i < NPA ? set.p[i] + pa_off(i) : 0; }
int32_t app_param_set(uint32_t i, int32_t v)
{
    if (i >= NPA)
        return 0;
    param_set(i, v - pa_off(i));
    return set.p[i] + pa_off(i);
}

/* backup and restore (fm1.toml backup = true) */
int app_backup_playing(void) { return ck.run; }
void app_backup_flush(void) {}           /* (SAVE saves: a backup takes what was saved) */
void app_backup_hold(int on) { (void)on; }   /* (nothing saves by itself) */
void app_backup_restored(void)
{
    settings_load();
    set_dirty = 0;
    ui.full = 1;
}

void app_file_changed(uint32_t obj)      /* the computer replaced the settings: use them */
{
    if (obj != OBJ_SETTINGS)
        return;
    settings_load();
    set_dirty = 0;
    ui.redraw = ui.redraw_row = ui.redraw_title = 1;
}

#ifdef PLAT_HOST
int app_expect(const char *what, int *got)
{
    if (!strcmp(what, "octave"))
        *got = set.octave;
    else if (!strcmp(what, "note"))
        *got = last_note;
    else if (!strcmp(what, "dirty"))
        *got = set_dirty;
    else if (!strcmp(what, "crev"))          /* CHOP: the slices played backwards */
        *got = (int)set.crev;
    else if (!strcmp(what, "saver"))         /* the screensaver on */
        *got = sv.on;
    else if (!strcmp(what, "saver_frames"))  /* its film's frames (0: none) */
        *got = sv.p ? (int)sv.nf : 0;
    else if (!strcmp(what, "saver_bad")) {   /* the film decoded whole: the frames not as tools/again_saver.py made them */
        uint32_t f, bad = 0, i, k, c;
        for (f = 0; sv.p && f < sv.nf; f++) {
            c = 0xFFFFFFFFu;
            if (sv_decode(f))
                c = 0;
            else
                for (i = 0; i < SV_FB; i++)
                    for (c ^= sv_fb[i], k = 0; k < 8u; k++)
                        c = c >> 1 ^ (0xEDB88320u & (0u - (c & 1u)));
            bad += ~c != sv_u32(sv.p + 12u + 4u * sv.nf + 4u * f);
        }
        sv.at = 0;
        *got = (int)bad;
    }
    else if (!strcmp(what, "page"))
        *got = set.page;
    else if (!strcmp(what, "inst"))
        *got = set.inst;
    else if (!strcmp(what, "run"))
        *got = ck.run;
    else if (!strcmp(what, "bd"))            /* BUILD / DROP: 0 none, 1 armed, 2 building */
        *got = gr.bd;
    else if (!strcmp(what, "drops"))
        *got = (int)gr.drops;
    else if (!strcmp(what, "kp_voices")) {   /* KEYS' voices sounding */
        int i, c = 0;
        for (i = 0; i < (int)KP_NV; i++)
            c += kp.v[i].note >= 0;
        *got = c;
    } else if (!strncmp(what, "p", 1) && what[1] >= '0' && what[1] <= '9')   /* pN: parameter N */
        *got = set.p[atoi(what + 1) % NPA];
    else if (!strcmp(what, "vel"))           /* the last key's velocity and pressure */
        *got = ui.vel[ui.last_key];
    else if (!strcmp(what, "prs"))
        *got = ui.prs[ui.last_key];
    else if (!strncmp(what, "turns", 5))    /* turnsN: encoder N's count (plat.h EN_*) */
        *got = ui.turns[atoi(what + 5) % NE];
    else
        return 0;
    return 1;
}
#endif
