#!/bin/bash

EGGDEV=$EGG_SDK/out/eggdev

if [ -n "$EGG_TEST_FILTER" ] ; then
  if ! grep -q 20250712-minify <<<"$EGG_TEST_FILTER" ; then
    echo "EGG_TEST SKIP 20250712-minify"
    exit 0
  fi
fi

# Input.js:209: `for (let i=0; i<2; i++) {` became `for (let h1 = 0; hZ < 2; hZ++) {`.
RESULT=$($EGGDEV minify <<EOF
let sum = 0;
for (let i=0; i<10; i++) {
  sum += i;
}
for (let i=0; i<10; i++) {
  sum += i;
}
EOF
)
echo "$RESULT" | sed -En 's/^.*([a-zA-Z0-9_]+)=0\;([a-zA-Z0-9_]+)<10.*$/\1 \2/p' | (
  read A B
  if [ "$A" = "$B" ] ; then
    echo "EGG_TEST PASS 20250712-minify.sh: Multiple declarations of the same variable ok, eg two 'for' loops."
  else
    echo "EGG_TEST FAIL 20250712-minify.sh: Two 'for' loops with the same LCV got all jumbled: $RESULT"
  fi
)
