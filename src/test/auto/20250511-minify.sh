#!/bin/bash

EGGDEV=$EGG_SDK/out/eggdev

if [ -n "$EGG_TEST_FILTER" ] ; then
  if ! grep -q 20250511-minify <<<"$EGG_TEST_FILTER" ; then
    echo "EGG_TEST SKIP 20250511-minify"
    exit 0
  fi
fi

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

# 2025-06-30: Was trying to test something else, and found accidentally that our expression optimizer is inappropriately forcing integer math sometimes.
RESULT=$($EGGDEV minify <<EOF
const lforate = 2048;
const tempo = 469;
const frequency = 256000 / (lforate * tempo);
EOF
)
if ( grep -q 'c=\.26652452' <<<$RESULT ) ; then
  echo "EGG_TEST PASS 20250511-minify.sh: Optimize mlt and div floatly."
else
  echo "EGG_TEST FAIL Minification appears to have evaluated mlt or div as ints: $RESULT"
fi

# 2025-06-30: What I'm actually after: (a/(b*c)) must retain the parens around (b*c).
# Oh for fuck's sake: function a(){return 123}const e=e();const e=e();const e=e();const e=e/e*e;
RESULT=$($EGGDEV minify <<EOF
function random() { return 123; }
const a = random();
const b = random();
const c = random();
const d = a / (b * c);
EOF
)
if ( grep -q 'e=e' <<<$RESULT ) ; then
  echo "EGG_TEST FAIL 20250511-minify.sh: What on earth is this? $RESULT"
else
  echo "EGG_TEST PASS 20250511-minify.sh: Using high-value minify names like 'a' is OK."
fi

# 2025-06-30: But seriously now: (a/(b*c)) must retain the parens around (b*c).
RESULT=$($EGGDEV minify <<EOF
function random() { return 123; }
const a = random();
const b = random();
const c = random();
const d = a / (b * c);
EOF
)
if ( grep -q '\(.\*.\);$' <<<$RESULT ) ; then
  echo "EGG_TEST FAIL 20250511-minify.sh: Dropped necessary parens: $RESULT"
else
  echo "EGG_TEST PASS 20250511-minify.sh: MLT vs DIV parens 'a/(b*c)'"
fi

# const gp=ge>>2&15;
# const gq=((ge&3)<<8|go)-512;
# console.log("wheel event ["+ge&3+","+go+"] = "+gq);
RESULT=$($EGGDEV minify <<EOF
function random() { return 123; }
const a = \`abc \${random() & random()} xyz\`;
EOF
)
if ( grep -q 'abc "+(' <<<$RESULT ) ; then
  echo "EGG_TEST PASS 20250511-minify.sh: Grave string units parenthesized."
else
  echo "EGG_TEST FAIL 20250511-minify.sh: Need parens in grave string unit: $RESULT"
fi
