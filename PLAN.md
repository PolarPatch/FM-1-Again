# FM-1 Again: plan (2026-11-24)

## The idea
Arctic Scream is dark, slow and wide. FM-1 Again is the other side: warm, emotional, moving. Press PLAY and it plays a
short piece by itself; hold keys and play chopped voices over chords; a button lets the drop fall.

## Decided
- **Platform: fm1-base** (Charles Vestal, MPL-2.0) on felucca-bsp, as the git submodule `base/`; the app in `app/`
  (copied from `base/template/`). Not a fork of GR!MM / Arctic Scream: Arctic Scream's modules (sampler, FM6, SPACE,
  TAPE, GRAIN, DRIVE, sequencer, ...) are brought in one at a time as they are needed. They are GPL-3.0, so the
  firmware as a whole is GPL-3.0; fm1-base's files stay MPL-2.0 file by file.
- **Identity:** `id_prefix = "FM-1_13370"` (dev `FM-1_1337000`, releases `FM-1_13370XXYYZZ`). `FM-1_1337` itself is
  reserved by FM1 DRUM OS in the hub's catalogue; checked against every `ident` there on 2026-11-24: no match.
  USB: "FM-1 Again" by "PolarPatch". Settings magic "AGN1".
- **Versioning:** its own line from 1.0.
- **Screen:** big bold type, black and white (the user, 2026-11-24: "primarily white and black"; loaf-breaker's
  layout is liked, its colours are too many). TeX Gyre Heros Bold with stamped letters after the USB002 posters; the
  waveform in an audio editor's blues is the one colour. Pages: the header (page / bar), the waveform, K1-K4 along
  the bottom (loaf-breaker's idea).
- **Bluetooth on fm1-base's own stack** (`ble_stack = "own"`): no JieLi libraries linked, so GPL modules may join,
  and no AC79 SDK needed to build.
- **E-piano:** an FM voice of our own in float (2026-11-24), no instrument's patches. Arctic Scream's FM6 port may
  come later if more is wanted.

## The parts
1. **CHOP**: a sample (a voice, a street, a room) cut into slices over the keys, each pitched; slice by transient or
   evenly. Our own recordings via the editor / upload tool.
2. **KEYS**: soft e-piano chords on FM6, wide, with a warm room (SPACE).
3. **PUMP**: a sidechain from the kick (or the clock) on everything else, depth and release on knobs.
4. **GROOVE**: drum patterns with swing / UK garage shuffle: kick, ghost snare, shaker, hats.
5. **BUILD / DROP**: a filter sweep and riser over a bar count, the drop on a button or on its own.

## Steps
1. ~~git init, fm1-base as submodule, the template as `app/`, the identity~~ (2026-11-24).
2. ~~The screen: black and white, big poster type~~ (2026-11-24).
3. ~~PUMP, CHOP, GROOVE, KEYS, BUILD/DROP, VOX; SPACE, ECHO, TAPE from Arctic Scream; PERFORM~~ (2026-11-24).
4. ~~RISE (a reversed stab made forwards) and ECHO's REVERSE mode~~ (2026-11-24, after the user's reference clip).
5. ~~loaf-breaker's ideas: pads on the black keys and their strip, page tabs, beat ticks, SONG, build styles and
   RUSH, step conditions and VARY, the tips~~ (2026-11-24).
6. ~~First flashes on the spare FM-1~~ (2026-11-24: runs well, CPU 53 % playing; the user: "helt nydelig").
   The REVERSE sample (app/tools/make_reverse.py) is on the FM-1 as CHOP's sample.

## Where it stands (2026-11-24, end of day 1)
- Host tests: `app/tests/basic.txt groove.txt perform.txt rise.txt loaf.txt` (all pass). Flash with a venv that has
  mido + python-rtmidi: `python base/tools/fm1_install.py app/build/again.fwsc --port Felucca --yes`.
- The browser emulator builds with `source ~/emsdk/emsdk_env.sh` first (`base/tools/build.py app --web`).
- Pool 313 of 344 KB used: new audio buffers need room found first.

## Next
- **The screensaver (built, 2026-11-24 evening; not flashed yet):** `app/saver.h`, film `app/media/again_saver.bin`
  in the resource region 0x97000..0xDBFFF (69 sectors). CHOP's sample moved to XDATA1 (0xE5000, one copy, 8.2 s at
  most; again_chop.py keeps "AT" off each sector's 0xF06). Made by `app/tools/again_saver.py` from `screenscaver/`
  clip 05 alone for now (the user, 2026-11-24: save room, more later; 01 and 02 are the next candidates, 58 KB
  together): 6 fps, 8x8 Bayer, black/white, sharpen 80 (bw6), 126 frames, 21 s, 216 KB of 282 KB. `tests/saver.txt`.
- More reference clips from the user to analyse and make sounds after (our own synthesis, never their audio).
- CPU check on the FM-1 with pads + chords + RISE together.
- CHOP: record into the sample; REVERSE per slice.
- The site and landing page (still the template's); a public repo only after asking.

## 1.0 (2026-11-24): released, on the hub's desk
- Released: github.com/PolarPatch/FM-1-Again, tag v1.0 (built and attested by zvenson/fm1-build@v1), site on gh-pages
  (polarpatch.github.io/FM-1-Again: front page with the emulator and six demos, cheat sheet, editor, installer).
- Submitted to thefirmwarehub.com by the user (2026-11-24). fmhub.json, docs/card.png, app/docs/img/screens.png
  (app/tools/make_shots.py from app/docs/shots.txt).
- Tested on the pink FM-1 with the release file: V15 -> 1.0; 1.0 -> Arctic Scream 2.1.6 -> 1.0; CHOP upload into XDATA1
  read back identical; parameters set live; FM-1 Live's Screen room mirrors it. Not yet: an interrupted update,
  backup/restore on the device, the editor and the demos in a real Chrome.
- Both flash chips accepted (fm1-base's FM1B_FLASH_JEDEC_OK; the loader checks none).
- Known: base/tools/fm1_install.py from V15 loses the FM-1 when the port renames (FM-1 -> Felucca) before writing the
  resources; the package's loader writes them anyway (fm1_resources.py by hand fixed it). Worth an issue on fm1-base.
- The public history starts at one squashed commit (no CLAUDE.md); the old history is the local branch local-history.

## Next
- Demo samples from the user (tomorrow); CHOP's factory phrase may be replaced by one of them.
- Screensaver clips 01 and 02 (58 KB) when wanted: app/tools/again_saver.py, 66 KB room left.
- 1.0.x: small, frequent releases (tag vX.Y.Z on main; then app/tools/make_again_site.py X.Y.Z --publish).

## Open questions for the user
- Which sounds to record for the first factory samples?
- Repo name (`fm1.toml` assumes PolarPatch/FM-1-Again)?
