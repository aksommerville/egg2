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
- - [WebAssembly Micro Runtime](https://github.com/bytecodealliance/wasm-micro-runtime)
- Editor: (anything?)
- MacOS: (anything?)
- Windows: (not supported)

## TODO

- [ ] Audio
- - Editor...
- - [ ] SongService: Should we auto-re-play on dirty?
- - [ ] SongService+SongChannelsUi: Mute and Solo buttons per channel.
- - [ ] SongChannelsUi: Copy levelenv when changing mode, and maybe do a per-mode default.
- - [ ] PostModal: Mysterious "invalid input" error on a newish channel. Can't repro.
- - [ ] Sounds require an explicit terminal delay. Have editor create this automagically from the events.
- - [ ] PostModal fields per stage type.
- - [ ] Song actions (SongChannelsUi)
- - [ ] ModecfgModal for drum: Spawn SongEditor in a modal per drum.
- - [ ] Special tooling to compare native vs web synth, in editor.
- - Synth (both)...
- - [ ] `eau-format.md`: "Events for a channel with no header will get a non-silent default instrument.". Confirm we're doing this in both implementations.
- - - We're not. And if it seems burdensome, we can change the spec to require CHDR.
- - - If we change the spec, ensure that MIDI=>EAU generates all CHDR. Not sure whether it does.
- - [ ] `eau-format.md`: "Duration of a sound is strictly limited to 5 seconds.". I didn't implement this yet.
- - [ ] Confirm we can get decent whoosh, click, and snap sounds without subtractive voices. I'm not sure we can.
- - Synth (web)...
- - [ ] Web synth: tuned voices use the oscillator's `detune` for both wheel and pitchenv. I expect they will conflict.
- - - Probably related to that, FM voices really can't bend at all, the modulator goes out of sync.
- - [ ] Web playhead incorrect for songs shorter than the forward period. That's a tricky one, and not likely to matter. Apparent in drumtest.
- - [ ] Print PCM for sound effects.
- - And finally...
- - [ ] Standard instruments.
- [ ] In-game menu. Quit, Audio prefs, Language, Input config.
- - [ ] Native menu.
- - [ ] Native input config.
- - [ ] Web menu.
- - [ ] Web input config.
- [ ] Web. I skipped a lot of details first time thru.
- - [x] prefs
- - [x] Detect loss of focus. At a minimum, pause audio. Maybe pause everything? Probly needs new soft-pause support in synth.
- - [ ] Video: Determine whether border is necessary. Apply to main fb as needed too; right now it's only situated for id>1 textures.
- - [x] Quantize final scale-up, don't use `object-fit:contain`.
- - [ ] Player count
- - [ ] prefs. Not implemented or what? See demo.
- - [ ] Input is not setting CD.
- - [ ] El Cheapo triggers have 1 and 2 swapped. Is that a real bug, or just bad luck with Standard Mapping?
- - [ ] Releasing manual synth note cuts off cold. It ought to enter the envelope's release stage.
- [ ] native: Can we use egg-stdlib's rand()? There might be some value in having PRNG behave exactly the same across targets.
- [ ] native: Record and playback session.
- [ ] macos: System language
- [ ] windows: System language
- [ ] native: Global config file. Command-line options, and also persist `egg_prefs_set()` here.
- [ ] EGG_GLSL_VERSION. Currently pretty hacky.
- [ ] macos: eggrun
- [ ] pulse: Fudged the estimated buffer length up 4x to avoid negative time-remaining. Can we fix it for real?
