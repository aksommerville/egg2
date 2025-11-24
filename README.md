# egg2

Engine for web and native games, retro style.
Most documentation is under [./etc/doc](./etc/doc/).

Differences from [Egg v1](https://github.com/aksommerville/egg):
- Custom build tool, similar to berry. Arbitrary targets selected at eggdev's build time, and web is not special.
- Direct access to data conversion a la carte via eggdev.
- Synth: Post pipes, LFOs, and stereo output.
- Synth: Single implementation for both native and web. Web runs Wasm in an AudioWorkletNode.
- ~More input options: Keyboard, mouse, touch. Maybe accelerometer?~ Decided against this, after getting partway in.
- Single render call taking a struct of uniforms and a GPU-ready vertex buffer.
- No image decoder at runtime (egg 1 used to do this; it's actually been removed already).
- Rich built-in menu. Quit, input config, toggle music, toggle sound, language.
- Web apps pack to a Zip file with boilerplate HTML and the binary ROM. Same as berry.
- Standalone HTML is no longer an option, due to synth.
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
- [When You Wish Upon A Bone](https://github.com/aksommerville/wishbone)

## TODO

- [x] Branch 20251106-synth3: Replace synthesizer with the AudioWorkletNode strategy proven out in egg3. (egg3 is not real; 2 is the go-forward version).
- - [x] Update MIDI notes in etc/doc/eau-format.md
- - [x] Are we going to have an EAU-Text format? v3 doesn't have one. ...I think we don't need, at least it's not worth the enormous effort right now.
- - [x] API changes
- - - [x] `EGG_PREF_MUSIC` and `EGG_PREF_SOUND` should be continuous trims, say 0..99. ...punt
- - - [x] Permit multiple songs? I really think we should, along the lines of egg3.
- - - [x] Note On / Note Off / Note Once / Wheel, make Egg Platform API match synth's API. Also "songid".
- - - [x] Update eggrt.
- - - [x] Update eggrun.
- - - [x] Update web.
- - - [x] Update demo.
- - [x] Web. Orchestrate load in Audio.js.
- - [x] Web: Estimate playhead. Do in Audio.js, not editor, so we can expose it via Platform API.
- - [x] Editor: Song model
- - [x] Editor: UI
- - [x] Eliminate standalone builds.
- - [x] Build synth wasm.
- - [x] `GET /api/webpath` is changed to return the Zip instead. That probably breaks launching from editor. ...confirmed, broken
- - [x] `eau_cvt_eau_midi`: Look up in SDK instruments. ...punt
- - [x] Is web playing mono only? ...YES. 9-sand_farming has a post, makes it obvious.
- - [x] Eliminate the global modal for comparing synthesizers. They're now the same thing.
- - [x] Update one client project before finalizing. Zen Garden?
- - - Update `egg_play_song` from `(rid,force,repeat)` to `(1,rid,repeat,1.0f,0.0f)`
- - - Use midevil to remove Meta 0x77 from songs. Old regime used multiple, and that will choke the new regime.
- - - Zap all sound effects. `echo -n "" > 1-yadda_yadda`
- - - Play natively to validate.
- - - If songs are requested redundantly, add a global `playing_song_id` -- synth doesn't manage that anymore.
- - - Add to Makefile for edit: `--htdocs=/synth.wasm:EGG_SDK/out/web/synth.wasm`
- - - Also `--htdocs=/build:out/%.*s-web.zip` tho we don't necessarily need it right now.
- - - Revoice song and sound resources.
- - [x] Humm Fu
- - [x] Mysteries of the Crypt
- - [x] When You Wish Upon a Bone
- [ ] Major changes.
- - [ ] Build client libraries individually per target, do not roll into libeggrt. Clients should include a la carte by just adding to OPT_ENABLE in their Makefile.
- - [ ] editor: MIDI-In for synth instrument testing. Maybe just while the modecfg modal is open?
- - [ ] Edit SDK instruments. Maybe a global option in the Editor's actions menu?
- - [ ] Standard instruments.
- - [ ] In-game menu. Quit, Audio prefs, Language, Input config.
- - - [ ] Native menu.
- - - [ ] Native input config.
- - - [ ] Web menu.
- - - [ ] Web input config.
- - [ ] Web: Touch input, on-screen gamepad.
- [ ] Audio
- - [x] Web synth: Play thru song 8, then start song 9 in demo. 9 doesn't start. Will start on second try. Are we running out of memory? (both big songs)
- - - ...Humm Fu does it at the end. ...increasing memory fixed it.
- - [ ] ^ No but seriously. We need better insight into memory usage, and maybe some mitigations at runtime, like evicting sounds not currently in use, or forcing a terminating song off.
- - [ ] Redefine `EGG_PREF_MUSIC` and `EGG_PREF_SOUND` as trims in 0..99.
- - [ ] Web Audio.js: Slice out audio parts of ROM, don't send the whole thing.
- - [ ] Look up SDK instruments during song compile.
- - [ ] Demo: Update re new synth. Remove "force", allow multiple songs, do a Yoshi track and danger track, ocarina, test all the things...
- [ ] Audio review, when close to ready.
- [ ] Minor bugs and tweaks outstanding.
- - [ ] Launch from within map editor didn't rebuild.
- - [ ] Song event modal: Default to note, not marker.
- - [ ] FM modecfg modal: Rate and range should present as float, regardless of how they're encoded.
- - [ ] Wave modal: Per-stage UI. Esp for harmonics, I want a clickable bar chart.
- - [ ] EnvUi: Is it enforcing a minimum 1 s or something? These typically run around 300 ms. Aim for the existing chart to fill like 3/4 of the available width.
- - [ ] Song editor: Set tempo. An action I guess.
- - [ ] Song editor: Adding event goes before those at same time; must be after.
- - [ ] Song editor: Auto end time clearly wrong for SUB voices.
- - [ ] Remove `eggdev_convert_context`. Make a similar thing in "serial", so we can share it around. Then share it around.
- - [ ] DecalsheetEditor: After using one of the clicky macros, sidebar scrolls to the top again. Can we keep it where it was? So annoying.
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
- - [ ] Web Video: Determine whether border is necessary. For now we are applying always. That's wasteful, but should be safe at least.
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
- - [ ] eggstra play: Show CPU consumption.
- - [ ] eggstra play: Read from stdin.
- - [ ] eggstra play: Play WAV files.
- - [ ] Editor Launch: Is it possible to dismiss the iframe when game ends? Or allow Esc after it terminates? Having trouble capturing key events for it.
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
- - [ ] Tag Team Adventure Quest
- - [ ] Campaign Trail of the Mummy
- - [ ] Plunder Squad
- - [ ] Full Moon. Huge and complex.
- - [ ] Too Heavy (JS; would be a full rewrite)
- - [ ] Economy of Motion (JS; full rewrite)
- - [ ] Sitter 2009 (a full rewrite is warranted)
- - Definitely not in scope: Chetyorska (MIDI-In), Pico Sitter (who cares)
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
- - [ ] Pre-OSX Mac? +Pippin if so.
- - [ ] NES? SNES? Gameboy? That's probably insane, right?
- - [ ] Wii/GameCube (maybe a full target)
- - [ ] Xbox ('')
- - [ ] Can we find a toolchain for Playstation? That would be super cool. And might be a full target.
- - [ ] Dreamcast. I recall this is highly doable, security-wise.
- - [ ] Philips CDI. I'm told it's very easy to develop for. Can we find one?
