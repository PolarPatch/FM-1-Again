#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# FM-1 Again: the firmware (app/) on fm1-base (base/). Arguments go to base/tools/build.py: --release X.Y, --host, --web.
#   ./build.sh                 app/build/again.fwsc (and again-res.bin, the screensaver's film)
#   ./build.sh --release 1.0   app/build/again-1.0.fwsc
set -e
cd "$(dirname "$0")"
exec "${PYTHON:-python3}" base/tools/build.py app "$@"
