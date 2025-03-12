# Egg Decalsheet Format

Contains instructions for slicing arbitrary rectangles out of an image.

Each decal has an ID in 1..255, 16-bit bounds, and an arbitrary comment you provide.

## Binary

```
   4  Signature: "\0EDS"
   1  Comment size.
 ...  Decals:
         1  Decal ID.
         2  X
         2  Y
         2  W
         2  H
       ...  Comment.
```

Decals must be sorted by ID, zero is illegal, and duplicates are illegal.

## Text

Line-oriented text.
`#` begins a line comment, start of line only.

`DECALID X Y W H [COMMENT]`. COMMENT is a hex dump up to 510 digits.

Decals do not need to be sorted but must be unique.
All comments pad with zeroes to the longest length.
