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
- [Queen of Clocks](https://github.com/aksommerville/queenofclocks)
- [Vexularg](https://github.com/aksommerville/vexularg)

## TODO

- [ ] Major changes.
- - [ ] Web: Touch input, on-screen gamepad.
- - - Where do we stand so far? `make demo-serve-public`, `GET http://192.168.1.82:8080` from a phone, it loads and appears to be working, but of course no input. No rotation either.
- - - The touch device shouldn't participate in incfg or mapping; those only take source devices to Egg's Generic Gamepad, and the Touch Device will expose like that in any case.
- - - We can test from the PC, we shouldn't actually need `make demo-serve-public` or the phone until it's close to ready.
- - - [ ] How to detect whether touch input is warranted?
- [ ] Audio
- - [ ] Synth: We need better insight into memory usage, and maybe some mitigations at runtime, like evicting sounds not currently in use, or forcing a terminating song off.
- - [ ] Redefine `EGG_PREF_MUSIC` and `EGG_PREF_SOUND` as trims in 0..99.
- - [ ] SongEditor: Ensure we can receive natural EAU files, save them as EAU, and also use EAU rather than MIDI for ones that started blank.
- - [ ] Demo sounds are still in the old format.
- - [ ] Demo: Update re new synth. Remove "force", allow multiple songs, do a Yoshi track and danger track, ocarina, test all the things...
- [ ] Audio review, when close to ready.
- [ ] Minor bugs and tweaks outstanding.
- - [ ] eggrun: Saving to "EGG_SDK/out/linux/eggrun.save" for every game. Should be "{{ROM}}.save".
- - [ ] `eggdev convert`: Saved games to/from JSON, for migrating your saves between native and web.
- - - [ ] That's such a simple conversion, and useful to players, maybe we should put it in `eggrun` too?
- - [ ] SongEditor: Something akin to MIDI-In when modecfg modals are open, for when there's no MIDI device.
- - [ ] Revise SDK's program zero to be more neutral config-wise, since it is what every tuned channel will default to. Doesn't matter whether it sounds nice.
- - [ ] SpriteEditor: Setting image or tile from the conveniences should fill a blank row if there is one, rather than adding.
- - [ ] Native build didn't detect a change to libfont.
- - [ ] native incfg: Timeout. See incfg_update().
- - [ ] Web incfg could bear some prettying-up.
- - [ ] MapEditor: Creating new map with position regime, I occasionally incorrectly get "position in use".
- - [ ] SongEditor EventModal: Show noteid in hex and musical name too.
- - [ ] SongEditor: Channel trim and pan can adjust in real time, in the synth. We should do that eagerly when it changes in the UI.
- - [ ] Editor: Sometimes deleting a post step doesn't work.
- - [ ] Editor: Pitch wheel disabled at MidiService.readEvent() because my device is noisy. Find a long-term solution.
- - [ ] Change malloc in egg-stdlib to use Wasm intrinsics, like synth. Then it won't produce a 16 MB ofile.
- - [ ] Launch from within map editor didn't rebuild.
- - [x] Song event modal: Default to note, not marker.
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
- - [ ] Would be nice if `eggdev convert` could change the pixel format of a PNG file, eg `eggdev convert -oout.png in.png --depth=1 --colortype=0`
- - [ ] `eggdev build` generates an Egg file qualified with "-web". That's correct, since there might be multiple web targets. But I dunno, should we drop the "-web" if there's just one target?
- - - (if that sounds super pointless, consider: Egg ROMs are universal. It was built as part of the Web process, but it can run anywhere, it's not really a "web" artifact)
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
