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
- - [ ] Loop position in MIDI.
- - [ ] Also need to manage loop in editor Song.
- - [ ] WAV from EAU, can we get rate and chanc from the caller somehow?
- - [ ] System language, for MacOS and Windows.
- - [ ] Expose a GM names service, using text ripped from eggdev/instruments.eaut dynamically. <-- have it for instruments in editor... can we get drums too?
- - [ ] synth: Brief blackout on song transitions? Not sure whether it's needed.
- - [ ] Decide whether to allow WAV for sound resources.
- - [ ] Add phase to FM LFO. I think. Maybe?
- - [ ] Native FM pitch wheel.
- - [ ] Web FM pitch wheel.
- - [ ] FM: Should we adjust the modulator too, when pitchenv in play? Currently keeping it fixed.
- - [ ] Web SongChannel: Respect tremolo phase.
- - [ ] Web synth: First note seems to get dropped sometimes?
- - [ ] Finish default instruments and drums. Punt until there's a good editor with MIDI-In.
- - [ ] eggdev_main_project.c:gen_makefile(): Serve editor and overrides.
- - [ ] HexEditor: Paging, offset, ASCII, multi-byte edits.
- - [ ] ImageEditor: Animation preview like we had in v1.
- - [ ] StringsEditor: Side-by-side editing across languages, like we had in v1.
- - [ ] SidebarUi: Highlight open resource.
- - [ ] DecalsheetEditor: Validation.
- - [ ] Generic command list support. Can we read a command schema off a comment in shared_symbols.h?
- - [ ] POI icons for sprite and custom overrides.
- - [x] SongEditor: Playback (From SongEditor top, and also within ModecfgDrumModal)
- - [ ] SongEditor: Live MIDI-In. Requires web synthesizer.
- - [ ] SongEditor:WaveUi: Have wave preview printed by synthesizer. Currently calling out to the server for each render.
- - [ ] SongEditor: Must be able to change order of post steps.
- - [ ] native inmgr: Select player
- - [ ] eggdev build: Replace `<title>` in HTMLs.
- - [ ] web: Detect loss of focus. At a minimum, pause audio. Maybe pause everything?
- - [ ] web Video: Load raw pixels with non-minimum stride
- - [ ] web `egg_texture_get_pixels`
- - [ ] web Video: Determine whether border is necessary. Apply to main fb as needed too; right now it's only situated for id>1 textures.
- - [ ] web: Quantize final scale-up, don't use `object-fit:contain`. Then implement `egg_video_fb_from_screen`
- - [ ] web: Player count 
- - [ ] Synth early termination of Channel Header payload. Document expected behavior, defaults for each field, and ensure both implementations actually do it. (web at least does not)
