# Egg Audio Format "EAU"

`song` and `sound` resources are both stored in the ROM as our private format EAU.

EAU is similar to MIDI, with these notable exceptions:
- Single track.
- Channel configuration and events are stored separately.
- Channels can't reconfigure during playback.
- Notes contain their duration, there is no "Note Off" event.
- A note can be held for no more than about 16 seconds.
- Delay is a standalone event, not attached to other events.
- Only Note, Wheel, and Delay events. No control change, pressure, sysex, etc.
- Timing in milliseconds.
- The stored channel configuration is adequate to produce exactly the same sound every time; no external config necessary.

## Song vs Sound

Song and sound resources are both EAU when shipped.
Pretty much anything allowed in one is allowed in the other, but:
- Only one song can play at a time.
- Sounds must mark their end time with explicit delays.
- Duration of a sound is strictly limited to 5 seconds.
- Sounds will be printed and replayed after the first time, while songs are synthesized from scratch each time. You shouldn't need to care.

## From MIDI

Meta 0x77 contains one Channel Header, ie a "CHDR" chunk. Meta 0x20 MIDI Channel Prefix is ignored, since the CHDR contains its own channel ID field.
If absent, our compiler will make up Channel Headers based on Program Change, SDK data, and other clues.
If Meta 0x77 is present, we don't guess anything from the MIDI events.

Meta 0x78 contains one Text Chunk, ie "TEXT".
Decoders should prefer 0x78 over standard events when they both appear, but I'm not imposing strict rules on precedence there.
Text is always optional anyway.

Meta 0x51 Set Tempo should appear once, at time zero.
If it appears mid-song, we'll process timing correctly, but the song's declared tempo will be undefined.

Meta 0x07 Cue Point with the payload "LOOP" to mark the loop point.

Meta 0x03 Track Name and 0x04 Instrument Name will be understood as channel names, if you use Meta 0x20 MIDI Channel Prefix.
We explicitly allow channels 0..255 for this, not just 0..15.

Opening a MIDI file in our editor and resaving it may destroy information.

## Instrument Conventions

During conversion, instruments are named by a Fully-Qualified Program Identifier or "fqpid".
This is a 21-bit integer composed of the Bank Select and Program Change events in MIDI.
Each group of 8 instruments should be related, continuing the GM convention.

fqpid 0..127 are GM. Repeating its groups (look up specific instrument names if you need them, it's all standard):
- 0x00..0x07: Piano
- 0x08..0x0f: Chromatic
- 0x10..0x17: Organ
- 0x18..0x1f: Guitar
- 0x20..0x27: Bass
- 0x28..0x2f: Solo String
- 0x30..0x37: String Ensemble
- 0x38..0x3f: Brass
- 0x40..0x47: Solo Reed
- 0x48..0x4f: Solo Flute
- 0x50..0x57: Synth Lead
- 0x58..0x5f: Synth Pad
- 0x60..0x67: Synth Effects
- 0x68..0x6f: World
- 0x70..0x77: Percussion
- 0x78..0x7f: Sound Effects

Egg extends that thru Bank One.
Higher banks can do what they want, but in cases of missing instruments, our compiler will try the low 8 bits of fqpid as one option (ie mirroring GM and Bank One).
- 0x80..0x87: Drum Kits
- - 0x80: Default Inoffensive Kit (GM Notes)
- - TODO
- 0x88..0x8f: Specialty Drum Kits (GM Notes)
- - 0x88: 8-Bit Kit
- - 0x89: Noise Kit
- - TODO
- 0x90..0x97: Sound Effects 1 (Drums but not GM)
- - 0x90: Happy Sounds
- - 0x91: Sci-Fi Sounds
- - 0x92: Natural Sounds
- - TODO
- 0x98..0x9f: Sound Effects 2 (Drums but not GM)
- - TODO
- 0xa0..0xa7: Tuned Drums 1 (Instruments, not drum kits)
- - 0xa0: Natural Tom 1
- - 0xa1: Natural Tom 2
- - 0xa2: Synth Tom 1
- - 0xa3: Synth Tom 2
- - 0xa4: Chirp 1
- - 0xa5: Chirp 2
- - 0xa6: Tick 1
- - 0xa7: Tick 2
- 0xa8..0xaf: Tuned Drums 2 (Instruments, not drum kits)
- - 0xa8: Clang 1
- - 0xa9: Clang 2
- - 0xaa: Thump 1
- - 0xab: Thump 2
- - TODO
- 0xb0..0xb7: Simple Synth
- - 0xb0: Sine Lead
- - 0xb1: Square Lead
- - 0xb2: Saw Lead
- - 0xb3: Triangle Lead
- - 0xb4: Sine Pad
- - 0xb5: Square Pad
- - 0xb6: Saw Pad
- - 0xb7: Triangle Pad
- 0xb8..0xbf: Lead Guitar
- - 0xb8: Clean Delay Guitar
- - 0xb9: Clean Wah Lead
- - 0xba: Rock Lead
- - 0xbb: Wah Lead
- - 0xbc: Brain Melting Lead
- - 0xbd: Panty Ripping Lead
- - 0xbe: Stutter Lead
- - 0xbf: Flat Lead Guitar
- 0xc0..0xc7: Rhythm Guitar
- - TODO
- 0xc8..0xcf: Plucks
- - TODO
- 0xd0..0xd7: Noise
- - TODO
- 0xd8..0xdf: TODO
- 0xe0..0xe7: TODO
- 0xe8..0xef: TODO
- 0xf0..0xf7: TODO
- 0xf8..0xff: TODO

## EAU Binary

File can be understood as chunks with 8-byte headers:
```
  4 Chunk ID.
  4 Length.
```

The first chunk must have ID "\0EAU", and this also serves as the file signature.

Chunks defined here: "\0EAU" "CHDR" "EVTS" "TEXT"

Unknown chunks, and excess trailing data in any chunk, should be ignored.
Each chunk type may have its own rules for short data, but in general short chunks are allowed if they break at a field boundary.

### Chunk "\0EAU"

Required, once only, and must be the first chunk.

```
  2 Tempo ms/qnote, >0 =500.
  2 Loop position, bytes into "EVTS" =0.
```

### Chunk "CHDR"

Optional. Forbidden after "EVTS".
Events for a channel with no header will be ignored.
Decoder behavior re duplicate chid is undefined. Use the first or last or fail.
An empty CHDR is technically legal but meaningless.
CHDR for channels 16 and above are perfectly legal. We may use EAU and EAU-Text as instrument repositories.
In MIDI files, this payload is repeated verbatim as Meta 0x77.

```
  1 Channel ID 0..255, but only 0..15 are addressable.
  1 Trim 0..255 =0x40.
  1 Pan 0..128..255 = left..center..right =128.
  1 Mode =2.
  2 Modecfg length.
  ... Modecfg.
  2 Post length.
  ... Post.
```

### Chunk "EVTS"

May only appear once, and only after "\0EAU" and all "CHDR".
Single-appearance is a strict rule on technical grounds: the runtime wants to point directly into the ROM for this and read it live.
It is legal to omit EVTS, in which case our runtime will insert a long delay. (4096 ms in both implementations, but let's not mandate that exact number).

Zero or more events, distinguishable from high bits of the first byte:
```
  00tttttt                            : Delay (t) ms. Zero is noop.
  01tttttt                            : Delay ((t+1)*64) ms
  10ccccnn nnnnnvvv vvvvdddd dddddddd : Note (n) on channel (c), velocity (v), duration (d*4) ms.
  11ccccww wwwwwwww                   : Wheel on channel (c), (w)=0..512..1023 = -1..0..1
```

### Chunk "TEXT"

Optional. Tooling is expected to strip TEXT chunks at the final build.
May appear zero or one times, any more is an error.

Body is zero or more of:
```
  1 chid
  1 noteid
  1 length
  ... text
```

Each entry names a channel (noteid==0) or one note of a channel, presumably drums.
Technically, zero is a valid noteid, but in MIDI it's not, and mehhhhh just don't use it, ok?
We're not imposing any rules about collisions, do whatever is easiest.

### Channel mode 0: NOOP

Any modecfg is legal, and the channel is always silent.

### Channel mode 1: DRUM

Modecfg is zero or more notes:
```
  1 Noteid 0..255, but only 0..127 are addressable.
  1 Trim lo 0..255.
  1 Trim hi 0..255.
  1 Pan 0..128..255 = left..center..right.
  2 Length.
  ... EAU file.
```

### Channel mode 2: FM

Modes (2,3,4) are the same thing except their oscillator.
If we were implementing native only, they'd be one mode. But I'm trying to keep it simple to implement in WebAudio too.
(eg you can't expand harmonics or perform FM on an arbitrary wave).
These should share most of their internal plumbing, and might use the same editor UI.

```
  u7.8 Rate, or u8.8|0x8000 Absolute rate qnotes = 0.
  u8.8 Range = 0.
  ... Level env. Complex default.
  ... Range env. Default constant 0xffff.
  ... Pitch env. Value is cents+0x8000. Default constant 0x8000 ie noop.
  u16 Wheel range, cents = 200.
  u8.8 LFO rate, qnotes = 0.
  u0.8 LFO depth = 1.
  u0.8 LFO phase = 0.
```
  
### Channel mode 3: HARSH

```
  u8 Shape (0,1,2,3) = sine,square,saw,triangle = 0.
  ... Level env. Complex default.
  ... Pitch env. Value is cents+0x8000. Default constant 0x8000 ie noop.
  u16 Wheel range, cents = 200.
```

### Channel mode 4: HARM

```
  u8 Harmonics count. Zero is equivalent to one harmonic at full amplitude. No DC.
  ... Harmonics, u0.16 each.
  ... Level env. Complex default.
  ... Pitch env. Value is cents+0x8000. Default constant 0x8000 ie noop.
  u16 Wheel range, cents = 200.
```

### Channel mode 5: SUB

```
  ... Level env. Complex default.
  u16 Width low, Hz = 200.
  u16 Width high, Hz = 200. NB Default is 200 regardless of low.
  u8 Stage count = 1. Zero is legal, in which case Center and Width are ignored and we produce white noise.
  u8.8 Gain = 1.
```

### Envelope

```
  1 Flags:
      01 Initials
      02 Velocity
      04 Sustain
      f8 Reserved, illegal.
  (2) Initlo, if Initials.
  (2) Inithi, if Initials and Velocity.
  1 (susp<<4)|pointc
  ... Points:
       2 tlo
       2 vlo
       (2) thi if Velocity.
       (2) vhi if Velocity.
```

The default envelope `[0,0]` must be understood as "unset, use default".
To explicitly encode a constant-zero envelope, give it an oob susp, eg `[0,16]`.
The initial point can't be sustained.
We're intrinsically limited to 15 points, by design. At runtime, after maybe inserting a sustain point, it's 16.

### Post

Zero or more stages:
```
  1 Stageid.
  1 Length.
  ... Params.
```

- `0x00 NOOP`: All params legal, does nothing.
- `0x01 DELAY`: u8.8 period qnotes=1.0, u0.8 dry=0.5, u0.8 wet=0.5, u0.8 store=0.5, u0.8 feedback=0.5, u8 sparkle(0..128..255)=0x80.
- - Sparkle adjusts the rate slightly for the two stereo channels, noop for mono.
- `0x02 WAVESHAPER`: u0.16... levels. Positive half only and no zero. ie a single 0xffff is noop. Edge case exception: Empty is noop.
- `0x03 TREMOLO`: u8.8 period qnotes=1, u0.8 depth=1, u0.8 phase=0.

## EAU Text

Text format that roughly mirrors EAU, but allows extra commentary.
We'll use this for the standard instrument set.
Token-oriented text: `KEYWORD [PARAMS...] ;` or `KEYWORD { [STATEMENTS...] }`.
`#` begins a line comment. Please do not put line comments inside a semicolon-delimited statement.
Quoted strings are JSON but must not contain escaped quotes (use "\u0022" if you really need one).
Fixed-point fields should be written as their integer equivalent, eg "0x0100" for 1.0 as u8.8.

Global statements (order matters):
```
tempo MS_PER_QNOTE ; # Must appear before any "chdr" or "events". Absent is ok too.
chdr { ... }
events { ... } # 0, 1, or 2. If 2, they must be adjacent.
```

If there are two `events` blocks, the loop position falls between them. More than two is an error.

Within `events {...}` (order matters):
```
delay MS ;
note CHID NOTEID VELOCITY DURMS ;
wheel CHID 0..512..1023 ;
```

Within `chdr {...}` (any order):
```
name STRING ;
chid 0..255 ;
trim 0..255 ;
pan 0..128..255 ;
mode ( 0..255 | noop | drum | fm | harsh | harm | sub ) ;
modecfg { ... }   # For modes (drum,fm,harsh,harm). See below.
modecfg HEXDUMP ; # For all other modes, of which none exist yet.
post { ... }
```

Within `post {...}` (order matters):
```
STAGEID HEXDUMP ;
delay PERIOD_U88 DRY WET STO FBK SPARKLE ; # 0..255
waveshaper HEXDUMP ; # two bytes per unit
tremolo PERIOD_U88 DEPTH PHASE ; # 0..255
```

Within `modecfg {...}` for `mode drum` (any order):
```
drum {
  name STRING ;
  noteid 0..255 ;
  trim 0..255 0..255 ;
  pan 0..128..255 ;
  serial { ...EAU_TEXT_FILE... }
}
```

Within `modecfg {...}` for `mode fm` (any order):
```
rate REL_U88 ;
absrate QNOTES_U88 ;
range U88 ;
levelenv ENV ;
rangeenv ENV ;
pitchenv ENV ;
wheelrange CENTS_U16 ;
lforate QNOTES_U88 ;
lfodepth 0..255 ;
lfophase 0..255 ;
```

Within `modecfg {...}` for `mode harsh` (any order):
```
shape ( 0..255 | sine | square | saw | triangle ) ;
levelenv ENV ;
pitchenv ENV ;
wheelrange CENTS_U16 ;
```

Within `modecfg {...}` for `mode harm` (any order):
```
harmonics HEXDUMP ; # 2 bytes per
levelenv ENV ;
pitchenv ENV ;
wheelrange CENTS_U16 ;
```

Within `modecfg {...}` for `mode sub` (any order):
```
levelenv ENV ;
widthlo HZ ;
widthhi HZ ; # NB Default is 200 regardless of 'widthlo'.
stagec U8 ;
gain U88 ;
```

ENV:
```
[=INITIAL[..HI]] [+MS[..HI] =VALUE[..HI][*] ...]
```
Times and values are 16-bit integers.
Values must be prefixed with '=', and times with '+' (for the whole construction, not for the high part).
One value may be suffixed `*` to mark the sustain point.
Empty is legal but also pointless; you can skip the field instead.
