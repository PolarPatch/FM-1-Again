/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 PolarPatch */
/* Wi-Fi and Ableton Link (fm1.toml wifi, wifi_ap, link; fm1-base's own stack and base/link/, GPL-3.0-only). The LINK
 * page: WI-FI (OFF, AUTO: the networks kept, given over USB with base/tools/fm1_remote.py wifi join SSID PASSWORD; AP:
 * the FM-1 is the network, its name and password on the page) and LINK (on: a peer of the session whenever there is
 * an address).
 *
 * With a peer in the session (and no MIDI clock coming in, which leads first), the session leads: its tempo is the
 * clock's (BUILD's RUSH waits), the clock is pulled onto its beat a little each frame, and its start and stop are
 * PLAY's: started, the groove begins at the next bar line, as Link's apps do. Alone, the FM-1 gives the session its
 * own tempo and PLAY. Part of app.c (before play_toggle). Main loop only. */

#define LK_Q 4000000                             /* a bar: four beats, in micro-beats */
static struct {
    uint8_t was_playing;                         /* the session's start / stop last seen */
    int64_t arm_ub;                              /* a start waiting for this bar line (micro-beats); -1 none */
    int32_t mpb;                                 /* the session's tempo last seen */
    uint32_t peers, state;                       /* (the page: drawn again when they change) */
} lk = {0, -1, 0, 0, 0};

/* WI-FI and LINK into the platform: at the start, and when they are turned */
static void net_apply(void)
{
    static const uint8_t MODE[3] = {WIFI_MODE_OFF, WIFI_MODE_AUTO, WIFI_MODE_AP};
    int m = MODE[set.p[PA_WIFI] % 3u];
    if (plat_wifi_get_mode() != m) {
        plat_link_enable(0);                     /* (Link opens its port and joins its group again on the new mode) */
        plat_wifi_mode(m);
    }
    plat_link_enable(set.p[PA_LINK] != 0u);
}

static int lk_with_peers(void) { return plat_link_on() && plat_link_peers() > 0u && plat_link_mpb() > 0; }

/* PLAY: with peers, the session starts and the groove waits for the bar line; alone, at once (and the session too) */
static int lk_play(int start)
{
    if (!plat_link_on())
        return 0;
    if (start && lk_with_peers()) {
        int64_t ub = plat_link_beat_at(plat_link_now());
        if (ub < 0)
            return 0;
        if (lk.arm_ub >= 0) {                    /* PLAY again while waiting: not after all */
            lk.arm_ub = -1;
            return 1;
        }
        lk.arm_ub = ub - plat_link_phase(ub, LK_Q) + LK_Q;
        if (!plat_link_playing())
            plat_link_set_playing(1);
        lk.was_playing = 1;
        return 1;                                /* (the groove starts at the bar line: lk_frame) */
    }
    plat_link_set_playing(start);
    lk.was_playing = (uint8_t)start;
    return 0;
}

/* the TEMPO knob turned (or a MIDI clock's tempo): the session's too */
static void lk_tempo(void)
{
    if (plat_link_on() && bpm_base > 0.0f)
        plat_link_set_tempo((int32_t)(60000000.0f / bpm_base + 0.5f));
}

/* where the clock is in its bar, in sixteenths 0 .. 16, as if without SWING (a pair of sixteenths keeps its length) */
static float lk_ours(void)
{
    uint32_t step = ck.step;
    float l0 = SR * 15.0f / ck.bpm, in = ck.len - ck.left;
    if (step == 0xFFFFFFFFu)
        return -1.0f;
    if (step & 1u)
        in += l0 * (1.0f + ck.swing);
    return (float)(step & 14u) + in / l0;
}

static void lk_frame(void)
{
    int64_t ub;
    int playing;
    uint32_t st = (uint32_t)plat_wifi_state() | (plat_link_on() ? 0x100u : 0u);
    if (lk.state != st || lk.peers != plat_link_peers()) {
        lk.state = st;
        lk.peers = plat_link_peers();
        if (set.page == PG_LINK)
            ui.redraw = ui.redraw_title = 1;
    }
    lk_lead = (uint8_t)(lk_with_peers() && !sy.here);
    if (!plat_link_on() || (ub = plat_link_beat_at(plat_link_now())) < 0) {
        lk.arm_ub = -1;
        return;
    }
    if (lk_lead) {                               /* the session's tempo: the clock's, and TEMPO's knob shows it */
        int32_t mpb = plat_link_mpb();
        bpm_base = 60000000.0f / (float)mpb;
        if (mpb != lk.mpb) {
            int32_t t = (int32_t)(bpm_base + 0.5f) - 80;
            lk.mpb = mpb;
            if (t >= 0 && t <= PA[PA_TEMPO].top && set.p[PA_TEMPO] != (uint8_t)t) {
                set.p[PA_TEMPO] = (uint8_t)t;    /* (not unsaved: the session's, not a turn) */
                ui.redraw_row = ui.redraw_title = 1;
            }
        }
    } else if (plat_link_mpb() > 0 && (int32_t)(60000000.0f / bpm_base + 0.5f) != plat_link_mpb()) {
        lk_tempo();                              /* alone, or a MIDI clock leads: ours is the session's */
        lk.mpb = plat_link_mpb();
    }
    playing = plat_link_playing();
    if (playing != lk.was_playing) {             /* a peer started or stopped the session */
        lk.was_playing = (uint8_t)playing;
        if (lk_lead && playing && !ck.run && lk.arm_ub < 0)
            lk.arm_ub = ub - plat_link_phase(ub, LK_Q) + LK_Q;
        else if (lk_lead && !playing && (ck.run || lk.arm_ub >= 0)) {
            lk.arm_ub = -1;
            if (ck.run) {
                ck.run = 0;
                want_stop = 1;
                app_say("STOPPED", "LINK");
            }
        }
    }
    if (lk.arm_ub >= 0) {                        /* a start at the bar line */
        if (!lk_lead || !plat_link_playing())
            lk.arm_ub = -1;
        else if (ub >= lk.arm_ub) {
            lk.arm_ub = -1;
            if (!ck.run) {
                ck_start();
                app_say("PLAYING", "LINK");
            }
        }
        return;
    }
    if (lk_lead && ck.run) {                     /* the clock onto the session's beat: a part of the way each frame */
        float theirs = (float)(int32_t)plat_link_phase(ub, LK_Q) * (16.0f / (float)LK_Q), ours = lk_ours(), err;
        if (ours < 0.0f)
            return;
        err = ours - theirs;
        if (err >= 8.0f)
            err -= 16.0f;
        else if (err < -8.0f)
            err += 16.0f;
        lk_nudge = err * (SR * 15.0f / ck.bpm) * (err > 1.0f || err < -1.0f ? 1.0f : 0.1f);
    }
}

/* the page's second line: how far the Wi-Fi got, the session; in MODE AP the network's name, its password and the
 * session by turns */
static void lk_status(char *t)
{
    static const char *const ST[] = {"Wi-Fi off", "Starting", "No network", "Joining", "Address", "Connected",
                                     "Failed"};
    char n[16];
    t[0] = 0;
    if (set.p[PA_WIFI] == 2u) {
        char nm[33], key[PLAT_WIFI_AP_KEY_MAX];
        nm[0] = key[0] = 0;
        plat_wifi_ap_name(nm);
        if (plat_wifi_ap_state() != WIFI_AP_UP)
            text_cat(t, plat_wifi_ap_state() == WIFI_AP_FAILED ? "AP failed" : "AP starting");
        else {
            uint32_t k = plat_ms() / 2000u % 3u;  /* its name, its password, the session: by turns */
            if (k == 1u && plat_wifi_ap_default_key(key) && key[0])
                text_cat(t, key);
            else if (k == 2u && plat_link_on()) {
                int_text((int32_t)plat_link_peers(), n);
                text_cat(t, n);
                text_cat(t, plat_link_peers() == 1u ? " peer" : " peers");
            } else
                text_cat(t, nm);
        }
        return;
    }
    if (plat_wifi_state() != WIFI_CONNECTED) {
        uint32_t s = (uint32_t)plat_wifi_state();
        text_cat(t, s == WIFI_FAILED && plat_wifi_why()[0] ? plat_wifi_why() : ST[s < 7u ? s : 0u]);
        return;
    }
    if (!plat_link_on()) {
        text_cat(t, set.p[PA_LINK] ? "Link starting" : "Link off");
        return;
    }
    int_text((int32_t)plat_link_peers(), n);
    text_cat(t, n);
    text_cat(t, plat_link_peers() == 1u ? " peer" : " peers");
}

/* the title bar's right: the address (MODE AP: the clients joined) */
static void lk_where(char *t)
{
    char n[8];
    uint32_t ip = plat_wifi_addr(), i;
    t[0] = 0;
    if (set.p[PA_WIFI] == 2u) {
        int_text((int32_t)plat_wifi_ap_clients(), n);
        text_cat(t, n);
        text_cat(t, " JOINED");
        return;
    }
    for (i = 0; ip && i < 4u; i++) {
        int_text((int32_t)(ip >> (24u - 8u * i) & 255u), n);
        text_cat(t, n);
        if (i < 3u)
            text_cat(t, ".");
    }
}
