# Eggdev Build Process

The `eggdev` tool can build Egg projects in one shot, just `eggdev build` from the project's root.

This will produce an executable for every target you configured when building Egg.

Typical Egg projects ship with a Makefile which just calls `eggdev build`.
You should prefer to call `make`, `make run`, `make edit`, and `make web-run` rather than calling eggdev directly.

## Targets

A typical Egg installation has two targets: `web`, and whatever's appropriate to the host (`linux`, `macos`, `mswin`, ...).

Your targets are listed as `EGG_TARGETS` in `EGG_SDK/local/config.mk`.

Each of those must define a few fields:
- `{{TARGET}}_OPT_ENABLE`: Names of directories under `EGG_SDK/src/opt/` to include in the build.
- `{{TARGET}}_CC`, `{{TARGET}}_AR`, `{{TARGET}}_LD`, `{{TARGET}}_LDPOST`: C toolchain.
- `{{TARGET}}_EXESFX`: ".exe" for mswin, blank for others.
- `{{TARGET}}_PACKAGING`: Tells eggdev the shape of the finished product. One of: `exe`, `web`, `macos`.
- - `exe`: A self-contained executable linked against `EGG_SDK/out/TARGET/libeggrt.a`.
- - `web`: Zipped HTML and ROM.
- - `macos`: Same as `exe`, but also some ancillary MacOS app bundle bits.

## Runtime Libraries

Normally games link against `libeggrt.a`, which contains the Egg Runtime and also `main()`.

There's an alternative `libeggrt-headless.a`, which is the same thing but no `main()`.
If you link against headless, you must supply `main()`, and call:
- `eggrt_configure(argc,argv)` as early as possible.
- `eggrt_init()` between configure and the first update.
- `eggrt_update()` repeatedly. It may block.
- `eggrt_quit(status)` before terminating.

I'm not sure whether there's any point to headless.
It's easy to build so we do.
Might be cases where you want to embedded an Egg runtime alongside other runtimes?

## Project Layout

This is also discussed in `rom-format.md`.

`eggdev build` expects this structure:
```
MY_PROJECT/
  out/ : Final artifacts go here. Created automatically. Should be gitignored.
  mid/ : Intermediate artifacts go here. Created automatically. Should be gitignored.
  Makefile
  README.md
  etc/ : Your commentary, notes, whatever. Egg ignores.
  src/ : All inputs to the build process should live in here.
    game/: All game code.
      shared_symbols.h : A special header that eggdev uses during resource compilation.
      ...
    data/: Structured resource file inputs. See rom-format.md.
    editor/: Extensions for the web-based resource editor.
    tool/: Explicitly ignored.
```

Perfectly sensible to build your own tools, under `src/tool/`.
Orchestrate the build yourself in your Makefile.
