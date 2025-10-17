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
- [Mysteries of the Crypt](https://github.com/aksommerville/myscrypt)

## TODO

- [ ] Major changes.
- - [x] Confirm we can get decent whoosh, click, and snap sounds without subtractive voices. I'm not sure we can.
- - - Humm Fu had pretty decent hihats, but I had trouble with snare. I think we do need subtractive noise.
- - - [x] Use a precalculated noise buffer, say one second long, generated with a private PRNG. That way, native and web can produce exactly the same noise.
- - - - They'll still sound different at different sample rates but I don't think we can avoid that.
- - - [x] eggdev
- - - [x] native
- - - [x] editor
- - - [x] web
- - [ ] Also add IIR post stages. They will make the HARSH voice mode much more useful.
- - [ ] editor: MIDI-In for synth instrument testing. Maybe just while the modecfg modal is open?
- - [ ] Standard instruments.
- - [ ] In-game menu. Quit, Audio prefs, Language, Input config.
- - - [ ] Native menu.
- - - [ ] Native input config.
- - - [ ] Web menu.
- - - [ ] Web input config.
- - [ ] Web: Touch input, on-screen gamepad.
- [ ] Audio
- - [ ] Web synth: tuned voices use the oscillator's `detune` for both wheel and pitchenv. I expect they will conflict.
- - - Probably related to that, FM voices really can't bend at all, the modulator goes out of sync.
- - [ ] Check default envelopes. I think I'm hearing some clicking when unconfigured.
- - [ ] In Humm Fu, there's a pop as you enter gameover. Due to terminating song with Delay posts?
- [ ] Audio review, when close to ready.
- - [ ] Native and web do not sound the same. Once better tooling is in place, figure out why and tweak them closer.
- - [ ] Music level is too high relative to sound effects. Cheat it down globally. Fudged a correction in both Zen Garden and Humm Fu.
- [ ] Minor bugs and tweaks outstanding.
- - [ ] Permit command-line and query params to prepopulate the store, for keys specified via metadata.
- - - I'm picturing printing QR codes that embed a saved game from one machine, that the user can reopen in her browser.
- - - Could also be super helpful during development: `./mygame --startAtLevel=13`
- - - Do require games to opt in to this behavior per field by naming the keys in metadata.
- - [ ] editor: New map in "neighbors" regime created an incorrect (zeroes) command in the new one, and didn't create in the old one.
- - - ...might only happen to the first map in a layer, or maybe the first in a project. Wishbone got it on the first neighbor creation, and not after.
- - - ...no, it did happen later in wishbone too.
- - [ ] editor: Sprite in the edit-poi modal are sorted as strings; prefer to sort numerically by rid.
- - [ ] editor: "Copy from other resource" option in the channels' "Store..." modal.
- - [ ] editor: Creating new tilesheet or decalsheet, somehow prompt existing images.
- - [ ] graf: Can flush due to full buffer in the middle of raw geometry, breaking sets. eg try a bunch of `EGG_RENDER_LINES`.
- - - Confirm that all impacted cases are single calls into graf. If so, the fix will be easy, just start allocating multiple vertices at once internally.
- - [x] editor: World map. Ugh, I needed this badly during Mysteries of the Crypt. :(
- - [x] MapEditor: Producing 3-byte "position" commands, which can never be legal.
- - [x] eggdev: Builds still do not appear to pull in their deps correctly; I'm needing to `make clean` when I shouldn't need to.
- - - I think it's the files with relative paths that don't rebuild. eg `myscrypt/src/sprite/sprite.c` includes "../myscrypt.h" and doesn't rebuild as expected.
- - - ...indeed the makefile that gcc generates for that does have ".." entries preserved. We need to resolve those during `eggdev build`.
- - [ ] Web Video: Determine whether border is necessary. For now we are applying always. That's wasteful, but should be safe at least.
- - [x] editor: Rainbow pencil overwrites appointment-only neighbors, it shouldn't. ...actually you can't even place an appt-only with the rainbow, if neighbors exist. ouch
- - [x] MapEditor: We can do better with the edit-poi modal...
- - - [x] Drop-down for sprites.
- - - [x] If `NS_sprtype_` exists and the selected sprite links to it, use its comment as the remainder of the command.
- - - - This bit me in Mysteries of the Crypt. Would have been great to get a little hint about the arg format when placing a sprite.
- - [x] MapEditor: Can we handle transparent tiles better? Maybe `NS_sys_bgcolor` or something?
- - [x] TilesheetEditor: Weird when the image is mostly transparent. Use a different color for the margin.
- - [x] editor: New map modal doesn't dismiss after creating map, in "position" regime. Also, got a wildly incorrect position.
- - [x] editor: Maps get created with 3-param position. There are no 3-param command sizes. Make it 2 or 4.
- - [x] editor: Delete resource, if selected we should reload with no selected resource.
- - [x] editor: MissingResourcesService: For song, Report missing drums and unconfigured channels. ...DECLINE. They're MIDI at that point. Can validate in SongEditor instead.
- - [ ] editor: SidebarUi scroll bar broken, doesn't appear
- - [ ] native: Can we use egg-stdlib's rand()? There might be some value in having PRNG behave exactly the same across targets.
- - [ ] native: Record and playback session.
- - [ ] native: Add an initial audio delay like we did in v1. I've noticed missed notes in Humm Fu.
- - [ ] eggdev client: Detect changes to shared_symbols.h and rebuild symbols when changed. Currently you have to restart the server if you change symbols.
- - [ ] macos: System language
- - [ ] windows: System language
- - [ ] native: Global config file. Command-line options, and also persist `egg_prefs_set()` here.
- - [ ] EGG_GLSL_VERSION. Currently pretty hacky.
- - [ ] macos: eggrun
- - [ ] pulse: Fudged the estimated buffer length up 4x to avoid negative time-remaining. Can we fix it for real?
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
- - [ ] Upsy-Downsy -- least difficult and most beneficial of these. We could then retire `pebble`
- - [ ] Sitter 2009 (a full rewrite is warranted)
- - [ ] Tag Team Adventure Quest
- - [ ] Campaign Trail of the Mummy
- - [ ] Plunder Squad
- - [ ] Full Moon. Huge and complex.
- - [ ] Too Heavy (JS; would be a full rewrite)
- - [ ] Economny of Motion (JS; full rewrite)
- - Definitely not in scope: Chetyorska (MIDI-In)
- [ ] "eggzotics": Sample games that build for something weird, and also Egg.
- - Anything with a virtual runtime is definitely out. So no Pico-8, and nothing using Java, JS, Lua, etc.
- - [ ] Shovel. That's my other games framework, specifically to build web apps under 13 kB. I bet we can arrange a way to build for both Egg and Shovel, with Shovel's constraints.
- - - [ ] Opener of Cages
- - - [ ] Nine Lives
- - [ ] Tiny Arcade
- - - If this works, migrate all my Tiny games.
- - [ ] Thumby
- - [ ] Thumby Color
- - [ ] PicoSystem
- - [ ] Playdate
- - [ ] Pre-OSX Mac?
- - [ ] NES? SNES? Gameboy? That's probably insane, right?
