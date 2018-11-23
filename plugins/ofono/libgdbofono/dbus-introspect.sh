#!/bin/bash

DEST="$1"
OBJ_PATH="$2"
METHOD="$3"
shift 3

dbus-send "$@" --print-reply --dest="$DEST" "$OBJ_PATH" "$METHOD" | \
    grep -v '^method return' | \
    sed -e 's/^[[:space:]]\+string "</</' \
        -e 's_</node>"_</node>_'
