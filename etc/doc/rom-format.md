# Egg ROM Format

Egg games ship as a ROM file.
This is an opaque binary file containing all of the game's assets.
For web builds, it also contains the WebAssembly code.
For native builds, it's embedded in the native executable.

## Binary

```
   4  Signature: "\0ERM"
 ...  Commands.
```

Read commands bytewise with a state machine:
```
  int tid = 1
  int rid = 1
```

High bits of the first byte tell you how to process each command:
```
00000000                   : EOF. Stop reading. Required.
00dddddd                   : TID. (d) nonzero. tid+=d, rid=1
01dddddd dddddddd          : RID. rid+=(d+1)
10llllll llllllll llllllll : RES. Followed by resource of length (l+1). rid+=1
11xxxxxx                   : RESERVED, illegal.
```
EOF is required.
Decoders must check (tid) and (rid) for overflow. Limited to 255 and 65535 respectively.
Per this format, it is impossible for resources not to be sorted by (tid,rid), and IDs of zero are also impossible.
Zero-length resources are not possible.
Longest possible resource is exactly 4 MB.

## Loose Directory

Conventionally, Egg projects will be laid out like so:
```
PROJECT/
  src/
    data/ <-- Arguably the ROM starts here.
      metadata
      code.wasm
      TYPE/
        [LANG-]RID[-NAME][[.COMMENT].FORMAT]
    game/
      main.c
      shared_symbols.h
    editor/
      override.css
      override.js
  etc/
```

`metadata` and `code.wasm` are special. They become resources metadata:1 and code:1.

`LANG` is two lowercase letters, an ISO 631 language code. If present, RID must be in 1..63. The full rid is `((LANG[0]-'a'+1)<<11)|((LANG[1]-'a'+1)<<6)|RID`.

`RID` is a decimal integer 0..65535.

`NAME` is an optional C identifier. This will be made available to client code via the ROM TOC Header.

`COMMENT` are extra processing instructions to the compiler. Only allowed if `FORMAT` also present. May contain dots.

`FORMAT` is as usual, eg "png", "mid"...

## Final Packaging

Native builds contain an embedded ROM with no code resource, exported as `_egg_embedded_rom` and `_egg_embedded_rom_size`.

"Separate" web builds, the preferred format, are a Zip file containing at the root "game.bin" (ROM) and "index.html" (boilerplate).
index.html expects game.bin to be adjacent to it once deployed.
There used to be "Standalone" web builds, a single HTML file, but due to the synth's architecture that is no longer possible.

## Standard Types

| tid      | name       | desc |
|----------|------------|------|
| 0        |            | ILLEGAL |
| 1        | metadata   | Required, rid 1 only. See metadata-format.md. |
| 2        | code       | Required for web, rid 1 only. WebAssembly module. |
| 3        | strings    | Multiple chunks of text addressable by index. See strings-format.md. |
| 4        | image      | PNG. |
| 5        | song       | EAU. See eau-format.md. |
| 6        | sound      | Also EAU; song and sound are the same thing. |
| 7        | tilesheet  | Convenience. See tilesheet-format.md. |
| 8        | decalsheet | Convenience. See decalsheet-format.md. |
| 9        | map        | Convenience. See cmdlist-format.md. |
| 10       | sprite     | Convenience. See cmdlist-format.md. |
| 11..31   |            | Reserved for future standard types. |
| 32..127  |            | Reserved for client use. |
| 128..255 |            | Reserved for I don't know what. |

Every ROM must contain metadata:1, and owing to the format, it must begin at byte 7 in the ROM.
That knowledge really shouldn't be helpful; it's easy and safe to read ROM files correctly. But still kind of interesting.
