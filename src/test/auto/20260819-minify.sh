#!/bin/bash

EGGDEV=$EGG_SDK/out/eggdev

if [ -n "$EGG_TEST_FILTER" ] ; then
  if ! grep -q 20260819-minify <<<"$EGG_TEST_FILTER" ; then
    echo "EGG_TEST SKIP 20260819-minify"
    exit 0
  fi
fi

# Noticed during development of Just Below The Surface that if you mask a parameter name as the name of a lambda parameter,
# minify misassigns usages of that lambda parameter.
#
# This quick test happened to expose a different bug: "Expected identifier" at the second open paren, just before "a".
# ...easy fix, just need to stop on open paren during mf_js_is_lambda().
#
# But then an even differenter bug:
#   function c(d,e){return f=>d()+g=>e()}
# This is exposing the bug we sought: The second appearance of "d" and "e" should have been "f" and "g".
# But it also incorrectly dropped the parens around my lambdas! Should be `(f=>f)()`, certainly not `f=>d()` or even `f=>f()`.
# fyi: The fact that these lambdas are noop will not be caught by minify; it's not currently designed for that.
# ...fixed at mf_js_output_LAMBDA.
#
# NB: We get the same bug with `function(a){return a;}`.
#
RESULT=$($EGGDEV minify <<EOF
function sum(a, b) {
  return ((a) => a)(a) + ((b) => b)(b);
}
EOF
)
# This validation will be somewhat brittle, depending on specific choices for shortened variable names.
if [ "$RESULT" = "function c(d,e){return(f=>f)(d)+(g=>g)(e)}" ] ; then
  echo "EGG_TEST PASS 20260819-minify: Parameter name masked by lambda ok."
else
  echo "EGG_TEST FAIL 20260819-minify: Parameter name masked by lambda broken: $RESULT"
fi

# And a similar control case, where we *do* want the outer symbol to replace:
RESULT=$($EGGDEV minify <<EOF
function sum(a,b) {
  return (() => a)() + ((b) => b)(b);
}
EOF
)
if [ "$RESULT" = "function c(d,e){return(()=>d)()+(f=>f)(e)}" ] ; then
  echo "EGG_TEST PASS 20260819-minify: Lambda parameter masking control case ok."
else
  echo "EGG_TEST FAIL 20260819-minify: Lambda parameter masking control case failed: $RESULT"
fi
