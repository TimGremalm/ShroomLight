#!/bin/sh

# For outomatic reload of pyscript, run: find . -name "*py" | entr ./reload.sh
pgrep python3 | xargs kill -9; python3 shroomlight.py &

