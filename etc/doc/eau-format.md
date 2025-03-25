# Egg Audio Format "EAU"

`song` and `sound` resources are both stored in the ROM as our private format EAU.

EAU is similar to MIDI, with these notable exceptions:
- Single track.
- Channel configuration and events are stored separately.
- Channels can't reconfigure during playback.
- Notes contain their duration, there is no "Note Off" event.
- Only Note, Wheel, and Delay events. No control change, pressure, sysex, etc.
- Timing mostly in milliseconds.
- The stored channel configuration is adequate to produce exactly the same sound every time; no external config necessary.
- Note On velocity reduces to 4 bits, and Off velocity is discarded.

## From MIDI

Meta 0x77 contains the full Channel Headers, from the first (Channel ID), and not including the 0xff Terminator.
If absent, our compiler will make up Channel Headers based on Program Change and other clues.
If Meta 0x77 is present, we don't guess anything from the MIDI events.

TODO: Need another Meta event or something, to indicate the loop position.

## EAU Binary

```
   4  Signature: "\0EAU"
   2  Tempo ms/qnote.
   2  Loop position, bytes into events stream.
 ...  Channel Headers:
         1  Channel ID 0..254.
         1  Trim.
         1  Pan 0..128..255 = left..center..right
         1  Mode, see below.
         2  Payload Length.
       ...  Payload.
         2  Post Length.
       ...  Post, see below.
   1  Channel Headers Terminator = 0xff
 ...  Events, see below.
```

Channels must be sorted by ID and duplicates are an error.

Mode 0 = NOOP.
Channel will be silent.
Likewise, any channel with trim==0 will be silent, will not be configured.

Mode 1 = DRUM.
Payload is zero or more of:
```
   1  Note ID.
   1  Trim lo.
   1  Trim hi.
   1  Pan.
   2  Length
 ...  EAU file.
```

Mode 2 = FM.
This becomes a plain wavetable channel as an edge case.
Payload may terminate anywhere between fields:
```
 ...  Level env.
 ...  Wave. (carrier)
 ...  Pitch env.
   2  Wheel range, cents.
u8.8  Mod rate.
u8.8  Mod range.
 ...  Range env.
u8.8  Range LFO rate, qnotes.
u0.8  Range LFO depth.
```

Mode 3 = SUB.
```
 ...  Level env.
   2  Width, hz.
   1  Stage count.
u8.8  Gain.
```

ENV:
```
   1  Flags:
         0x01 Velocity
         0x02 Initials
         0x04 Sustain
         0xf8 Reserved, illegal.
 (2)  Initial lo, if (Initials).
 (2)  Initial hi, if (Initials&&Velocity).
 (1)  Sustain index, if (Sustain). The initial point is not sustainable.
   1  Point count. 0..16, or 0..15 if (Sustain).
 ...  Points:
         2 Time lo, ms.
         2 Value lo.
       (2) Time hi, if (Velocity).
       (2) Value hi, if (Velocity).
```

WAVE:
```
   1  Shape: (0,1,2,3,4)=(sine,square,saw,triangle,fixedfm)
   1  Qualifier, depends on Shape.
   1  Harmonics count.
 ...  Harmonics, u0.16 each.
```

Post:
```
   1  Stage ID.
   1  Length.
 ...  Content.
```

Post Stage ID:
- 0: NOOP.
- 1: GAIN: u8.8 gain, u0.8 clip = 1.
- 2: DELAY: u8.8 period qnotes, u0.8 dry, u0.8 wet, u0.8 store, u0.8 feedback.
- 3: LOPASS: u16 mid hz.
- 4: HIPASS: u16 mid hz.
- 5: BPASS: u16 mid hz, u16 width hz.
- 6: NOTCH: u16 mid hz, u16 width hz.
- 7: WAVESHAPER: u0.16... levels. Positive side only, with an implicit leading zero.

Event:
```
00000000                   : EOF. Optional.
0ttttttt                   : SHORT DELAY. (t) ms. Nonzero.
1000tttt                   : LONG DELAY. ((t+1)*128) ms.
1001cccc nnnnnnnv vvvddddd : SHORT NOTE. (d) ms
1010cccc nnnnnnnv vvvddddd : MEDIUM NOTE. ((d+1)*32) ms
1011cccc nnnnnnnv vvvddddd : LONG NOTE. ((d+1)*1024) ms
1100cccc vvvvvvvv          : WHEEL. 0x80 by default.
1101xxxx                   : Reserved, illegal.
1110xxxx                   : Reserved, illegal.
1111xxxx                   : Reserved, illegal.
```

## EAU Text

Line-oriented text.
`#` starts a line comment, start of line only.
Comments must not contain curly brackets (or they must be balanced if present).

Top level is organized into blocks, and blocks can nest arbitrarily.
'{' must be the last character on its line, and '}' must be alone on its line, after trimming whitespace.

Once at top level, first block of the file:
```
globals {
  tempo MS_PER_QNOTE
}
```

One or two times at top level (after Channel Headers):
```
events {
  delay MS
  note CHID NOTEID VELOCITY DURMS # VELOCITY in 0..15
  wheel CHID VALUE                # VALUE in 0..128..255
}
```
If there's two `events` block, the loop point is between them.

Any other top-level block is a Channel Header:
```
CHID TRIM PAN MODE {
  # ...mode-specific fields...
  post {
    gain GAIN [CLIP]                      # Floats
    delay PERIOD DRY WET STORE FEEDBACK   # Floats
    lopass HZ
    hipass HZ
    bpass HZ WIDTH
    notch HZ WIDTH
    waveshaper U16...
    STAGEID HEXDUMP
  }
}
```
`post` may only appear at the end of the Channel Header.
Mode-specific fields have an exact order they must be specified, and you're not allowed to skip except at the end.

The entire mode-specific section may be omitted and instead: `modecfg HEXDUMP`

MODE = noop. No configuration.

MODE = drum.
Configuration is a block per note:
```
  note NOTEID [TRIMLO=0x80 [TRIMHI=0xff [PAN=0x80]] {
    # ...EAU-Text file...
  }
```

MODE = fm.
```
  level ENV
  wave WAVE
  pitchenv ENV: cents, bias 0x8000. Ignored if it's a single zero.
  wheel CENTS
  rate FLOAT
  range FLOAT
  rangeenv ENV: Ignored if it's a single zero.
  rangelforate FLOAT
  rangelfodepth FLOAT
```

MODE = sub.
```
  level ENV
  width HZ
  stagec INT
  gain FLOAT
```

ENV: `LEVEL[..HI] [TIME[..HI] LEVEL[..HI][*] ...]`
One LEVEL pair (not the initial) may be followed by `*` to indicate sustain.
TIME in ms.
LEVEL in 0..65535.

WAVE: `SHAPE [+QUALIFIER] [HARMONICS...]`
SHAPE is 0..255 or one of: sine square saw triangle fixedfm.
HARMONICS in 0..65535.
