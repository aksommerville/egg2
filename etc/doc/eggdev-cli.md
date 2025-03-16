# eggdev Command Line Interface

This file is consumed by `eggdev --help`.
I'm also keeping it legible to be read directly by humans.
Anything at h2 level here is addressable as `eggdev --help=TOPIC`.

## build

Usage: `eggdev build [PROJECT_DIR]`

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
