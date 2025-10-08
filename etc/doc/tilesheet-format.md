# Egg Tilesheet Format

Tilesheets are multiple tables of 256 data points each, corresponding to 256 tiles of an image.
The tilesheet rid should be the same as the image rid it describes.

## Binary

```
   4  Signature: "\0ETS"
 ...  Runs:
         1  Table ID, >0.
         1  Tile ID.
         1  Tile count - 1.
       ...  Content, one byte per tile.
```

It is an error if any (Tile ID + Tile count) exceeds 256.
Any unspecified tiles are zero.

No order, overlap, or adjacency constraints on runs.

## Text

Line-oriented text.
Comments and empty lines are permitted only between tables.

Each table begins with a line containing the integer table id, or a name in `shared_symbols:NS_tilesheet_*`.
Followed by 16 lines of 32 hex digits each.

Named tables, their value is >0 to store normally (tableid).
If the value is zero, the table is dropped at compile, and only available to the editor.

The following tables are used by editor if present:
- `family`: Arbitrary assignment for a set of joinable tiles. 0 if none.
- `neighbors`: Mask of expected neighbors in the same family. 0x80..0x01 = NW,N,NE,W,E,SW,S,SE
- `weight`: Inverse relative likelihood of this tile, when there's multiple candidates. 0=likely, 254=unlikely, 255=appointment-only.

`weight==255` (appointment-only) means it participates in neighbor detection, but will never be selected or modified automatically.

Additionally, if you declare a namespace that matches the table name, editor will show hints.
eg `NS_tilesheet_physics`, then `NS_physics_vacant=0`, `NS_physics_solid=1`, and you'll see helpful "vacant", "solid" in the editor.
