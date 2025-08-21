# Egg Strings Format

Pack multiple strings into one file to retrieve with an index 1..1024.

## Binary

```
   4  Signature: "\0EST"
 ...  Strings, starting with index 1:
        2  Length
      ...  Content, UTF-8.
```

Zero-length entries are perfectly legal, to skip an index.
Index above 1024 is an error.

## Text

Line-oriented text.
`#` begins a line comment, start of line only.
Beware that our editor will strip comments and blanks on save.

Each non-empty line is: `INDEX TEXT...` or `INDEX JSON_STRING`

INDEX is a decimal integer 1..1024.
Strings must be sorted by index and duplicates are an error.
You're allowed to skip indices.
The loose text option can't have leading or trailing whitespace, and can't begin with a quote.
