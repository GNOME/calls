#!/bin/sh

#set -x
#echo "$@"

INPUT="$1"
OUTPUT0="$2"

BASENAME="$( basename "$INPUT" .xml )"
WD="$PWD"
DIR="$( dirname "$OUTPUT0" )"

cd "$DIR"
gdbus-codegen \
    --generate-c-code "gdbo-${BASENAME}" \
    --c-namespace GDBO \
    --interface-prefix org.ofono. \
    "${WD}/${INPUT}"
