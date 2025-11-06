# Eggdev Build Process

The `eggdev` tool can build Egg projects in one shot, just `eggdev build` from the project's root.

This will produce an executable for every target you configured when building Egg.

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
- `eggrt_init()` begin configure and the first update.
- `eggrt_update()` repeatedly. It may block.
- `eggrt_quit(status)` before terminating.

I'm not sure whether there's any point to headless.
It's easy to build so we do.
Might be cases where you want to embedded an Egg runtime alongside other runtimes?
