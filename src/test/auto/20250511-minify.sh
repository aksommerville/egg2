#!/bin/bash

EGGDEV=$EGG_SDK/out/eggdev

# At mf_output.c, we had been checking for parens for OP nodes, but not for INDEX nodes.
# So there was some operator-precedence confusion around constructions like: (a || b)[c]
RESULT=$($EGGDEV minify <<EOF
const abc = "abc".split(); // We need some extra calls to "split" to trick minify into hoisting the name.
const xyz = "xyz".split();
const yum = "banana".split();
const [w, h] = (this.rt.rom.getMeta("fb") || "640x360").split("x").map(v => +v);
EOF
)
if ( grep -q '"640x360"\[' <<<$RESULT ) ; then
  echo "EGG_TEST FAIL Minification of index ignored child LOR: $RESULT"
elif ( grep -q '"640x360")\[' <<<$RESULT ) ; then
  echo "EGG_TEST PASS 20250511-minify.sh: INDEX vs OP parens"
else
  echo "EGG_TEST FAIL Minification did not produce expected success or failure text: $RESULT"
fi
