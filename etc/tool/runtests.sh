#!/bin/bash

# Empty for plain text.
COLOR=1

FAILC=0
PASSC=0
SKIPC=0

exe() {
  true
}

pass() {
  PASSC=$((PASSC+1))
  if [ -n "$COLOR" ] ; then
    echo -e "\x1b[32mPASS\x1b[0m $1"
  else
    echo "PASS $1"
  fi
}

fail() {
  FAILC=$((FAILC+1))
  if [ -n "$COLOR" ] ; then
    echo -e "\x1b[31mFAIL\x1b[0m $1"
  else
    echo "FAIL $1"
  fi
}

skip() {
  SKIPC=$((SKIPC+1))
  if [ -n "$COLOR" ] ; then
    echo -e "\x1b[90mSKIP\x1b[0m $1"
  else
    echo "SKIP $1"
  fi
}

detail() {
  if [ -n "$COLOR" ] ; then
    echo -e "\x1b[31m$1\x1b[0m"
  else
    echo "$1"
  fi
}

loose() {
  echo "$1"
}

final() {
  if [ -n "$COLOR" ] ; then
    if [ "$FAILC" -gt 0 ] ; then FLAG="\x1b[41m    \x1b[0m"
    elif [ "$PASSC" -gt 0 ] ; then FLAG="\x1b[42m    \x1b[0m"
    else FLAG="\x1b[100m    \x1b[0m" ; fi
    echo -e "$FLAG $FAILC fail, $PASSC pass, $SKIPC skip"
  else
    if [ "$FAILC" -gt 0 ] ; then FLAG="[FAIL]"
    elif [ "$PASSC" -gt 0 ] ; then FLAG="[PASS]"
    else FLAG="[----]" ; fi
    echo "$FLAG $FAILC fail, $PASSC pass, $SKIPC skip"
  fi
}

for EXE in $* ; do
  exe "$EXE"
  while IFS= read INPUT ; do
    read INTRODUCER KW ARGS <<<"$INPUT"
    if [ "$INTRODUCER" = EGG_TEST ] ; then
      case "$KW" in
        PASS) pass "$ARGS" ;;
        FAIL) fail "$ARGS" ;;
        SKIP) skip "$ARGS" ;;
        DETAIL) detail "$ARGS" ;;
        *) loose "$INPUT" ;;
      esac
    else
      loose "$INPUT"
    fi
  done < <( "$EXE" 2>&1 || echo "EGG_TEST FAIL $EXE" )
done

final
