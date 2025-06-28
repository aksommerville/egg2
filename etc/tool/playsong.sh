#!/bin/sh
# etc/tool/playsong.sh
# Take a MIDI file from the demo, convert to EAU-Text if needed, and play it in a loop.
# For testing synth features. After the first run, you can modify the EAU-Text manually and just rerun.

if [ "$#" -ne 1 ] ; then
  echo "Usage: $0 SONG_NAME"
  exit 1
fi
SONG_NAME=$1

# Full 'make' always.
make || exit 1

# Convert from MIDI to EAU-Text only if the EAU-Text file doesn't exist yet.
if [ ! -f mid/playsong/$SONG_NAME.eaut ] ; then
  mkdir -p mid/playsong
  out/eggdev convert -omid/playsong/$SONG_NAME.eaut src/demo/src/data/song/*-$SONG_NAME.mid || exit 1
fi

# Convert EAU-Text to EAU.
out/eggdev convert -omid/playsong/$SONG_NAME.eau mid/playsong/$SONG_NAME.eaut || exit 1

# Play in a loop.
out/eggstra play mid/playsong/$SONG_NAME.eau --repeat --rate=44100 --chanc=2
