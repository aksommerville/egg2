#!/bin/bash
#TODO This is very temporary, just producing the same config every time.
# Replace with some intelligence and interactivity, to produce a sensible config for this host.

if [ "$#" -ne 1 ] ; then
  echo "Usage: $0 DSTPATH"
  exit 1
fi

DSTPATH="$1"
mkdir -p $(dirname "$DSTPATH") || exit 1

rm -f "$DSTPATH"
cat - >>"$DSTPATH" <<EOF
# $DSTPATH
# Local configuration for building eggdev and the Egg runtime.
# This is not version-controlled, it's specific to this host.
# To rerun the build config wizard, just delete this file and 'make'.

export EGG_SDK:=/home/andy/proj/egg
export EGG_TARGETS:=linux web
export EGG_NATIVE_TARGET:=linux

export eggdev_OPT_ENABLE:=serial fs midi synth
export eggdev_CC:=gcc -c -MMD -O3 -Isrc -Werror -Wimplicit \$(foreach U,\$(eggdev_OPT_ENABLE),-DUSE_\$U=1)
export eggdev_LD:=gcc -z noexecstack
export eggdev_LDPOST:=-lm -lz

export linux_OPT_ENABLE:=alsafd asound pulse xegl drm evdev
export linux_CC:=gcc -c -MMD -O3 -Isrc -Werror -Wimplicit \$(foreach U,\$(linux_OPT_ENABLE),-DUSE_\$U=1) -I/usr/include/libdrm
export linux_LD:=gcc -z noexecstack
export linux_LDPOST:=-lm -lz -lasound -lpulse-simple -lX11 -lGLESv2 -lEGL -lgbm -ldrm
export linux_PACKAGING:=exe

export web_OPT_ENABLE:=
export web_CC:=clang -c -MMD -O3 --target=wasm32 -nostdlib \
  -Werror -Wno-comment -Wno-parentheses -Isrc -Wno-incompatible-library-redeclaration -Wno-builtin-requires-header \
  \$(foreach U,\$(web_OPT_ENABLE),-DUSE_\$U=1)
export web_LD:=wasm-ld --no-entry -z stack-size=4194304 --no-gc-sections --allow-undefined --export-table \
  --export=egg_client_init --export=egg_client_quit --export=egg_client_update --export=egg_client_render
export web_LDPOST:=
export web_PACKAGING:=web
EOF

echo "************************************************"
echo "* $0: Generated local build config at $DSTPATH"
echo "* Please review and modify as needed, then run 'make' again."
echo "* Will now report an error to make, to prevent the real build."
echo "***********************************************"
exit 1
