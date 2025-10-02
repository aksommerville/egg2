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
- [Humm Fu](https://github.com/aksommerville/hummfu)

## TODO

- [ ] Audio
- - Editor...
- - [x] SongService: Should we auto-re-play on dirty? ...NO
- - [x] SongService+SongChannelsUi: Mute and Solo buttons per channel.
- - [x] SongChannelsUi: Copy levelenv when changing mode, and maybe do a per-mode default.
- - [x] PostModal: Mysterious "invalid input" error on a newish channel. ...due to initializing with empty arrays instead of empty Uint8Arrays.
- - [ ] PostModal fields per stage type.
- - [ ] Song actions (SongChannelsUi). Four remain unimplemented; all four require parameters from the user.
- - [ ] IMPORTANT! ModecfgModal for drum: Spawn SongEditor in a modal per drum.
- - [ ] Special tooling to compare native vs web synth, in editor.
- - [ ] Can we get the EAU unit logging out for delivery via eggdev http when appropriate? (eg tweaking standard instruments, have to jump back and forth to console to see errors).
- - [x] Show events in SongToolbar's playhead minimap.
- - Synth (both)...
- - [ ] Confirm we can get decent whoosh, click, and snap sounds without subtractive voices. I'm not sure we can.
- - - Humm Fu had pretty decent hihats, but I had trouble with snare. I think we do need subtractive noise.
- - - [ ] Use a precalculated noise buffer, say one second long, generated with a private PRNG. That way, native and web can produce exactly the same noise.
- - - - They'll still sound different at different sample rates but I don't think we can avoid that.
- - [ ] Also add IIR post stages. They will make the HARSH voice mode much more useful.
- - [ ] Native and web do not sound the same. Once better tooling is in place, figure out why and tweak them closer.
- - [ ] Music level is too high relative to sound effects. Cheat it down globally. Fudged a correction in both Zen Garden and Humm Fu.
- - Synth (web)...
- - [ ] Web synth: tuned voices use the oscillator's `detune` for both wheel and pitchenv. I expect they will conflict.
- - - Probably related to that, FM voices really can't bend at all, the modulator goes out of sync.
- - [ ] Check default envelopes. I think I'm hearing some clicking when unconfigured.
- - Synth (native)...
- - [ ] In Humm Fu, there's a pop as you enter gameover. Due to terminating song with Delay posts?
- - And finally...
- - [ ] Standard instruments.
- [ ] In-game menu. Quit, Audio prefs, Language, Input config.
- - [ ] Native menu.
- - [ ] Native input config.
- - [ ] Web menu.
- - [ ] Web input config.
- [ ] Web.
- - [ ] Video: Determine whether border is necessary. For now we are applying always. That's wasteful, but should be safe at least.
- [ ] native: Can we use egg-stdlib's rand()? There might be some value in having PRNG behave exactly the same across targets.
- [ ] native: Record and playback session.
- [ ] native: Add an initial audio delay like we did in v1. I've noticed missed notes in Humm Fu.
- [x] CommandListEditor: New blank fields are getting created as I set up a new sprite.
- [ ] macos: System language
- [ ] windows: System language
- [ ] native: Global config file. Command-line options, and also persist `egg_prefs_set()` here.
- [ ] EGG_GLSL_VERSION. Currently pretty hacky.
- [ ] macos: eggrun
- [ ] pulse: Fudged the estimated buffer length up 4x to avoid negative time-remaining. Can we fix it for real?
- [ ] Review all "TODO" in source, there's a ton of them.
- With the above complete, we can start migrating games:
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
- - [ ] Upsy-Downsy -- least difficult and most beneficial of these. We could then retire `pebble`
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
