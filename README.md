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
 - Small builds. <1MB is typical.
 - Fast builds. Egg itself might take a minute once, and games usually <10s from scratch.
 - Built-in generic metadata for indexing a collection of games.

## Prereqs

- All use cases:
- - gcc, make, etc. Can use a different C compiler; set it up in `local/config.mk`. But its semantics must be close to `gcc`.
- - Graphics, music, and text, bring your own tools. Graphics must be PNG, and music MIDI.
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

## Games

- [Zen Garden](https://github.com/aksommerville/zennoniwa)

## TODO

- [ ] Audio
- - Editor...
- - [ ] SongService: Should we auto-re-play on dirty?
- - [ ] SongService+SongChannelsUi: Mute and Solo buttons per channel.
- - [ ] SongChannelsUi: Copy levelenv when changing mode, and maybe do a per-mode default.
- - [ ] PostModal: Mysterious "invalid input" error on a newish channel. Can't repro.
- - [x] Sounds require an explicit terminal delay. Have editor create this automagically from the events. ...manual action
- - [ ] PostModal fields per stage type.
- - [ ] Song actions (SongChannelsUi). Four remain unimplemented; all four require parameters from the user.
- - [ ] ModecfgModal for drum: Spawn SongEditor in a modal per drum.
- - [ ] Special tooling to compare native vs web synth, in editor.
- - Synth (both)...
- - [x] `eau-format.md`: "Events for a channel with no header will get a non-silent default instrument.". Confirm we're doing this in both implementations.
- - - We're not. And if it seems burdensome, we can change the spec to require CHDR.
- - - If we change the spec, ensure that MIDI=>EAU generates all CHDR. Not sure whether it does.
- - - ...changed spec, now missing CHDR means noop notes. Confirmed both synths ignore, and `eau_cvt_eau_midi` does generate a default.
- - [ ] `eau-format.md`: "Duration of a sound is strictly limited to 5 seconds.". I didn't implement this yet.
- - [ ] Confirm we can get decent whoosh, click, and snap sounds without subtractive voices. I'm not sure we can.
- - [ ] Native and web do not sound the same. Once better tooling is in place, figure out why and tweak them closer.
- - Synth (web)...
- - [ ] Web synth: tuned voices use the oscillator's `detune` for both wheel and pitchenv. I expect they will conflict.
- - - Probably related to that, FM voices really can't bend at all, the modulator goes out of sync.
- - And finally...
- - [ ] Standard instruments.
- [ ] In-game menu. Quit, Audio prefs, Language, Input config.
- - [ ] Native menu.
- - [ ] Native input config.
- - [ ] Web menu.
- - [ ] Web input config.
- [ ] Web.
- - [ ] Video: Determine whether border is necessary. Apply to main fb as needed too; right now it's only situated for id>1 textures.
- [ ] native: Can we use egg-stdlib's rand()? There might be some value in having PRNG behave exactly the same across targets.
- [ ] native: Record and playback session.
- [ ] macos: System language
- [ ] windows: System language
- [ ] native: Global config file. Command-line options, and also persist `egg_prefs_set()` here.
- [ ] EGG_GLSL_VERSION. Currently pretty hacky.
- [ ] macos: eggrun
- [ ] pulse: Fudged the estimated buffer length up 4x to avoid negative time-remaining. Can we fix it for real?
- [ ] Rewrite or migrate eggsamples for v2.
- [ ] Migrate existing v1 projects to v2.
- - [ ] Season of Penance
- - [ ] Spelling Bee
- - [ ] Thirty Seconds Apothecary
- - [ ] Presto Changeo
- - [ ] Reddin Iggle
- - [ ] Dot's Wicked Garden
- - [ ] Gobblin Kabobblin'
- - [ ] Dead Weight
- - [ ] Cherteau
- - [ ] Sam-Sam
- [ ] Enormous effort, but how do you feel about migrating or rewriting old non-Egg games? Could make provisioning new kiosks a lot smoother.
- - [ ] Sitter 2009 (a full rewrite is warranted)
- - [ ] Tag Team Adventure Quest
- - [ ] Campaign Trail of the Mummy
- - [ ] Plunder Squad
- - [ ] Full Moon
- - [ ] Too Heavy (JS; would be a full rewrite)
- - [ ] Upsy-Downsy
- - Definitely not in scope: Chetyorska, Economy of Motion, Opener of Cages, Nine Lives
- [ ] "eggzotics": Sample games that build for something weird, and also Egg.
- - [ ] Tiny Arcade
- - - If this works, migrate all my Tiny games.
- - [ ] Thumby
- - [ ] Thumby Color
- - [ ] PicoSystem
- - [ ] Playdate
- - [ ] Pre-OSX Mac?
- - [ ] NES? SNES? Gameboy? That's probably insane, right?
