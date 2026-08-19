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
- Below this point were written first in Egg v2. Above were written for something else, mostly Egg v1, and migrated later.
- [Zen Garden](https://github.com/aksommerville/zennoniwa)
- [Humm Fu](https://github.com/aksommerville/hummfu)
- [Mysteries of the Crypt](https://github.com/aksommerville/myscrypt)
- [When You Wish Upon A Bone](https://github.com/aksommerville/wishbone)
- [Queen of Clocks](https://github.com/aksommerville/queenofclocks)
- [Vexularg](https://github.com/aksommerville/vexularg)
- [Inversion](https://github.com/aksommerville/inversion)
- [Licence to Illuse](https://github.com/aksommerville/licensetoilluse)
- [XRM: Extreme Racing Machines](https://github.com/aksommerville/xrm)
- [Kleptomania](https://github.com/aksommerville/kleptomania)
- [All Fifty Two](https://github.com/aksommerville/all52)

## TODO

- [ ] I feel we've outgrown `image_decode` and `image_encode`. There's often need for more detailed analysis at the caller's scope.
- [ ] Rendering filtered decals into an offscreen texture, it seems they are not blending; looked like alpha was copying like a color. Worked around it but this does need fixed.
- [ ] `eggdev build`: Can we ignore changes to shared_symbols.h for purposes of dirty detection? Rebuilding the whole project when I add something is getting old.
- - Most games it just doesn't matter, but at the size of bellacopia, it is a thing.
- - Anything that engages the `FOR_EACH_` macros should rebuild. What if we shlep those off into a separate file? One we build automatically?
- [ ] MacBook: Touchpad doesn't work right. Two fingers makes a left click, and right click doesn't seem to be possible.
- [ ] MacBook: Crashed with no detail after plugging in the Genesis knockoff gamepad. And won't start while it's plugged in.
- - Same behavior with El Cheapo. Tried the 8bitdo SNES and there was just no reaction at all. I guess Gen and ElC are working, and our HID driver is crashing on them somewhere.
- - It's a damn shame, because in most other ways, Bellacopia runs great on the Macbook.
- [ ] Badly need both a fullscreen toggle and some command-line option to select a screen. I can't run fullscreen on the big TV :(
- [ ] Why does `eggdev run` build for web? It only needs the native executable. A project the size of bellacopia, it does matter.
- [ ] Consider adding an alignment option in font. Left,center,right.
- [ ] web: If an input state is nonzero at launch, wait for it to clear. This is a problem when launching via ra4 web; the button that starts the game gets picked up as a keystroke in-game too.
- [ ] Would it make sense to generate `FOR_EACH_` macros in the res toc for all NS symbols? It's mildly annoying to declare them manually.
- [ ] Does MapEditor not show region commands? I'm using them in `xrm`. ...got them preserving at least. Rendering will be a whole other thing.
- [ ] SongEditor: Consider removing the action "Auto end time" and just do it every time without asking.
- [ ] Editor sidebar: Group resources when too many in a type. Maybe a limit of 100 per bucket? Bellacopia's maps and sprites are getting ridiculous.
- [ ] ^ Similar bucketting in the Sprites dropdown at new POI.
- [ ] Consider a spec change re multiple `code` resources: Concatenate all, rather than just using id 1.
- - Finish Bellacopia Maleficia, it will have a ridiculous amount of code. If its code:1 is over 2 MB, make the change.
- - Actually, make the change regardless. One can imagine projects embedding their data instead of using resources, and 4 MB could be too small.
- [ ] alsafd, pi 4, hdmi audio: Long lead time lost, and playhead is way off. (Cherteau is unplayable, and all games, the lead loss is noticeable)
- [ ] Editor: Global action to reorder maps, eg for a game like zennoniwa or inversion.
- [ ] Revise SDK instruments, after some playing around.
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
- - Definitely not in scope: Chetyorska (MIDI-In), Pico Sitter (who cares), Bandit and earlier (source lost)
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
