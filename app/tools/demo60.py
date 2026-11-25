#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 PolarPatch
"""The one-minute demo's performance, written to app/tests/demo60.txt (the host simulator's script; then
tools/make_demo_video.py puts it under the picture). 124 BPM, KEY A, CHORDS FOG (C add9 two bars, A minor 7 two
bars, round and round: bar b plays C when b % 4 < 2). Every note played is a tone of the chord under it, and the
keys play single notes (ONE KEY: SINGLE), so the groove's chords carry the harmony.

The parts, a bar each 4 beats: INTRO 0-3 (the filter opening), KEYS 4-7, VOX 8-11, CHOP 12-15, the tricks 16-18,
BUILD from 19 (ARP on beat 75), DROP 23-28, TAPE STOP 29.

    /usr/bin/python3 app/tools/demo60.py && app/build/host/again_host app/tests/demo60.txt app/build/demo"""
import os

BEAT = 60000 / 124
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# the keys (key 0 is F3): the white keys of A minor
G3, A3, B3, C4, D4, E4, F4, G4, A4, B4, C5, D5, E5, G5 = 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 26
# CHOP's factory slices (SLICES: a key, a slice) by the note they were sung on
CH_A4, CH_C5, CH_E5, CH_D5, CH_G4, CH_A4b, CH_C5b = 0, 4, 2, 23, 26, 7, 11

ev = []


def at(b, cmd):
    ev.append((b, cmd))


def hit(b, keys, beats, vel=100):
    for k in keys if isinstance(keys, (list, tuple)) else [keys]:
        at(b, f"key {k} down {vel}")
        at(b + beats * 0.92, f"key {k} up")


def phrase(b0, notes, vel=100):
    for b, k, l in notes:
        hit(b0 + b, k, l, vel)


# the intro: the filter opens over four bars
at(0, "tap PLAY")
for i in range(1, 7):
    at(i * 16 / 6 - 0.2, "turn K3 8 20")       # (8 detents 20 ms apart: 160 ms)

# KEYS (bars 4-7: C C Am Am): an electric piano line in thirds, answered, resolving onto C over A minor
phrase(16, [(0, [C4, E4], 1.5), (1.5, D4, 0.5), (2, [E4, G4], 1.0), (3.5, G4, 0.5),
            (4, A4, 0.75), (4.75, G4, 0.75), (5.5, E4, 2.5),
            (8, [A3, E4], 1.5), (9.5, C4, 0.5), (10, [E4, A4], 1.0), (11.5, G4, 0.5),
            (12, E4, 0.75), (12.75, D4, 0.75), (13.5, C4, 2.5)], 92)

# VOX (bars 8-11): a call in the middle, an answer that lifts, then it comes home
at(31.8, "turn ALGO 1 20")
phrase(32, [(0, E4, 0.5), (0.75, G4, 0.5), (1.5, A4, 1.0), (3, G4, 0.5), (3.5, E4, 0.5),
            (4, G4, 0.75), (4.75, E4, 0.75), (5.5, D4, 2.0),
            (8, E4, 0.5), (8.75, G4, 0.5), (9.5, A4, 1.0), (11, C5, 0.5), (11.5, A4, 0.5),
            (12, G4, 0.75), (12.75, A4, 0.75), (13.5, E4, 2.0)], 104)

# CHOP (bars 12-15): the sung slices as a garage vocal chop, with room between them
at(47.8, "turn ALGO 1 20")
for bar, (one, two) in enumerate([(CH_C5, CH_E5), (CH_G4, CH_D5), (CH_A4, CH_C5b), (CH_E5, CH_A4b)]):
    b = 48 + 4 * bar
    phrase(b, [(0, one, 0.5), (0.75, one, 0.25), (1.5, two, 0.75), (2.5, one, 0.25), (3, two, 0.5)], 110)

# the tricks, over VOX (bars 16-18: C C Am)
at(63.8, "turn ALGO -1 20")
at(64, "key 3 down 100"); phrase(64, [(0, G4, 0.5), (0.5, E4, 0.5)]); at(66, "key 3 up")           # STUTTER 1/16
hit(66, 8, 2)                                                                                        # REVERSE
at(68, "key 17 down 100"); phrase(68, [(0, A4, 1), (1, E4, 1)]); at(70, "key 17 up")                # ECHO THROW
phrase(70, [(0, C5, 0.5)]); hit(70, 5, 1)                                                           # STUTTER 1/32
hit(71, 15, 3)                                                                                       # LOW SWEEP
at(73.8, "turn ALGO -1 20"); hit(74, 25, 2)                                                               # DRUMS OUT
at(75, "tap ARP")

# BUILD (bars 19-22: Am C C Am): the keys climbing through the chords
phrase(76, [(0, [A3, E4], 3.8), (4, [C4, G4], 3.8), (8, [E4, C5], 3.8), (12, [A4, E5], 3.5)], 88)

# DROP (bars 23-28: Am C C Am Am C): garage stabs on the chord of each bar
STABS = [(0, 0.5), (1.5, 0.5), (2.5, 0.25), (3, 0.5)]
for bar in range(23, 29):
    pair = [E4, A4] if bar % 4 >= 2 else [E4, G4]
    top = [A4, C5] if bar % 4 >= 2 else [G4, C5]
    for i, (b, l) in enumerate(STABS):
        hit(bar * 4 + b, top if i == 0 else pair, l, 112 if i == 0 else 96)

# TAPE STOP (bar 29): everything slows to a stop
at(116, "key 10 down 100"); at(122, "tap PLAY"); at(123, "key 10 up")
at(127, "wavstop")

# the setup before the sound: CHORDS FOG, ONE KEY SINGLE (KEYS' page), the filter closed
head = ["master 4096", "wait 600", "tap EDIT", "wait 50", "turn K3 4", "turn PRESET -3", "wait 50", "tap HOME",
        "wait 50", "turn K3 -46", "wait 300", "wav again-demo-60.wav"]


def cost(cmd):
    """the simulated time a command itself runs (base/host/plat_host.c): the waits after it are that much shorter,
    or every note lands later than the last and the playing drifts off the groove"""
    w = cmd.split()
    if w[0] == "tap":
        return 80
    if w[0] in ("press", "release", "key"):
        return 2
    if w[0] == "turn":
        return abs(int(w[2])) * (int(w[3]) if len(w) > 3 else 80)
    return 0


ev.sort(key=lambda e: e[0])
out, t = [], 0                                  # t: the simulator's time since the sound began, ms
for b, cmd in ev:
    ms = round(b * BEAT)
    if ms > t:
        out.append(f"wait {ms - t}")
        t = ms
    out.append(cmd)
    t += cost(cmd)
open(os.path.join(ROOT, "tests", "demo60.txt"), "w").write(
    "# The 60 s demo's sound, made by tools/demo60.py (edit that, not this): again_host app/tests/demo60.txt "
    "app/build/demo\n" + "\n".join(head + out) + "\n")
print(f"tests/demo60.txt: {t / 1000:.1f} s")
