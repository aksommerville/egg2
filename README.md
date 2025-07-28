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
- - [ ] web: Detect loss of focus. At a minimum, pause audio. Maybe pause everything? Probly needs new soft-pause support in synth.
- - [ ] web Video: Load raw pixels with non-minimum stride
- - [ ] web `egg_texture_get_pixels`
- - [ ] web Video: Determine whether border is necessary. Apply to main fb as needed too; right now it's only situated for id>1 textures.
- - [ ] web: Quantize final scale-up, don't use `object-fit:contain`. Then implement `egg_video_fb_from_screen`
- - [ ] web: Player count 
- 2025-06-23 audio rekajiggerment:
- - [ ] Update editor.
- - - [ ] New serial format.
- - [ ] Define some instruments.
- - [ ] Test perceptually.
- - - Pay close attention to FM. I hacked it fast and loose for web, probably got it all wrong.
- - - Add some editor tooling to print a song both native and web, and play them back with an easy toggle.
- - [ ] Confirm we can get decent whoosh, click, and snap sounds without subtractive voices. I'm not sure we can.
- - [ ] `eggdev_convert_audio.c:eggdev_wav_from_eau`: Arbitrary params from user for conversion? (rate,chanc,method) in this case.
- - [ ] Sounds require an explicit terminal delay. Have editor create this automagically from the events.
- - [ ] `eau-format.md`: "Events for a channel with no header will get a non-silent default instrument.". Confirm we're doing this in both implementations.
- - - We're not. And if it seems burdensome, we can change the spec to require CHDR.
- - - If we change the spec, ensure that MIDI=>EAU generates all CHDR. Not sure whether it does.
- - [ ] `eau-format.md`: "Duration of a sound is strictly limited to 5 seconds.". I didn't implement this yet.
- - [ ] Web synth: tuned voices use the oscillator's `detune` for both wheel and pitchenv. I expect they will conflict.
- - [ ] Web playhead incorrect for songs shorter than the forward period. That's a tricky one, and not likely to matter.
