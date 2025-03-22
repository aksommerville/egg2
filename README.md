# egg2

2025-03-12: I'd been working on this for about a month and accidentally deleted it all.
Nice one, Andy. Don't do that again.
For reference, here's what we had before the deletion:
- Custom build tool with multiple arbitrary targets.
- Abstract data conversion.
- Most of the native synthesizer: FM, Sub, and Drum voices. Post pipes. Note and Wheel events.
- Platform API defined, headers only. Single render call with GPU-ready vertex buffer. Optional mouse, keyboard, and touch events.
- Many data formats defined: ROM, metadata, strings, map, cmdlist, tilesheet, decalsheet, EAU, EAU-Text
- Test harness.

Planned differences from Egg v1:
- Custom build tool, similar to berry. Arbitrary targets selected at eggdev's build time, and web is not special.
- Direct access to data conversion a la carte via eggdev.
- Synth: Post pipes, LFOs, and stereo output.
- More input options: Keyboard, mouse, touch. Maybe accelerometer?
- Single render call taking a struct of uniforms and a GPU-ready vertex buffer.
- No image decoder at runtime (egg 1 used to do this; it's actually been removed already).
- Rich built-in menu. Quit, input config, toggle music, toggle sound, language.
- Web apps pack to a Zip file with boilerplate HTML and the binary ROM. Same as berry. Also continue to allow standalone HTML as an option.
- eggdev --help reads from etc/doc/eggdev-cli.md directly.
- Default instruments live somewhere in the SDK.
- No Wasm runtime for native builds. If we want that in the future, it should be achievable but I don't expect to want it.

# TODO

- [x] Define data formats.
- - [x] EAU
- [x] Build config.
- [x] Test harness.
- [x] Zip utility unit.
- [x] eggdev
- - [x] Generic outer layers.
- - [x] build
- - [x] minify
- - [x] serve
- - [x] convert
- - [x] config
- - [x] metadata
- - [x] unpack
- - [x] pack
- - [x] list
- - [x] run: Same as build, but after a success launch the native executable. (update the project wizard's makefile too)
- - [x] dump: Convenience to extract one resource from a ROM and dump it.
- [ ] Native runtime.
- - [x] Load.
- - [x] Pick language.
- - [x] Event queue.
- - [ ] Synth.
- - - [x] Data converters.
- - - [ ] Global orchestration, song player, etc.
- - - [ ] FM channel
- - - [ ] Drum channel
- - - [ ] Sub channel
- - - [ ] Post
- - [ ] Render.
- - [ ] Input.
- - - [ ] Device mapping.
- - - [ ] Persist mappings.
- - - [ ] egg_gamepad_get_button: Need to cache full capability reports in inmgr.
- - - [ ] Interactive reconfig.
- - [x] Persistence.
- - [ ] Record and playback session.
- [ ] Web runtime.
- - [ ] Load binary.
- - [ ] Load base64.
- - [ ] Synth.
- - [ ] Render.
- - [ ] Input.
- - [ ] Persistence.
- [ ] Editor.
- [ ] Client utilities.
- - [ ] stdlib
- - [ ] graf
- - [ ] font
- Minor things punted:
- - [ ] Loop position in MIDI.
- - [ ] WAV from EAU, can we get rate and chanc from the caller somehow?
- - [ ] System language, for MacOS and Windows.
- - [ ] Expose a GM names service, using text ripped from eggdev/instruments.eaut dynamically.
