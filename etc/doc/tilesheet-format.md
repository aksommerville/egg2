# Egg Tilesheet Format

Tilesheets are multiple tables of 256 data points each, corresponding to 256 tiles of an image.
The tilesheet rid should be the same as the image rid it describes.

## Binary

```
   4  Signature: "\0ETS"
 ...  Runs:
         1  Table ID, >0.
         1  Tile ID.
         1  Tile count.
       ...  Content, one byte per tile.
```

It is an error if any (Tile ID + Tile count) exceeds 256.
Any unspecified tiles are zero.

## Text

Line-oriented text.
No comments.
Empty lines are ignored.

Each table begins with a line containing the integer table id, or a name in `shared_symbols:NS_tilesheet_*`.
Followed by 16 lines of 32 hex digits each.

Named tables, their value is >0 to store normally (tableid).
If the value is zero, the table is dropped at compile, and only available to the editor.
