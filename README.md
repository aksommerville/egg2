# egg2

Engine for web and native games, retro style.
Most documentation is under [./etc/doc](./etc/doc/).

## Should I use Egg?

Target use case is low-resolution 2d sprite graphics with beepy sound and local multiplayer.
Think SNES.

Some common game features that Egg *does not* support:
 - Analogue joysticks.
 - Text from the keyboard. (keyboard masquerades as a gamepad).
 - Touch, accelerometer. Mouse is supported in a limited fashion.
 - Networking.
 - Recorded sound.
 - Arbitrary FS access.
 - 3d graphics.
 - Script languages.
 
Features we *do* support:
 - 2d sprite graphics.
 - Beepy music and sound.
 - Local multiplayer, if the user has multiple gamepads.
 - Multiple languages. Easy for strings, but you're on your own for text written in images.
 - Universal input config. Individual games never need to worry about it.
 - Highly portable.
 - Security guarantees. If paranoid, a user can extract the ROM from a web build and run that safely in their own runtime.
 - No unnecessary branding. For the most part, users don't know that you're using Egg, why should they care.
 - Small builds. <1MB is typical.
 - Fast builds. Egg itself might take a minute once, and games usually <10s from scratch.
 - Fast startup. As in, instantaneous, usually.
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
- Windows: (not supported yet)

## Games

A few simple example projects can be found at [eggsamples](https://github.com/aksommerville/eggsamples).

- [Upsy-Downsy](https://github.com/aksommerville/upsy-downsy)
- [The Season of Penance](https://github.com/aksommerville/penance)
- [Spelling Bee](https://github.com/aksommerville/spellingbee)
- [Thirty Seconds Apothecary](https://github.com/aksommerville/apothecary)
- [Presto Changeo](https://github.com/aksommerville/presto)
- [Reddin Iggle](https://github.com/aksommerville/iggle)
- [Dot's Wicked Garden](https://github.com/aksommerville/zerosigma)
- [Goblin Kabobblin](https://github.com/aksommerville/kabobblin)
- [Dead Weight](https://github.com/aksommerville/deadweight)
- [Cherteau](https://github.com/aksommerville/cherteau)
- [Sam-Sam](https://github.com/aksommerville/samsam)
- [Zen Garden](https://github.com/aksommerville/zennoniwa)
- [Humm Fu](https://github.com/aksommerville/hummfu)
- [Mysteries of the Crypt](https://github.com/aksommerville/myscrypt)
- [When You Wish Upon A Bone](https://github.com/aksommerville/wishbone)
- [Queen of Clocks](https://github.com/aksommerville/queenofclocks)
- [Vexularg](https://github.com/aksommerville/vexularg)
- [Inversion](https://github.com/aksommerville/inversion)


## TODO

- [ ] alsafd, pi 4, hdmi audio: Long lead time lost, and playhead is way off. (Cherteau is unplayable, and all games, the lead loss is noticeable)
- [ ] Editor: Global action to reorder maps, eg for a game like zennoniwa or inversion.
- [x] MapService: Incorrectly handling planeless solo maps, it creates a 1x1 plane containing whichever map showed up last at (0,0,0). Evident in bellacopia.
- [x] Map editor: New door should offer to make a new map too.
- [x] In bellacopia, I keep getting prompted to create a new neighbor map when one does exist, visible in canvas and everything.
- - Happens reliably on a fresh load, after the first change. Watch the map editor's tattle as the save processes.
- - ...due to MapService searching by MapRes identity. Use rid instead.
- [ ] Revise SDK instruments, after some playing around.
- [x] ModecfgModalDrum: Use peak trim and pan during individual sound playback.
- [x] ModecfgModalDrum: Can we demo the kit more broadly? I need to carefully compare eg hihat vs kick.
- [x] Web: Playhead keeps reporting after song ends (without repeat). Game-breaking in Cherteau, the dance-off.
- [ ] native: Record and playback session.
- [ ] native: Global config file. Command-line options, and also persist `egg_prefs_set()` here.
- [ ] eggdev client: Detect changes to shared_symbols.h and rebuild symbols when changed. Currently you have to restart the server if you change symbols. Need a generalization of inotify. Not trivial.
- [ ] pulse: Fudged the estimated buffer length up 4x to avoid negative time-remaining. Can we fix it for real?
- [ ] windows: System language
- [ ] macos: System language
- [ ] macos: eggrun
- [ ] EGG_GLSL_VERSION. Currently pretty hacky.
- [ ] Web Video: Determine whether border is necessary. For now we are applying always. That's wasteful, but should be safe at least.
- [ ] Web incfg could bear some prettying-up.
- [ ] Add a fullscreen toggle in the universal menu.
- [ ] Review all "TODO" in source, there's a ton of them.
- [ ] eggsamples: Bring back "Hard Boiled" from a couple Eggs ago. Nice game, and now that we have mouse support, we can do it for real.
- - egg-202408 is so different from v2, I think a full rewrite would be easier. It's not complicated. The irreplaceable bit is the graphics.
- [ ] Enormous effort, but how do you feel about migrating or rewriting old non-Egg games? Could make provisioning new kiosks a lot smoother.
- - [x] Upsy-Downsy -- least difficult and most beneficial of these. We could then retire `pebble`
- - - ...surprisingly easy to convert.
- - For the rest of these, it's kind of hard to picture a lift-n-shift like Upsy-Downsy and the Egg1s. I think they'd all be full rewrites.
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
