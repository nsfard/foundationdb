#!/bin/bash

# This script executes the flatbuffers
# fuzz tests. It runs a python script that
# generates random flatbuffers schemas and
# verifies that they can be read and written
# with our flatbuffers implementation and
# the reference implementation.
#
# It takes the following arguments
#  - path to the build directory
#  - a seed
#  - path to flatc
#  - path to fuzz test executable
#  - path to the flat_buffers_fuzz.py script
#  - path to the python executable

set -euo pipefail

WORK_DIR=`mktemp -d`

function cleanup {
  rm -rf "$WORK_DIR"
}

trap cleanup EXIT
trap exit INT

cd $WORK_DIR

BUILD_DIR=$1
_SEED=$2
FLATC=$3
FUZZ=$4
FUZZ_PY=$5
PYTHON_EXE=$6

TABLE_FBS=$BUILD_DIR/table.fbs
DATA_JSON=data.json
DATA_BIN=data.bin
OUT_BIN=out.bin
OUT_JSON=out.json
TMP_CPP=tmp.h

for i in `seq 100` ; do
    $PYTHON_EXE $FUZZ_PY $_SEED data $i > $DATA_JSON
    $FLATC -b $TABLE_FBS $DATA_JSON
    rm $DATA_JSON
    $FUZZ $DATA_BIN $OUT_BIN
    $FLATC -t $TABLE_FBS --raw-binary --strict-json --defaults-json -- $DATA_BIN
    $FLATC -t $TABLE_FBS --raw-binary --strict-json --defaults-json -- $OUT_BIN
    diff -u <($PYTHON_EXE -m json.tool $DATA_JSON) <($PYTHON_EXE -m json.tool $OUT_JSON)
    rm *
done
