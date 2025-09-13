#!/bin/bash
# Produces local/config.mk, possibly after interactive prompts.

if [ "$#" -ne 1 ] ; then
  echo "Usage: $0 DSTPATH"
  exit 1
fi

echo "***********************************************"
echo "* Egg Build Config Wizard"
echo "***********************************************"

DSTPATH="$1"
mkdir -p $(dirname "$DSTPATH") || exit 1

# We will definitely need these three globals.
EGG_SDK=
EGG_TARGETS=
EGG_NATIVE_TARGET=
# Then for each target: OPT_ENABLE AR CC LD LDPOST PACKAGING
# And a special target "eggdev": OPT_ENABLE CC LD LDPOST

# Prefer `realpath` if available. On MacOS <=12 it is not.
if ( which realpath ) ; then
  FULLPATH="$(realpath $0)"
else
  FULLPATH="$PWD/$0"
fi
EGG_SDK="${FULLPATH%/etc/tool/genbuildconfig.sh}"

#-----------------------------------------------------------------------------------------
# Identify the native target. It's not mandatory.

POSSIBLE_TARGETS="$(ls etc/make | sed 's/\([a-zA-Z0-9_]*\)\.mk/\1/g;/demo/d;/eggdev/d;/test/d;/web/d' | tr '\n' ' ')"

UNAMES="$(uname -s)"
case "$UNAMES" in
  Linux) EGG_NATIVE_TARGET=linux ;;
  Darwin) EGG_NATIVE_TARGET=macos ;;
  MINGW*) EGG_NATIVE_TARGET=mswin ;;
  *)
    echo "Unable to determine native target from 'uname -s' = '$UNAMES'."
    echo "Options: $POSSIBLE_TARGETS"
    read -p "Native target: " EGG_NATIVE_TARGET
  ;;
esac

EGG_TARGETS="$EGG_NATIVE_TARGET"

#-----------------------------------------------------------------------------------------
# Test whether clang can compile for wasm32. If so, add the "web" target.

if (
clang --target=wasm32 -c -xc -otmp.o - >/dev/null 2>&1 <<EOF
int main(int argc,char **argv) { return 0; }
EOF
) ; then
  EGG_TARGETS="$EGG_TARGETS web"
  web_OPT_ENABLE="stdlib res graf"
  web_AR=ar
  web_CC="clang -c -MMD -O3 --target=wasm32 -nostdlib -Werror -Wno-comment -Wno-parentheses -Isrc -Wno-incompatible-library-redeclaration -Wno-builtin-requires-header"
  web_LD="wasm-ld --no-entry -z stack-size=4194304 --no-gc-sections --allow-undefined --export-table   --export=egg_client_init --export=egg_client_quit --export=egg_client_update --export=egg_client_render"
  web_LDPOST=
  web_PACKAGING=web
  # EXESFX,WAMR_SDK are not meaningful with "web" packaging, but we define them for the sake of uniformity.
  web_EXESFX=
  web_WAMR_SDK=
else
  echo "clang does not appear to support the 'wasm32' target. This Egg installation will not build for web."
fi
rm -f tmp.o

#-----------------------------------------------------------------------------------------
# Allow manual selection of additional targets.

echo "Available targets: $POSSIBLE_TARGETS"
echo "Already selected: $EGG_TARGETS"
read -p "Select additional targets: " ADDL
EGG_TARGETS="$EGG_TARGETS $ADDL"

#-----------------------------------------------------------------------------------------
# Prepare config for each target.

STDRTOPT="fs serial synth res graf hostio render image real_stdlib"

for TARGET in $EGG_TARGETS ; do
  case "$TARGET" in
    web) ;; # Already configured.
    
    macos)
      macos_OPT_ENABLE="$STDRTOPT macos macaudio machid macwm"
      macos_AR="ar"
      macos_CC="gcc -c -MMD -O3 -Isrc -Werror -Wimplicit -Wno-comment -Wno-parentheses"
      macos_LD="gcc -z noexecstack"
      macos_LDPOST="-lm -lz -framework OpenGL -framework CoreGraphics -framework IOKit -framework AudioUnit -framework Cocoa -framework Quartz"
      macos_PACKAGING=macos
      macos_EXESFX=
      macos_WAMR_SDK=
    ;;
    
    mswin)
      mswin_OPT_ENABLE="$STDRTOPT mswin"
      mswin_AR="ar"
      mswin_CC="gcc -c -MMD -O3 -Isrc -m32 -mwindows -Werror -Wimplicit -Wno-comment -Wno-parentheses -D_WIN32_WINNT=0x0501 -D_POSIX_THREAD_SAFE_FUNCTIONS"
      mswin_LD="gcc -z noexecstack"
      mswin_LDPOST="-lm -lz -lopengl32 -lwinmm -lhid"
      mswin_PACKAGING=mswin
      mswin_EXESFX=.exe
      mswin_WAMR_SDK=
    ;;
    
    linux)
      linux_OPT_ENABLE="$STDRTOPT evdev alsafd"
      linux_AR=ar
      linux_CC="gcc -c -MMD -O3 -Isrc -Werror -Wimplicit"
      linux_LD="gcc -z noexecstack"
      linux_LDPOST="-lm -lz"
      linux_PACKAGING=exe
      linux_EXESFX=
      linux_WAMR_SDK=
      # Select other drivers based on the available headers:
      if [ -f /usr/include/asoundlib.h ] ; then
        linux_OPT_ENABLE="$linux_OPT_ENABLE asound"
        linux_LDPOST="$linux_LDPOST -lasound"
      fi
      if [ -d /usr/include/pulse ] ; then
        linux_OPT_ENABLE="$linux_OPT_ENABLE pulse"
        linux_LDPOST="$linux_LDPOST -lpulse-simple"
      fi
      if [ -d /usr/include/X11 ] ; then
        linux_OPT_ENABLE="$linux_OPT_ENABLE xegl"
        linux_LDPOST="$linux_LDPOST -lX11"
        if [ -f /usr/include/X11/extensions/Xinerama.h ] ; then
          linux_OPT_ENABLE="$linux_OPT_ENABLE xinerama"
          linux_LDPOST="$linux_LDPOST -lXinerama"
        fi
        NEED_EGL=1
        NEED_GLES2=1
      fi
      if [ -d /usr/include/libdrm ] ; then
        linux_OPT_ENABLE="$linux_OPT_ENABLE drmgx"
        linux_CC="$linux_CC -I/usr/include/libdrm"
        linux_LDPOST="$linux_LDPOST -ldrm -lgbm"
        NEED_EGL=1
        NEED_GLES2=1
      fi
      if [ -n "$NEED_EGL" ] ; then
        linux_LDPOST="$linux_LDPOST -lEGL"
      fi
      if [ -n "$NEED_GLES2" ] ; then
        linux_LDPOST="$linux_LDPOST -lGLESv2"
      fi
    ;;
    
    *)
      echo "WARNING: Unknown target '$TARGET'. Assuming a generic gcc configuration."
      declare "${TARGET}_OPT_ENABLE=$STDRTOPT"
      declare "${TARGET}_AR=ar"
      declare "${TARGET}_CC=gcc -c -MMD -O3 -Isrc"
      declare "${TARGET}_LD=gcc"
      declare "${TARGET}_LDPOST=-lm"
      declare "${TARGET}_PACKAGING=exe"
      declare "${TARGET}_EXESFX="
      declare "${TARGET}_WAMR_SDK="
    ;;
  esac
done

#-----------------------------------------------------------------------------------------
# Set eggdev up with hard-coded defaults.

eggdev_OPT_ENABLE="serial fs synth zip http image res real_stdlib eau"
eggdev_CC="gcc -c -MMD -O3 -Isrc -Werror -Wimplicit"
eggdev_LD="gcc -z noexecstack"
eggdev_LDPOST="-lm -lz"

#-----------------------------------------------------------------------------------------
# Done gathering data. Write it out.

rm -f "$DSTPATH"
cat - >>"$DSTPATH" <<EOF
# $DSTPATH
# Local configuration for building eggdev and the Egg runtime.
# This is not version-controlled, it's specific to this host.
# To rerun the build config wizard, just delete this file and 'make'.
# Values must not contain any dynamic expressions. They may be read by processes other than make (eg eggdev build).

export EGG_SDK:=$EGG_SDK
export EGG_TARGETS:=$EGG_TARGETS
export EGG_NATIVE_TARGET:=$EGG_NATIVE_TARGET

export eggdev_OPT_ENABLE:=$eggdev_OPT_ENABLE
export eggdev_CC:=$eggdev_CC
export eggdev_LD:=$eggdev_LD
export eggdev_LDPOST:=$eggdev_LDPOST
EOF

for TARGET in $EGG_TARGETS ; do
OPT_ENABLE=${TARGET}_OPT_ENABLE
AR=${TARGET}_AR
CC=${TARGET}_CC
LD=${TARGET}_LD
LDPOST=${TARGET}_LDPOST
PACKAGING=${TARGET}_PACKAGING
EXESFX=${TARGET}_EXESFX
WAMR_SDK=${TARGET}_WAMR_SDK
cat - >>"$DSTPATH" <<EOF

export ${TARGET}_OPT_ENABLE:=${!OPT_ENABLE}
export ${TARGET}_AR:=${!AR}
export ${TARGET}_CC:=${!CC}
export ${TARGET}_LD:=${!LD}
export ${TARGET}_LDPOST:=${!LDPOST}
export ${TARGET}_PACKAGING:=${!PACKAGING}
export ${TARGET}_EXESFX:=${!EXESFX}
export ${TARGET}_WAMR_SDK:=${!WAMR_SDK}
# Set WAMR_SDK to build eggrun. Get its source here: https://github.com/bytecodealliance/wasm-micro-runtime
EOF
done

echo "************************************************"
echo "* $0: Generated local build config at $DSTPATH"
echo "* Please review and modify as needed, then run 'make' again."
echo "* Will now report an error to make, to prevent the real build."
echo "***********************************************"
exit 1
