# Egg Command List Format

This is a flexible binary and text format that we use for map and sprite resources.
It is also available for general use by clients.

Define your formats in `shared_symbols.h`, with tokens `CMD_{{restype}}_{{cmdname}} {{opcode}}`.
Because payload length is an intrinsic part of opcode, our tooling can validate things for you generically.

## Binary

Stream of commands where the first byte is opcode, and its high 3 bits indicate the payload length:
- `00000000`: Illegal.
- `000xxxxx`: No payload.
- `001xxxxx`: 2 bytes.
- `010xxxxx`: 4 bytes.
- `011xxxxx`: 8 bytes.
- `100xxxxx`: 12 bytes.
- `101xxxxx`: 16 bytes.
- `110xxxxx`: 20 bytes.
- `111xxxxx`: Next byte is the remaining length.

## Text

Line-oriented text.
`#` begins a comment, start of line only.

Each line starts with opcode name or integer 1..255.

Followed by space-delimited arguments:
- `0x` followed by hex digits for a hex dump of arbitrary length.
- Other naked integers emit as u8.
- `(u8[:NAMESPACE])VALUE` to evaluate `VALUE` in the context of `NAMESPACE` and emit the given size (8, 16, 24, 32) big-endianly.
- `(b8[:NAMESPACE])VALUE[,VALUE,...]` to combine multiple `(1<<VALUE)` bitfields.
- `TYPE:NAME` for a 16-bit resource ID. Note that the type id is not recorded, only for lookup.
- JSON strings emit as verbatim UTF-8.
- `@N,N[,N,N,...]` emits each `N` as u8. This format is a marker for our map editor.

## Map

Binary has a preamble:
```
   4  Signature: "\0EMP"
   1  Width
   1  Height
 ...  Cells (Width*Height)
```

TODO Consider trivial compression for the cells, they tend to get big. RLE or a table of expectations per tileid?

Followed by cmdlist.

Text begins with the cells image as a rectangular hex dump terminated by an empty line.

## Sprite

Just cmdlist, but binary includes a signature first:
```
   4 Signature: "\0ESP"
```
