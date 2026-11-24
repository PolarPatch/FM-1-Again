/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 PolarPatch */
/* The boot screen, as Arctic Scream p0larpatch's (its ui.c ui_boot: the same storm, tears, light and timing): FM-1 and
 * AGAIN in the pixfont at four times its size come out of the dark in a storm of torn pixel rows that settles into
 * the banner's glitch (some rows of each word torn and split into ice, plasma and violet), with a short burst now and
 * then; p0larpatch fades in under it in ice and the four colour lines draw out from their middles. BOOT_MS long, or
 * until the first touch. The pixfont: Press Start 2P (SIL OFL 1.1), app/gen/again_pixfont.h.
 * Part of app.c (the unity build). Main loop only. */
#include "gen/again_pixfont.h"

#define BOOT_MS 5600u
#define BOOT_FRAME_MS 55u                       /* a frame of the storm: ~18 a second */
#define BT_Y1 66                                /* FM-1's top, */
#define BT_Y2 110                               /* AGAIN's, */
#define BT_PY 162                               /* p0larpatch's line, */
#define BT_LY 196                               /* the lines' */
#define BT_TY (BT_Y1 - 4)                       /* the band the title is drawn in */
#define BT_TH (BT_Y2 + 36 - BT_TY)
#define BT_ICE RGB(0x22, 0xd3, 0xee)
#define BT_PLASMA RGB(0xff, 0x1a, 0x8c)
#define BT_VIOLET RGB(0xa7, 0x8b, 0xfa)
#define BT_GREEN RGB(0x3f, 0xb9, 0x50)
typedef struct { int8_t ya, yb, dx; uint8_t k; } bt_slice;   /* a word's rows ya .. yb (px), shifted dx, colour k */
static const uint16_t BT_K[] = {RGB(0xea, 0xf6, 0xfa), RGB(0xff, 0xff, 0xff), BT_ICE, BT_PLASMA, BT_VIOLET,
                                RGB(0xff, 0xe4, 0xf3), RGB(0xe2, 0xf7, 0xff)};   /* white, bright, ice, plasma, violet,
                                                                                * rose, frost */
static const uint16_t BT_LINES[4] = {BT_ICE, BT_GREEN, BT_VIOLET, BT_PLASMA};
static const bt_slice BT_W1[] = {{16, 20, -3, 2}, {16, 20, 4, 3}, {16, 20, 2, 1}, {20, 22, -4, 4}, {20, 24, 1, 6}};
static const bt_slice BT_W2[] = {{0, 4, -2, 3}, {0, 4, 4, 2}, {0, 4, 0, 1}, {6, 8, 4, 4}, {4, 8, -3, 2}, {4, 8, -2, 5}};
static const char *const BT_WORD[2] = {"FM-1", "AGAIN"};
static struct {
    uint8_t on;
    uint32_t t0, at, rnd, k, pk, ln;            /* since; the last frame; the dice; the title's light (0 .. 64);
                                                 * p0larpatch's (0 .. 8); the lines' length */
    int8_t dx[2][16];                           /* this frame's tears: each word's 2 px bands, their shift */
    uint8_t gk[2][16];                          /* (and their ghosts' colour) */
    int torn;
} bt;

static uint32_t bt_rnd(uint32_t n)
{
    bt.rnd ^= bt.rnd << 13, bt.rnd ^= bt.rnd >> 17, bt.rnd ^= bt.rnd << 5;
    return bt.rnd % n;
}
static uint16_t bt_mix(uint16_t a, uint16_t b, uint32_t t, uint32_t n)   /* RGB565 a .. b, t of n */
{
    uint32_t r = ((a >> 11) * (n - t) + (b >> 11) * t) / n, g = ((a >> 5 & 63u) * (n - t) + (b >> 5 & 63u) * t) / n,
             bl = ((a & 31u) * (n - t) + (b & 31u) * t) / n;
    return (uint16_t)(r << 11 | g << 5 | bl);
}
static uint16_t bt_c(uint32_t k) { return bt_mix(C_BLACK, BT_K[k], bt.k, 64u); }
/* a word's rows ya .. yb at canvas row y, shifted dx */
static void bt_rows(int32_t y, const char *s, int32_t ya, int32_t yb, int32_t dx, uint16_t c)
{
    int32_t n = 0, x, r, b, b0, t, u;
    while (s[n])
        n++;
    for (x = (240 - n * 32) / 2 + dx; *s; s++, x += 32) {
        const uint8_t *g = PIXFONT[(uint8_t)*s - 32u];
        for (r = ya / 4; r < 8 && r * 4 < yb; r++) {
            t = r * 4 > ya ? r * 4 : ya, u = r * 4 + 4 < yb ? r * 4 + 4 : yb;
            for (b = 0; b < 8; b = b0) {        /* (a run of pixels: one rect) */
                for (b0 = b; b0 < 8 && g[r] & (0x80u >> b0); b0++)
                    ;
                if (b0 > b)
                    cv_rect(x + b * 4, y + t, (b0 - b) * 4, u - t, c);
                else
                    b0++;
            }
        }
    }
}
static void bt_word(uint32_t w, int32_t y, const bt_slice *sl, uint32_t n)
{
    uint32_t i, b, keep = 0;
    int32_t dx;
    for (i = 0; i < n; i++)
        keep |= (0xFFFFu >> (16 - (sl[i].yb - sl[i].ya + 1) / 2)) << sl[i].ya / 2;
    for (b = 0; b < 16u; b++) {                 /* the word, but for the banner's torn rows: its bands, torn or not */
        if (keep >> b & 1u)
            continue;
        dx = bt.dx[w][b];
        if (dx)
            bt_rows(y, BT_WORD[w], (int32_t)b * 2, (int32_t)b * 2 + 2, dx + (dx > 0 ? 3 : -3), bt_c(bt.gk[w][b]));
        bt_rows(y, BT_WORD[w], (int32_t)b * 2, (int32_t)b * 2 + 2, dx, bt_c(dx ? 1u : 0u));
    }
    for (i = 0; i < n; i++)                     /* the banner's torn rows, behind first */
        bt_rows(y, BT_WORD[w], sl[i].ya, sl[i].yb, sl[i].dx + bt.dx[w][sl[i].ya / 2], bt_c(sl[i].k));
}
static void bt_draw_title(void)
{
    cv_begin(240, BT_TH, C_BLACK);
    bt_word(0, BT_Y1 - BT_TY, BT_W1, sizeof BT_W1 / sizeof BT_W1[0]);
    bt_word(1, BT_Y2 - BT_TY, BT_W2, sizeof BT_W2 / sizeof BT_W2[0]);
    cv_blit(0, BT_TY);
}
static void bt_draw_name(void)                  /* p0larpatch: the pixfont at twice its size */
{
    const char *s = "p0larpatch";
    int32_t x = 40, r, b;
    uint16_t c = bt_mix(C_BLACK, BT_ICE, bt.pk, 8u);
    cv_begin(240, 16, C_BLACK);
    for (; *s; s++, x += 16)
        for (r = 0; r < 8; r++)
            for (b = 0; b < 8; b++)
                if (PIXFONT[(uint8_t)*s - 32u][r] & (0x80u >> b))
                    cv_rect(x + b * 2, r * 2, 2, 2, c);
    cv_blit(0, BT_PY);
}
static void bt_draw_lines(void)                 /* four lines, 44 px, growing from their middles */
{
    uint32_t t;
    cv_begin(240, 2, C_BLACK);
    for (t = 0; t < 4u && bt.ln; t++)
        cv_rect(23 + (int32_t)t * 50 + 22 - (int32_t)bt.ln / 2, 0, (int32_t)bt.ln, 2, BT_LINES[t]);
    cv_blit(0, BT_LY);
}
/* the boot screen at e ms: the light, the storm's tears (Arctic Scream's bt_state) */
static void bt_state(uint32_t e)
{
    static const uint16_t BURST[] = {2750, 3900, 4650, 5200};   /* (short bursts once it settled, 140 ms each) */
    uint32_t g = 0, i, n, w, b, m;
    int32_t dx;
    if (e < 1700u)
        g = e < 300u ? 64u : 64u - (e - 300u) * 64u / 1400u;   /* the storm, dying down */
    for (i = 0; i < sizeof BURST / sizeof BURST[0]; i++)
        if (e >= BURST[i] && e < BURST[i] + 140u)
            g = 36u;
    bt.k = e < 250u ? 0u : e < 1100u ? (e - 250u) * 64u / 850u : 64u;
    if (g && bt_rnd(80) < g)
        bt.k = bt.k * (1u + bt_rnd(3)) / 4u;    /* (it flickers in the storm) */
    bt.torn = 0;
    for (w = 0; w < 2u; w++)
        for (b = 0; b < 16u; b++)
            bt.torn |= bt.dx[w][b], bt.dx[w][b] = 0;
    n = g ? 1u + g * 8u / 64u : bt_rnd(14) == 0;   /* (settled: a band jumps a pixel or two now and then) */
    m = g ? 3u + g * 22u / 64u : 2u;
    for (i = 0; i < n; i++) {
        w = bt_rnd(2), b = bt_rnd(16), dx = (int32_t)bt_rnd(2u * m + 1u) - (int32_t)m;
        bt.dx[w][b] = (int8_t)dx, bt.gk[w][b] = (uint8_t)(2u + bt_rnd(3));
        if (b < 15u && bt_rnd(2))
            bt.dx[w][b + 1] = (int8_t)dx, bt.gk[w][b + 1] = bt.gk[w][b];
        bt.torn = 1;
    }
    bt.torn |= g != 0;
    bt.pk = e < 1300u ? 0u : e < 2100u ? (e - 1300u) * 8u / 800u : 8u;
    bt.ln = e < 2000u ? 0u : e < 2800u ? (e - 2000u) * 44u / 800u : 44u;
}
static void boot_start(uint32_t now)
{
    bt.on = 1;
    bt.t0 = bt.at = now;
    bt.rnd = 0x9E3779B9u ^ now;
    lcd_fill(0, 0, 240, 240, C_BLACK);
    bt_state(0);
}
/* a frame of it; 0 once it is over */
static int boot_frame(uint32_t now)
{
    uint32_t k = bt.k, pk = bt.pk, ln = bt.ln;
    if (now - bt.t0 >= BOOT_MS) {
        bt.on = 0;
        return 0;
    }
    if (now - bt.at < BOOT_FRAME_MS)
        return 1;
    bt.at = now;
    bt_state(now - bt.t0);
    if (bt.torn || bt.k != k)
        bt_draw_title();
    if (bt.pk != pk)
        bt_draw_name();
    if (bt.ln != ln)
        bt_draw_lines();
    return 1;
}
