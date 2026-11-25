#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 PolarPatch
"""The one-minute demo video: what FM-1 Again plays (app/tests/demo60.txt, rendered by the host simulator) over the
USB002 tour's creative footage (screenscaver/*.mov: not GPL, see LICENSING.md), dithered black and white as the
screensaver is, cut to the bars. An intro of FM-1 AGAIN glitching in big type that sinks into the dither, the parts
named as they play (KEYS, VOX, CHOP, the tricks, BUILD, DROP), each trick done to the picture too, and an end card.

    first: base/tools/build.py app --host; app/build/host/again_host app/tests/demo60.txt app/build/demo
    /usr/bin/python3 app/tools/make_demo_video.py [--out video/fm1-again-demo.mp4] [--preview 12]"""
import argparse, os, re, subprocess, sys
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CLIPS = os.path.join(ROOT, 'screenscaver')
FPS = 25
GW, GH = 640, 360                                  # the dither's grid; the video is 3 times it
BEAT = 60.0 / 124
BAR = 4 * BEAT
WHITE = np.array([255, 255, 255], np.uint8)
ICE, PLASMA, VIOLET, GREEN = (0x22, 0xd3, 0xee), (0xff, 0x1a, 0x8c), (0xa7, 0x8b, 0xfa), (0x3f, 0xb9, 0x50)


def pixfont():
    src = open(os.path.join(ROOT, 'app', 'gen', 'again_pixfont.h')).read()
    rows = re.findall(r'\{((?:0x[0-9A-F]{2}, ){7}0x[0-9A-F]{2})\}', src)
    return [[int(v, 16) for v in r.split(', ')] for r in rows]


FONT = pixfont()


def text_mask(s, scale):
    m = np.zeros((8, 8 * len(s)), bool)
    for i, ch in enumerate(s):
        g = FONT[ord(ch) - 32]
        for r in range(8):
            for b in range(8):
                m[r, i * 8 + b] = g[r] >> (7 - b) & 1
    return np.kron(m, np.ones((scale, scale), bool))


def bayer(n):
    m = np.array([[0]])
    while len(m) < n:
        k = len(m)
        m = np.block([[4 * m, 4 * m + 2], [4 * m + 3, 4 * m + 1]])
    return (m + 0.5) / m.size


THR = np.tile(bayer(4), (GH // 4, GW // 4))


# ---- the footage: shots, each a clip's stretch cut to 16:9 (centre cx, cy; zoom z), levels stretched
def shot(name, start, dur, cx=0.5, cy=0.5, z=1.0, gamma=1.0):
    path = os.path.join(CLIPS, name + '.mov')
    w, h = map(int, subprocess.run(['ffprobe', '-v', 'error', '-select_streams', 'v:0', '-show_entries',
                                    'stream=width,height', '-of', 'csv=p=0', path],
                                   capture_output=True, text=True).stdout.strip().split(','))
    cw = min(w, h * 16 / 9) / z
    ch = cw * 9 / 16
    x0 = min(max(cx * w - cw / 2, 0), w - cw)
    y0 = min(max(cy * h - ch / 2, 0), h - ch)
    vf = f'crop={int(cw)}:{int(ch)}:{int(x0)}:{int(y0)},scale={GW}:{GH}:flags=area,fps={FPS},format=gray'
    raw = subprocess.run(['ffmpeg', '-v', 'error', '-ss', str(start), '-t', str(dur), '-i', path, '-vf', vf,
                          '-f', 'rawvideo', '-'], capture_output=True, check=True).stdout
    f = np.frombuffer(raw, np.uint8).reshape(-1, GH, GW).astype(np.float32)
    lo, hi = np.percentile(f[::3, ::5, ::5], [2, 98])
    return np.clip((f - lo) / max(hi - lo, 1), 0, 1) ** gamma


class Shot:
    def __init__(self, *a, **k):
        self.a, self.k, self.f = a, k, None

    def at(self, s):                                # the frame s seconds in (clamped; negative: from its end)
        if self.f is None:
            self.f = shot(*self.a, **self.k)
        n = len(self.f)
        i = int(s * FPS) if s >= 0 else n + int(s * FPS)
        return self.f[min(max(i, 0), n - 1)]


SH = {
    'fog':     Shot('screen01', 0.6, 5.8),
    'dancer':  Shot('screen04', 0.0, 8.2),
    'shadow':  Shot('screen04', 8.4, 3.5),
    'hair':    Shot('screen03', 0.3, 15.5),
    'hands':   Shot('screen02', 0.0, 4.3, cy=0.5, z=1.25),        # (no one on the stage: the crowd only)
    'stage':   Shot('screen05', 11.5, 9.0, cy=0.0, z=1.9),         # (the cloth over the stage, no one under it)
    'arms':    Shot('screen02', 0.0, 4.3, cy=0.42),
    'fogcrowd': Shot('screen01', 4.6, 1.8, cy=0.7, z=1.3),
}


# ---- the edit: what plays when (seconds of the sound); each gives (grey image, caption, line 2)
PARTS = [  # start, name, what it says
    (0.0, 'INTRO', ''),
    (16 * BEAT, 'KEYS', 'SOFT FM PIANO: ONE KEY, A CHORD'),
    (32 * BEAT, 'VOX', 'SUNG SYLLABLES ON THE KEYS'),
    (48 * BEAT, 'CHOP', 'A RECORDING, CUT INTO SLICES'),
    (64 * BEAT, 'STUTTER 1/16', 'HOLD A BLACK KEY: A TRICK'),
    (66 * BEAT, 'REVERSE', 'THE LAST BEAT BACKWARDS'),
    (68 * BEAT, 'ECHO THROW', 'THE KEYS THROWN INTO THE ECHO'),
    (70 * BEAT, 'STUTTER 1/32', ''),
    (71 * BEAT, 'LOW SWEEP', 'A LOW-PASS CLOSING'),
    (74 * BEAT, 'DRUMS OUT', ''),
    (76 * BEAT, 'BUILD', 'ARP: FOUR BARS TO THE DROP'),
    (92 * BEAT, 'DROP', 'THE PUMP, THE ROOM, THE TAPE'),
    (116 * BEAT, 'TAPE STOP', ''),
    (120.5 * BEAT, 'END', ''),
]


def part(t):
    cur = PARTS[0]
    for p in PARTS:
        if t >= p[0]:
            cur = p
    return cur


DROP_CUTS = ['arms', 'hair', 'fogcrowd', 'dancer', 'hands', 'hair', 'fog', 'dancer', 'arms', 'hair', 'fogcrowd', 'hair']


def picture(t):
    """the grey picture at t, and the effects: (img, flash 0/1/2 (none, white, inverted), gain)"""
    b = t / BEAT
    name = part(t)[1]
    fi = int(round(t * FPS))
    on_beat = int(t / BEAT) != int((t - 1 / FPS) / BEAT)     # the first frame of a beat
    if name == 'INTRO':
        return SH['fog'].at(t * 0.75), 0
    if name == 'KEYS':
        return SH['dancer'].at(t - 16 * BEAT), 2 if on_beat and int(b) % 4 == 0 else 0
    if name == 'VOX':
        return SH['hair'].at(t - 32 * BEAT), 2 if on_beat and int(b) % 4 == 0 else 0
    if name == 'CHOP':                              # a cut each half beat, as the slices
        h = int((t - 48 * BEAT) / (BEAT / 2))
        src = [('hair', 8.0), ('shadow', 0.3), ('fogcrowd', 0.2), ('dancer', 5.0), ('hair', 11.0), ('shadow', 2.0),
               ('hands', 1.0), ('hair', 13.5)][h % 8]
        s = src[1] + (h // 8) * 0.6 + (t - 48 * BEAT - h * BEAT / 2)
        return SH[src[0]].at(s), (2 if h % 8 == 0 else 1) if on_beat else 0
    tt = t - 64 * BEAT
    if name == 'STUTTER 1/16':                      # the last sixteenth, round and round
        loop = BEAT / 4
        return SH['hands'].at(0.2 + (tt % loop) + (0 if tt < BEAT else 0.4)), 1 if fi % 3 == 0 else 0
    if name == 'REVERSE':
        return SH['hands'].at(1.0 + 2 * BEAT - (tt - 2 * BEAT)), 0
    if name == 'ECHO THROW':                        # the picture echoes: two ghosts a beat apart
        s = 1.2 + (tt - 4 * BEAT)
        a, g1, g2 = SH['hands'].at(s), SH['hands'].at(s - BEAT / 2), SH['hands'].at(s - BEAT)
        return np.maximum(a, np.maximum(g1 * 0.7, g2 * 0.45)), 0
    if name == 'STUTTER 1/32':
        loop = BEAT / 8
        return SH['hands'].at(2.2 + (tt - 6 * BEAT) % loop), 2 if fi % 2 else 0
    if name == 'LOW SWEEP':                         # softer and darker as it closes
        k = (tt - 7 * BEAT) / (3 * BEAT)
        img = SH['hands'].at(2.3 + (tt - 7 * BEAT) * 0.6)
        n = 1 + int(k * 10)
        ker = np.ones(2 * n + 1) / (2 * n + 1)
        img = np.apply_along_axis(lambda r: np.convolve(r, ker, 'same'), 1, img)
        img = np.apply_along_axis(lambda c: np.convolve(c, ker, 'same'), 0, img)
        return img * (1 - 0.6 * k), 0
    if name == 'DRUMS OUT':
        return SH['fogcrowd'].at(tt - 10 * BEAT) * 0.45, 0
    if name == 'BUILD':                             # closer and closer; the strobe faster each bar
        k = (t - 76 * BEAT) / (16 * BEAT)
        img = SH['stage'].at(t - 76 * BEAT)
        z = 1 + 0.8 * k * k
        h, w = int(GH / z), int(GW / z)
        y0, x0 = (GH - h) // 2, (GW - w) // 2
        img = img[y0:y0 + h, x0:x0 + w].repeat(int(np.ceil(z)) + 1, 0).repeat(int(np.ceil(z)) + 1, 1)
        img = img[np.linspace(0, img.shape[0] - 1, GH).astype(int)][:, np.linspace(0, img.shape[1] - 1, GW).astype(int)]
        bar = int(k * 4)
        every = [BEAT, BEAT / 2, BEAT / 4, 2 / FPS][min(bar, 3)]
        tb = t - 76 * BEAT
        fl = int(tb / every) != int((tb - 1 / FPS) / every)
        if t > 91.5 * BEAT:
            return img * 0, 0
        return img * (0.75 + 0.25 * k), (2 if bar >= 3 and fi % 2 else 1) if fl else 0
    if name == 'DROP':
        td = t - 92 * BEAT
        if td < 2 / FPS:
            return np.ones((GH, GW), np.float32), 0
        c = int(td / (2 * BEAT))
        sname = DROP_CUTS[c % len(DROP_CUTS)]
        img = SH[sname].at((td % (2 * BEAT)) + [0.0, 3.0, 0.0, 1.0, 1.5, 9.0, 0.3, 4.5, 2.0, 12.0, 0.6, 6.0][c % 12])
        return np.clip(img * 1.15, 0, 1), (2 if int(b) % 4 == 0 else 1) if on_beat else 0
    if name == 'TAPE STOP':                         # slower and slower to a stop, and dark
        ts = t - 116 * BEAT
        k = min(ts / (4 * BEAT), 1)
        s = 4.0 + ts * (1 - k / 2)
        return SH['hair'].at(s) * (1 - k), 0
    return np.zeros((GH, GW), np.float32), 0


# ---- the title, its storm and ghosts (as the boot screen: app/boot.h)
TITLE = text_mask('FM-1 AGAIN', 7)                 # 560 x 56 of the grid
TY, TX = (GH - TITLE.shape[0]) // 2 - 18, (GW - TITLE.shape[1]) // 2
GHOSTS = [(2.6 / 2.2, -24, ICE, [(15, 22, 0, 30), (53, 60, 40, 72), (86, 93, 66, 100)]),
          (3.1 / 2.2, 24, PLASMA, [(23, 30, 26, 54), (62, 69, 70, 100), (94, 100, 4, 36)]),
          (4.3 / 2.2, 15, VIOLET, [(41, 48, 52, 82), (79, 86, 14, 44)])]
rng = np.random.default_rng(7)
TEARS = rng.integers(-40, 41, size=(400, TITLE.shape[0] // 2))


def put(rgb, mask, y, x, col):
    h, w = mask.shape
    y0, x0 = max(y, 0), max(x, 0)
    y1, x1 = min(y + h, GH), min(x + w, GW)
    if y1 > y0 and x1 > x0:
        rgb[y0:y1, x0:x1][mask[y0 - y:y1 - y, x0 - x:x1 - x]] = col


def title(rgb, t, storm, light=1.0, ghosts=True, dith=None):
    """the title at t: torn rows while storm > 0; light (0..1) dithered when under 1"""
    m = TITLE
    if storm > 0:                                   # each 2-row band thrown sideways, less as it settles
        tear = TEARS[int(t * FPS) % len(TEARS)]
        out = np.zeros_like(m)
        for i, dx in enumerate(tear):
            dx = int(dx * storm * (1 if rng.random() < 0.6 + 0.4 * storm else 0))
            out[2 * i:2 * i + 2] = np.roll(m[2 * i:2 * i + 2], dx, axis=1)
        m = out
    if ghosts and light > 0.45:                     # a band of the title in a colour, beside it (drawn under it)
        for cyc, dx, col, bands in GHOSTS:
            pc = (t % cyc) / cyc * 100
            for a, b_, top, bot in bands:
                if a <= pc < b_:
                    g = np.zeros_like(TITLE)
                    y0, y1 = TITLE.shape[0] * top // 100, TITLE.shape[0] * bot // 100
                    g[y0:y1] = TITLE[y0:y1]
                    put(rgb, g, TY, TX + dx, np.array(col, np.uint8))
                    if storm > 0.2:                 # (the storm: thrown further)
                        put(rgb, g, TY, TX + 3 * dx, np.array(col, np.uint8))
    if light < 1:
        sub = THR[TY:TY + m.shape[0], TX:TX + m.shape[1]] < light
        m = m & sub
    put(rgb, m, TY, TX, WHITE)


def lines(rgb, k, y):
    """the four colour lines under the title, drawn out from their middles (k 0..1)"""
    cols = [ICE, GREEN, VIOLET, PLASMA]
    seg = TITLE.shape[1] // 4
    for i, c in enumerate(cols):
        mid = TX + i * seg + seg // 2
        half = int((seg - 8) / 2 * min(max(k, 0), 1))
        rgb[y:y + 3, mid - half:mid + half] = c


def label(rgb, s, s2):
    """the part's name, as the FM-1's lit knob: white, black type; the line under it small"""
    m = text_mask(s, 2)
    x, y = 16, GH - 16 - 22 - (12 if s2 else 0)
    rgb[y:y + 22, x:x + m.shape[1] + 10] = 255
    put(rgb, m, y + 3, x + 5, np.zeros(3, np.uint8))
    if s2:
        m2 = text_mask(s2, 1)
        rgb[y + 24:y + 36, x:x + m2.shape[1] + 6] = 0
        put(rgb, m2, y + 26, x + 3, WHITE)


def frame(t, end):
    name = part(t)[1]
    rgb = np.zeros((GH, GW, 3), np.uint8)
    if name == 'INTRO':
        # 0 .. 1 bar: the storm out of the dark; to 2 bars the title whole, ghosts torn in; then it sinks into the
        # dither and the crowd comes up behind it
        bg_k = min(max((t - 2 * BAR) / (2 * BAR), 0), 1)
        img = SH['fog'].at(0.25 + max(t - 2 * BAR, 0) * 0.45) * bg_k   # (the fog's bright crowd)
        rgb[img > THR] = 255
        storm = max(0, 1 - t / (0.9 * BAR)) if t > 0.15 else 0
        light = 1 - 0.85 * bg_k
        if t >= 0.15:
            title(rgb, t, storm, light if bg_k > 0 else min(1, t / 0.5))
        return rgb
    if name == 'END':
        te = t - PARTS[-1][0]
        title(rgb, te + 3, max(0, 1 - te / 0.6), 1)
        lines(rgb, (te - 0.4) / 0.8, TY + TITLE.shape[0] + 22)
        if te > 1.0:
            pk = min((te - 1.0) / 0.6, 1)
            m = text_mask('p0larpatch', 3)
            put(rgb, m & (THR[:m.shape[0], :m.shape[1]] < pk), TY + TITLE.shape[0] + 44, (GW - m.shape[1]) // 2,
                np.array(ICE, np.uint8))
        if te > 1.8:
            m = text_mask('A FIRMWARE FOR THE M-VAVE FM-1', 1)
            put(rgb, m, TY + TITLE.shape[0] + 82, (GW - m.shape[1]) // 2, np.array([150, 150, 150], np.uint8))
        if t > end - 0.8:                           # out to black
            rgb = (rgb * max(0, (end - t) / 0.8)).astype(np.uint8)
        return rgb
    img, fl = picture(t)
    if fl == 1:
        img = np.clip(img * 2.2 + 0.25, 0, 1)
    dots = img > THR
    if fl == 2:
        dots = ~dots
    rgb[dots] = 255
    p = part(t)
    if p[1] not in ('DROP', 'BUILD') or (p[1] == 'DROP' and t - p[0] < 2 * BAR) or (p[1] == 'BUILD' and t - p[0] < BAR):
        label(rgb, p[1], p[2])
    elif p[1] in ('BUILD', 'DROP'):
        label(rgb, p[1], '')
    return rgb


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--wav', default=os.path.join(ROOT, 'app', 'build', 'demo', 'again-demo-60.wav'))
    ap.add_argument('--out', default=os.path.join(ROOT, 'video', 'fm1-again-demo.mp4'))
    ap.add_argument('--preview', type=float, default=0, help='only the first N seconds (or from --start)')
    ap.add_argument('--start', type=float, default=0)
    a = ap.parse_args()
    end = PARTS[-1][0] + 3.6
    t0, t1 = a.start, (a.start + a.preview if a.preview else end)
    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    ff = subprocess.Popen(['ffmpeg', '-v', 'error', '-y', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-s', f'{GW}x{GH}',
                           '-r', str(FPS), '-i', '-', '-ss', str(t0), '-t', str(t1 - t0), '-i', a.wav,
                           '-vf', 'scale=1920:1080:flags=neighbor', '-c:v', 'libx264', '-pix_fmt', 'yuv420p',
                           '-crf', '16', '-preset', 'medium', '-c:a', 'aac', '-b:a', '256k',
                           '-af', f'afade=t=out:st={max(0, end - t0 - 1.5)}:d=1.5', '-shortest', '-movflags', '+faststart',
                           a.out], stdin=subprocess.PIPE)
    n = int((t1 - t0) * FPS)
    for i in range(n):
        ff.stdin.write(frame(t0 + i / FPS, end).tobytes())
        if i % 125 == 0:
            print(f'{t0 + i / FPS:5.1f} s', file=sys.stderr)
    ff.stdin.close()
    ff.wait()
    print(a.out)


if __name__ == '__main__':
    main()
