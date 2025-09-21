# egg2

Engine for web and native games, retro style.
Most documentation is under [./etc/doc](./etc/doc/).

Differences from [Egg v1](https://github.com/aksommerville/egg):
- Custom build tool, similar to berry. Arbitrary targets selected at eggdev's build time, and web is not special.
- Direct access to data conversion a la carte via eggdev.
- Synth: Post pipes, LFOs, and stereo output.
- ~More input options: Keyboard, mouse, touch. Maybe accelerometer?~ Decided against this, after getting partway in.
- Single render call taking a struct of uniforms and a GPU-ready vertex buffer.
- No image decoder at runtime (egg 1 used to do this; it's actually been removed already).
- Rich built-in menu. Quit, input config, toggle music, toggle sound, language.
- Web apps pack to a Zip file with boilerplate HTML and the binary ROM. Same as berry. Also continue to allow standalone HTML as an option.
- eggdev --help reads from etc/doc/eggdev-cli.md directly.
- Default instruments live somewhere in the SDK.

## Should I use Egg?

Target use case is low-resolution 2d sprite graphics with beepy sound and local multiplayer.
Think SNES.

Some common game features that Egg *does not* support:
 - Analogue joysticks.
 - Text from the keyboard. (keyboard masquerades as a gamepad).
 - Mouse, touch, accelerometer.
 - Networking.
 - Recorded sound.
 - Arbitrary FS access.
 - 3d graphics.
 - Script languages. Our API is geared for C. C++, Rust, and the like should be possible. Lua, JS, Python, and the like will never work.
 
Features we *do* support:
 - 2d sprite graphics.
 - Beepy music and sound.
 - Access to music's playhead (eg for rhythm games).
 - Local multiplayer. Most hosts can go up to at least 8.
 - Multiple languages. Easy for strings, but you're on your own for text written in images.
 - Universal input config. Individual games never need to worry about it.
 - Highly portable.
 - Security guarantees. If paranoid, a user can extract the ROM from a web build and run that safely in their own runtime.
 - No unnecessary branding. For the most part, users don't know that you're using Egg, why should they care.

## Prereqs

- All use cases:
- - gcc, make, etc. Can use a different C compiler; set it up in `local/config.mk`. But its semantics must be close to `gcc`.
- Linux native (can take these a la carte):
- - xegl
- - libdrm, libgbm
- - EGL, GLES2
- - libasound
- - libpulse-simple
- Web:
- - wasm32-capable LLVM
- Native Wasm runtime:
- - WebAssembly Micro Runtime
- Editor: (anything?)
- MacOS: (anything?)
- Windows: (not supported)

## TODO

- [ ] Flesh out and validate Prereqs, above.
- [ ] Programmatic access to a song eg for ocarinas or sustained notes. API change.
- - The synthesizers currently aren't built for infinitely-sustaining notes.
- - Need to think this thru carefully, and ensure we have some safeguards against stuck notes.
- [ ] Editor.
- - [ ] Standard actions... what's needed?
- - - Don't bother with map stuff; those tend to implement a little different per game, let the overrides worry about it.
- - - [ ] Detect missing strings across languages.
- - - [ ] Detect empty sound effects.
- - - [ ] Is it feasible to scan for missing resources? eg map names an image that doesn't exist.
- - [ ] SongService: Should we auto-re-play on dirty?
- - [ ] SongService+SongChannelsUi: Mute and Solo buttons per channel.
- - [ ] SongChannelsUi: Copy levelenv when changing mode, and maybe do a per-mode default.
- - [ ] Generic command list support. Can we read a command schema off a comment in shared_symbols.h?
- - [ ] PostModal: Mysterious "invalid input" error on a newish channel. Can't repro.
- - [ ] Sounds require an explicit terminal delay. Have editor create this automagically from the events.
- - [ ] PostModal fields per stage type.
- - [ ] Song actions (SongChannelsUi)
- - [ ] ModecfgModal for drum: Spawn SongEditor in a modal per drum.
- [ ] eggdev
- - [ ] `eau-format.md`: "Events for a channel with no header will get a non-silent default instrument.". Confirm we're doing this in both implementations.
- - - We're not. And if it seems burdensome, we can change the spec to require CHDR.
- - - If we change the spec, ensure that MIDI=>EAU generates all CHDR. Not sure whether it does.
- - [ ] `eau-format.md`: "Duration of a sound is strictly limited to 5 seconds.". I didn't implement this yet.
- [ ] Native runtime.
- - [ ] Input.
- - - [ ] Interactive reconfig.
- - - - [ ] Let the client declare which buttons it uses, so when configuring we don't ask for all 15 buttons.
- - - - - Maybe a metadata field "incfgMask" containing characters "dswne123lrLR". "d" being the dpad, all others correspond to one button.
- - [ ] Record and playback session.
- - [ ] System language, for MacOS and Windows.
- - [ ] Global config file. Command-line options, and also persist `egg_prefs_set()` here.
- [ ] Web runtime.
- - [ ] Input.
- - - [ ] Mapping.
- - - [ ] Live config.
- - [ ] prefs
- - [ ] web: Detect loss of focus. At a minimum, pause audio. Maybe pause everything? Probly needs new soft-pause support in synth.
- - [ ] web Video: Determine whether border is necessary. Apply to main fb as needed too; right now it's only situated for id>1 textures.
- - [ ] web: Quantize final scale-up, don't use `object-fit:contain`.
- - [ ] web: Player count
- - [ ] synth: Test perceptually.
- - - Pay close attention to FM. I hacked it fast and loose for web, probably got it all wrong.
- - - Add some editor tooling to print a song both native and web, and play them back with an easy toggle.
- - [ ] synth: Confirm we can get decent whoosh, click, and snap sounds without subtractive voices. I'm not sure we can.
- - [ ] Web synth: tuned voices use the oscillator's `detune` for both wheel and pitchenv. I expect they will conflict.
- - - Probably related to that, FM voices really can't bend at all, the modulator goes out of sync.
- - [ ] Web playhead incorrect for songs shorter than the forward period. That's a tricky one, and not likely to matter. Apparent in drumtest.
- [ ] Generic menu: Input config, quit, audio, language.
- - [ ] Native.
- - [ ] Web.
- [ ] Client utilities.
- - [x] stdlib
- - [x] graf
- - [x] font
- - [ ] Standard instruments and sound effects.
- - [ ] Can we make native builds use egg-stdlib's rand()? There might be some value in having PRNG behave exactly the same across targets.
- [x] Robust demo ROM.
- - [x] Generic menu widget.
- - [x] video
- - [x] audio: Play song, adjust playhead, play effects.
- - [x] audio: Track playhead and show a warning toast when the incoming value <= previous. That must only happen when it repeats or song changes.
- - - Ho ho! Looks like we're doing this wrong in native, good thing I added this.
- - [x] input: Show all states plus an event log.
- - [x] misc: Local time, real time, log.
- - [x] storage: Prefs, store, rom.
- - [x] regression framework -- punt this until i have something to test.
- [ ] Should we allow strings to use symbolic names in place of index? I'm leaning No but give it some thought. We do something like that for decalsheet.
- [ ] EGG_GLSL_VERSION. Currently pretty hacky.
- [ ] macos: eggrun
- [ ] Line strip cuts corners. See NW corner of line strip in demo/Video/Primitives, both xegl and web. I want the corner filled. Is that possible?
- - Also the SE corners get doubled; apparent at low alpha.
- [ ] pulse: Fudged the estimated buffer length up 4x to avoid negative time-remaining. Can we fix it for real?
- [ ] stdlib: Leading zeroes in snprintf.
- [ ] web: prefs. Not implemented or what? See demo.
- [ ] web: Input is not setting CD.
- [ ] web: El Cheapo triggers have 1 and 2 swapped. Is that a real bug, or just bad luck with Standard Mapping?
