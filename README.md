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
- ~More input options: Keyboard, mouse, touch. Maybe accelerometer?~ Decided against this, after getting partway in.
- Single render call taking a struct of uniforms and a GPU-ready vertex buffer.
- No image decoder at runtime (egg 1 used to do this; it's actually been removed already).
- Rich built-in menu. Quit, input config, toggle music, toggle sound, language.
- Web apps pack to a Zip file with boilerplate HTML and the binary ROM. Same as berry. Also continue to allow standalone HTML as an option.
- eggdev --help reads from etc/doc/eggdev-cli.md directly.
- Default instruments live somewhere in the SDK.
- No Wasm runtime for native builds. If we want that in the future, it should be achievable but I don't expect to want it.

# TODO

- [ ] Native runtime.
- - [ ] Input.
- - - [ ] Persist mappings.
- - - [ ] Interactive reconfig.
- - - - [ ] Let the client declare which buttons it uses, so when configuring we don't ask for all 15 buttons.
- - - - - Maybe a metadata field "incfgMask" containing characters "dswne123lrLR". "d" being the dpad, all others correspond to one button.
- - - [ ] Nix queue.
- - [ ] Record and playback session.
- [ ] Web runtime.
- - [ ] Synth.
- - [ ] Input.
- - - [ ] Mapping.
- - - [ ] Live config.
- - [ ] prefs
- [ ] Editor.
- - [ ] `eggdev project`: Prep overrides.
- - [ ] Standard actions... what's needed?
- - [ ] Verify overrides
- [ ] Client utilities.
- - [ ] stdlib
- - [ ] graf
- - [ ] font
- Minor things punted:
- - [ ] System language, for MacOS and Windows.
- - [ ] eggdev_main_project.c:gen_makefile(): Serve editor and overrides.
- - [ ] HexEditor: Paging, offset, ASCII, multi-byte edits.
- - [ ] ImageEditor: Animation preview like we had in v1.
- - [ ] StringsEditor: Side-by-side editing across languages, like we had in v1.
- - [ ] SidebarUi: Highlight open resource.
- - [ ] DecalsheetEditor: Validation.
- - [ ] Generic command list support. Can we read a command schema off a comment in shared_symbols.h?
- - [ ] POI icons for sprite and custom overrides.
- - [ ] native inmgr: Select player
- - [ ] eggdev build: Replace `<title>` in HTMLs.
- - [ ] web: Detect loss of focus. At a minimum, pause audio. Maybe pause everything?
- - [ ] web Video: Load raw pixels with non-minimum stride
- - [ ] web `egg_texture_get_pixels`
- - [ ] web Video: Determine whether border is necessary. Apply to main fb as needed too; right now it's only situated for id>1 textures.
- - [ ] web: Quantize final scale-up, don't use `object-fit:contain`. Then implement `egg_video_fb_from_screen`
- - [ ] web: Player count 

## 2025-06-23 resynth

I stepped away from this project for a while to play with AudioWorkletNode.
It's enticing... In theory, we could write one synthesizer in C, and use it in both web and native.
It does work, but it makes the plumbing quite a bit more complex (also it requires games to be served HTTPS, which is sure to be a problem).
Anyhoo, gave up on that idea (for now) and returning to Egg 2, and I'm going to scrap its current synth and rewrite.

The broad outline:
 - Separate web and native implementations. :(
 - One format, "EAU", for delivery at runtime. Source from MIDI or EAU-Text.
 - Stereo.
 - Per-channel post, LFO, trim, pan.
 - Limited set of post stages, only things we can reliably implement on both sides: Delay, Waveshaper, Tremolo
 - No SUB voices. Too hard to maintain parity across implementations.
 - No IIR post stages, same reason.
 - Implicit PCM printing.
 
TODO:
- [x] Define data formats.
- [x] Portable data converter unit.
- [x] Define native synth API.
- [x] Update eggdev.
- [x] Native synth implementation.
- [ ] Web synth implementation.
- - [x] Print PCM.
- - [ ] Soft pause.
- - [ ] Correct getPlayhead (currently not wrapping)
- - [x] Tuned voices wheel
- - [x] Tuned voices pitchenv
- - [x] Stereo
- - [ ] Post
- [ ] Update editor.
- [ ] Define some instruments.
- [ ] Test perceptually.
- - Pay close attention to FM. I hacked it fast and loose for web, probably got it all wrong.
- [ ] Confirm we can get decent whoosh, click, and snap sounds without subtractive voices. I'm not sure we can.
- [ ] `eggdev_convert_audio.c:eggdev_wav_from_eau`: Arbitrary params from user for conversion? (rate,chanc,method) in this case.
- [ ] Sounds require an explicit terminal delay. Have editor create this automagically from the events.
- [ ] `eau-format.md`: "Events for a channel with no header will get a non-silent default instrument.". Confirm we're doing this in both implementations.
- - We're not. And if it seems burdensome, we can change the spec to require CHDR.
- - If we change the spec, ensure that MIDI=>EAU generates all CHDR. Not sure whether it does.
- [ ] `eau-format.md`: "Duration of a sound is strictly limited to 5 seconds.". I didn't implement this yet.
- [x] !!! minify: `const frequency = 256000 / (lforate * this.player.tempo);` became `const ex=256000/eu*this[h8][hO]`, missing parens!
- [ ] Web synth: tuned voices use the oscillator's `detune` for both wheel and pitchenv. I expect they will conflict.
