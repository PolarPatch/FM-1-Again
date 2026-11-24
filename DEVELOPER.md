# FM-1 Again: developer notes

How FM-1 Again is built and where it is going. The day-to-day log is PLAN.md; this file is what to follow up.

## The family

| Target | Repository | Platform | State |
| --- | --- | --- | --- |
| FM-1 | [PolarPatch/FM-1-Again](https://github.com/PolarPatch/FM-1-Again) (this one) | fm1-base (`base/`, main) | 1.0 released, on the Firmware Hub's desk |
| VIBE FM (VST3 / AU / CLAP, the Playground, the hardware: one `.wasm`) | [PolarPatch/FM-1-Again-VIBE](https://github.com/PolarPatch/FM-1-Again-VIBE) | the VIBE FM SDK | started; VIBE Box Challenge entry, closes 15 Dec 2026 |
| SMK-37 Pro, Elite, MKE-P37 | [PolarPatch/FM-1-Again-SMK-37](https://github.com/PolarPatch/FM-1-Again-SMK-37) | fm1-base with `board = "smk37"` | waits for fm1-base (below) |

One version line for all of them (FM-1 Again 1.x everywhere), as Arctic Scream does. The engine is this
repository's; the ports take it from here.

## The order

1. Done: **the boot screen** (`app/boot.h`: FM-1 / AGAIN in Arctic Scream's storm, the glitch of BlueWatch's CONFIG
   title around the letters, the four lines, p0larpatch).
2. Done: **the engine apart from the FM-1's UI** (`app/core.c`; the host's audio bit for bit the same) and the film's
   decoder (`app/film.h`): both shared with FM-1 Again VIBE through its `tools/sync_engine.sh`.
3. Done: **MIDI clock in and out** after Dinghy 0.6 (SYNC AUTO / INTERNAL, CLOCK OUT, the MIDI page; `tests/clock.txt`).
   Still to do from Dinghy's kit: the MIDI basics (channel, pitch bend, mod wheel, sustain, CC 7, all notes off).
4. **FM-1 Again VIBE** (before 15 Dec; most of it done 2026-11-25, see its DEVELOPER.md): sound in straight into CHOP, long samples, the 16 RGB pads as slices and
   tricks, the 480 x 320 screen in black and white, the DAW's tempo and automation, the stage for the screensaver's
   film, VIBE LINK (Dinghy's `kit_link.h`: the pink FM-1 as its controller). Read first: the SDK's `AGENTS.md`,
   `FROM-FM1.md`, the OberVave source; `vibe-check` must pass. The user submits it (their account, the challenge box).
5. **FM-1 Again SMK-37**, once fm1-base's SMK-37 board is in its main branch.

## To follow up

- **fm1-base's SMK-37 board** lives on its `smk37-inhouse` branch (last 2026-11-06: `board = "smk37"`,
  `model = pro|elite|mke`, `device/plat_smk37.c`, `smk37_analog.h`). Merging it into main is **Charles Vestal's**
  (the owner of charlesvestal/fm1-base), not ours. Check now and then:
  `git -C base fetch origin && git -C base log --oneline origin/main | grep -i smk`. When it is there: bump `base/`,
  and start the SMK-37 repository on it. Until then nothing of the SMK-37 is built here (one fm1-base, not two).
- **What the SMK-37 port can take from Arctic Scream SMK-37** (PolarPatch's own, GPL-3.0): the control layout
  (K1 MASTER, K2..K4 SELECT / ALGORITHM / PRESETS, K5..K8 KNOB 1..4, a fader takes over once it moves, the outer keys
  play below and above, OCTAVE- at power-on the USB rescue), the browser's SMK-37 panel (`web/smk_panel.js`,
  `smk37.html`, `test_smk37.mjs`), the backup advice. Not its board code: fm1-base brings its own.
- **The SMK-37's flash map differs** (fm1-base's PLAT_OWNER at 0xAF000 inside XDATA1; the calibration at 0xC1000 is
  never written): CHOP's sample and the screensaver's film need new places there.
- **fm1_install.py from V15** loses the FM-1 when its port renames (FM-1 -> Felucca) before writing the resource
  region; the package's loader writes it anyway. Worth an issue on fm1-base.
- **Not yet tested on the FM-1**: an interrupted update, backup and restore, the editor and the demos in a real Chrome.
- The Firmware Hub: FM-1 Again submitted 2026-11-24; its card is `fmhub.json` and `docs/card.png`.
