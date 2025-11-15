# Egg Audio Format "EAU"

`song` and `sound` resources are both stored in the ROM as our private format EAU.

EAU is similar to MIDI, with these notable exceptions:
- Single track.
- Channel configuration and events are stored separately.
- Channels can't reconfigure during playback.
- Notes may contain their duration. (separate Note On and Note Off also exist).
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

Meta 0x51 Set Tempo should appear once at time zero.
Tempo changes are permitted, but we will declare only the initial tempo.

Meta 0x07 Cue Point with the payload "LOOP" to mark the loop point. This is permitted only zero or one times.
Legal but redundant at time zero.

The Channel Header and Text chunks may be encoded as custom Meta events at time zero, with exactly the payload to emit:
 - Meta 0x77 Channel Headers.
 - Meta 0x78 Text.
If one of those is present, compiler should not attempt to synthesize any content from other MIDI events.

In the absence of Meta 0x77 Channel Headers, we generate headers based on:
 - Bank Select.
 - Program Change.
 - Unconfigured channel 9 is presumed to be drums.
 - Control 0x07 Volume MSB.
 - Control 0x0a Pan MSB.

In the absence of Meta 0x78 Text, we generate text based on:
 - Meta 0x03 Track Name.
 - Meta 0x04 Instrument Name.

Opening a MIDI file in our editor and resaving it may destroy information.

## EAU Binary

```
4 Signature: "\0EAU"
2 Tempo, ms/qnote.
4 Channel Headers length.
... Channel Headers, see below.
4 Events length.
... Events, see below.
4 Text length.
... Text, see below.
```

### Channel Headers

Zero or more of:
```
u8 chid
u8 trim
u8 pan 0..128..255 = left..center..right
u8 mode
u16 modecfg length
... modecfg
u16 post length
... post
```

You must configure every channel that the events address.

Channels are not required to be in order.
Configuring the same channel twice is an error; decoder's behavior is undefined.
chid >=16 are perfectly legal but not addressable by events. We may use EAU as an instrument repository, where chid is really pid.

In general, (modecfg) is allowed to be short, even empty.
It's an error to cut it off mid-field, and the various modes may have additional constraints.

### Mode 0x00 NOOP

Channel is silent.
Same behavior as unconfigured channels, they will be silent and all events ignored.

### Mode 0x01 TRIVIAL

Square wave with no envelope.
The hold level is velocity-sensitive, but timings are not.

```
u16 wheel range, cents. =200
u16 minimum level. =0x2000
u16 maximum level. =0xffff
u16 minimum hold time, ms. =100
u16 release time, ms. =100
```

### Mode 0x02 FM

FM and wavetable voices.
Pretty much all tuned instruments should use this.
Simple wave-based voices are an edge case here, actual modulation is optional.

```
ENV levelenv. Complex default.
u16 wheelrange, cents. =200
WAVE wavea. =sine
WAVE waveb. =wavea
ENV mixenv. 0..1=a..b. =A only
u8.8 modrate. If 0x8000 set, it's absolute in qnotes. Otherwise relative to note rate. =0
u8.8 modrange. =1
ENV rangeenv. =constant 1.
ENV pitchenv (cents, bias to 0x8000). =constant 0.5
WAVE modulator. =sine
u8.8 rangelforate, qnotes. =0
u0.8 rangelfodepth. =1
WAVE rangelfowave. =sine
u8.8 mixlforate, qnotes. =0
u0.8 mixlfodepth. =1
WAVE mixlfowave. =sine
```

### Mode 0x03 SUB

White noise thru a bandpass.

```
ENV levelenv. Complex default.
u16 widthlo, hz. =200
u16 widthhi, hz. =widthlo
u8 stagec. Zero is legal, to disable the bandpass. =1
u8.8 gain. =1
```

### Mode 0x04 DRUM

Each note is its own EAU file for a single sound.
Note that modecfg is limited to 64 kB. It's possible to reach that limit in a drum channel.

Modecfg is zero of more of:
```
u8 noteid. >=0x80 are legal but not addressable. Might be used as a sound effects repository or something.
u8 trimlo
u8 trimhi
u8 pan
u16 len
... eau
```

Not required to sort by (noteid).
Duplicate (noteid) are forbidden; decoders' behavior is undefined in that case.

### ENV

A single zero byte is a legal encoded envelope, meaning to treat as if unset, use the default.
Do not include the (susp,ptc) byte in that case.

```
u8 flags:
     0x01 Initials
     0x02 Velocity
     0x04 Sustain
     0x08 Present: Should always be set, otherwise a leading zero means default.
     0xf0 Reserved, zero.
(u16) initlo, if Initials.
(u16) inithi, if Initials and Velocity.
u8 (susp<<4)|ptc
... Points, up to 15:
      u16 tlo, ms
      u16 vlo
      (u16) thi, ms, if Velocity.
      (u16) vhi, if Velocity.
```

### WAVE

Sequence of commands that build up a single-period wave incrementally.
All commands are identified by their leading byte, and unknown commands are an error.
The wave's encoded length is self-describing, if you comprehend every command.
A wave consisting only of EOF, ie a single zero, means default (typically sine).

```
0x00 EOF. Required.

-- Commands that replace the wave. These only make sense in the first position. --
0x01 SINE []
0x02 SQUARE [u8 smooth]. Trivial square at 0, sigmoiding to a sine at 0xff.
0x03 SAW [u8 smooth]. Trivial downward saw at 0, to triangle at 0xff.
0x04 TRIANGLE [u8 smooth]. Trivial triangle at 0, trapezoiding to square at 0xff. NB: More "smooth" makes a rougher sound.
0x05 NOISE []. White noise with forced signs. Like a dirty square wave. Random, but you'll get the same thing every time, even across hosts.

-- Commands that expect a non-silent wave as input. These do not make sense in the first position. --
0x06 ROTATE [u8 phase]. Shuffle samples such that (phase) is the new beginning.
0x07 GAIN [u8.8 mlt]. Multiply, allowed to go out of range.
0x08 CLIP [u8 limit]. Clamp to limit and -limit.
0x09 NORM [u8 limit]. Multiply such that the peak becomes (limit).
0x0a HARMONICS [u8 c, u16 ...coefv]. Mix the current wave against itself at various harmonics. First coefficient is for the natural rate.
0x0b HARMFM [u8 (rate<<4)|range]. Perform FM with the modulator at some positive harmonic rate.
0x0c MAVG [u8 windowlen]. Moving average, up to 255 samples.
```

With the (smooth) parameter, our editor can present initial shape as a continuum: SINE => SQUARE => TRIANGLE => SAW

### Post

Zero or more stages:
```
u8 type
u8 len
... payload
```

```
0x00 NOOP []
0x01 GAIN [u8.8 mlt, u0.8 clip=1, u0.8 gate=0]. Trivial gain and clip.
0x02 DELAY [u8.8 qnotes, u0.8 dry=0.5, u0.8 wet=0.5, u0.8 store=0.5, u0.8 feedback=0.5, u0.8 sparkle=0.5]. Sparkle is a stereo effect, noop at 0.5.
0x03 TREMOLO [u8.8 qnotes, u0.8 depth=1, u0.8 phase=0, u0.8 sparkle=0.5]. Sparkle puts L and R out of phase, so the sound moves dizzyingly.
0x04 DETUNE [u8.8 qnotes, u0.8 mix=0.5, u0.8 depth=0.5, u0.8 phase=0, u0.8 rightphase=0]. Detune by pingponging back and forth in time.
0x05 WAVESHAPER [u0.16 ...coefv]. Positive coefficients only. A single 0xffff is noop.

These IIR filter stages are defined but not yet implemented. I might remove them:
0x06 LOPASS [u16 hz]
0x07 HIPASS [u16 hz]
0x08 BPASS [u16 mid, u16 width]
0x09 NOTCH [u16 mid, u16 width]
```

TODO Reverb? Compression?

### Events

Our event stream looks a lot like a MIDI file's MTrk chunk.
Key differences:
 - Delay is its own event, not necessarily interleaved with other events.
 - Delay are in milliseconds, no such thing as a tick.
 - No Aftertouch, Control Change, Program Change, Meta, Sysex, or Realtime events. Just Note and Wheel.
 - We add a "Note Once" event containing its hold length. Milliseconds from start to release, clamped to the attack and decay length.
 - No Running Status, and hence no need for the velocity-zero trick.
 - Note Off velocity is recorded but is always ignored. Tooling is free to turn an On/Off pair into Note Once (which drops the Off velocity).

```
00tttttt                            : Short Delay, (t) ms.
01tttttt                            : Long Delay, ((t+1)<<6) ms. Limit 4096.
1000cccc xnnnnnnn xvvvvvvv          : Note Off
1001cccc xnnnnnnn xvvvvvvv          : Note On. No velocity-zero trick like MIDI.
1010cccc nnnnnnnv vvvvvvtt tttttttt : Note Once. (t<<4) ms. Limit about 16 s.
1011xxxx                            : Reserved, illegal.
1100xxxx                            : Reserved, illegal.
1101xxxx                            : Reserved, illegal.
1110cccc xaaaaaaa xbbbbbbb          : Wheel. (a|(b<<7)) as in MIDI.
1111nnnn                            : Marker. (n==0) means Loop Point. Others reserved but legal.
```

### Text

Songs may contain names of channels or drum notes.
Tooling should drop this content when building for production, and the runtime should never use it.

Zero or more of:
```
u8 chid
u8 noteid. 0xff for whole channel.
u8 length
... text
```
