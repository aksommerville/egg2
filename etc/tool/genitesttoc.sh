#!/bin/bash

if [ "$#" -lt 1 ] ; then
  echo "Usage: $0 OUTPUT INPUT..."
  exit 1
fi

DSTPATH="$1"
shift 1
TMPTOC=itest-tmp
rm -f "$DSTPATH" "$TMPTOC"
touch "$TMPTOC"

while [ "$#" -gt 0 ] ; do
  SRCPATH="$1"
  shift 1
  SRCBASE="$(basename "$SRCPATH")"
  nl -ba -s' ' "$SRCPATH" | sed -En 's/^ *([0-9]+) *(XXX_)?EGG_ITEST *\( *([0-9a-zA-Z_]+) *(,([^\)]*))?.*$/'"$SRCBASE"' \1 _\2 \3 \5/p' >>"$TMPTOC"
done

cat - >>"$DSTPATH" <<EOF
/* $DSTPATH
 * Generated at $(date).
 */
 
#ifndef EGG_ITEST_TOC_H
#define EGG_ITEST_TOC_H

EOF

cat "$TMPTOC" | while read F L I N T ; do
  echo "int $N();" >>"$DSTPATH"
done

cat - >>"$DSTPATH" <<EOF

static const struct egg_itest {
  int (*fn)();
  const char *name;
  const char *file;
  const char *tags;
  int line;
  int ignore;
} egg_itestv[]={
EOF

cat "$TMPTOC" | while read F L I N T ; do
  if [ "$I" = _XXX_ ] ; then
    IGNORE=1
  else
    IGNORE=0
  fi
  echo "  {$N,\"$N\",\"$F\",\"$T\",$L,$IGNORE}," >>"$DSTPATH"
done

cat - >>"$DSTPATH" <<EOF
};

#endif
EOF

rm "$TMPTOC"
