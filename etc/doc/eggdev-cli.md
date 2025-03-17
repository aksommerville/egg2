# eggdev Command Line Interface

This file is consumed by `eggdev --help`.
I'm also keeping it legible to be read directly by humans.
Anything at h2 level here is addressable as `eggdev --help=TOPIC`.

## build

Usage: `eggdev build [PROJECT_DIR]`

Detailed documentation about the build process is at EGG_SDK/etc/doc/eggdev-build.md.

## minify

Usage: `eggdev minify -oDSTPATH SRCPATH`

Minify HTML, Javascript, and CSS files.
Resolves imports, must be relative.
You may omit either path to use stdout and stdin.

## serve

Usage: `eggdev serve [--port=8080] [--unsafe-external] [--htdocs=[REMOTE:]LOCAL ...] [--writeable=LOCAL] [--project=DIR]`

HTTP server for serving our editor or the game.
**This is not suitable for use on the internet.**

Detailed documentation is at EGG_SDK/etc/doc/eggdev-http.md.

## convert

Usage: `eggdev convert -oDSTPATH SRCPATH [--dstfmt=FORMAT] [--srcfmt=FORMAT]`

Convert anything to anything.
Omit paths to use stdout and stdin.
We can usually infer `srcfmt` from the content and `dstfmt` from the path.
Use the special dstfmt "rommable" or "portable" for conversions to or from our standard resource formats.

## config

Usage: `eggdev config [KEYS]`

With no arguments, print the entire build-time configuration, one line per field, prefixed with keys.
Otherwise print the specified fields, value only.

## project

Usage: `eggdev project`

Interactively initialize a new Egg project.
Call from the directory where you want the project directory created.

## metadata

Usage: `eggdev metadata ROM [--lang=ISO631|all] [KEYS...]`

Dump fields from `metadata:1`. Use the `KEYS` listed, or every field by default.

If a language is specified, we will try to resolve fields ending in `$` by looking up the appropriate string resource.
We won't print empty strings. If it's not found, ultimately we print the string index (what's stored in metadata).

With `--lang=all`, we look up dollar fields and print every non-empty language option.
That looks like `title$[en]=My Game`, `title$[fr]=Mon Jeu`, plus `title=The Default` if it was present.

## pack

Usage: `eggdev pack -oROM DIRECTORY [--verbatim]`

Generate a ROM file from loose inputs. See `etc/doc/rom-format.md` for notes on the directory layout.

By default, we will try to compile known resource types to their "rommable" form.
Use `--verbatim` to suppress that and copy resources exactly as they are.

Note that the usual build flow does not need this. Usually `eggdev build` is all you need.

## unpack

Usage: `eggdev unpack -oDIRECTORY ROM [--verbatim]`

Extract a ROM file's contents into a new directory.
`ROM` may also be an executable, a standalone HTML, or a Zip file containing `game.bin`.

By default, we will try to convert known resource types to their "portable" form.
Use `--verbatim` to suppress that and copy resources exactly as they are.
