# Egg Metadata Format

The metadata resource is a set of key=value pairs, each string being 0..255 bytes of ASCII.
Some keys are for machine consumption, things the platform might need to load your game.
Others are loose text for human consumption.

A key ending '$', the value is a decimal integer for an index in strings:1.
By this mechanism, you can have values longer than 255 bytes, use UTF-8, and have different values per language.
When using $, you should also include the non-dollar key with a default value.

All standard keys will be C identifiers beginning with a lowercase letter.
Not sure why you'd want to, but you're free to define custom keys.
Use reverse-DNS or uppercase letters or something to distinguish from standard keys.

## Binary

```
   4  Signature: "\0EMD"
 ...  Fields:
        u8 kc >0
        u8 vc
       ... k
       ... v
   1  Terminator = 0
```

Terminator is required.
Empty keys are forbidden.
Key and value content must be ASCII G0, with leading and trailing space forbidden.

## Text

Line-oriented text.
`#` begins a line comment, start of line only.
`KEY = VALUE`, spaces optional.

## Standard Keys

| key         | desc |
|-------------|------|
| fb          | "WIDTHxHEIGHT", framebuffer size. 640x360 if unspecified. |
| title       | Game's title eg for window title. |
| author      | Your name. |
| copyright   | eg "(c) 2025 AK Sommerville". |
| freedom     | License summary. See below. |
| iconImage   | image rid, recommend 16x16. |
| posterImage | image rid, recommend 2:1 aspect, opaque, with no text. |
| lang        | Comma-delimited ISO 631 language codes, in order of preference. |
| desc        | Loose description for human consumption. |
| advisory    | Freeform warning of gore, obscenity, etc. |
| rating      | Machine-readable ratings from official agencies. See below. |
| required    | Machine-readable feature flags. Runtime should refuse to launch if unavailable. See below. |
| optional    | Same flags as `required` but allow to launch without. |
| version     | Version of your game (not of Egg). Recommend eg "v1.2.3". |
| time        | Publication time. ISO 8601, typically you'll stop after the year. |
| genre       | Arbitrary. Recommend using strings that have been seen on Itch.io, as a reference. |
| tags        | Like genre. Comma-delimited. |
| contact     | Email, phone, URL, or whatever, for human consumption only. |
| homepage    | URL of game's home page. |
| source      | URL of source code, presumably clonable with git. |
| players     | "MIN..MAX" decimal. |
| persistKey  | If present, contributes to store isolation. Only versions of your game with the same persistKey will see the same store. |
| incfgMask   | Which gamepad keys are used? Some combination of: dswen123lrLR |
| incfgNames  | Decimal index in strings:1, for the first key in incfgMask. Must be followed by as many keys as you listed. For interactive configurer. |
| revdns      | Reverse-DNS namespace for this game. MacOS builds require it. "com.aksommerville.unspec.{{PROJECT}}" if you don't specify. |
| menu        | "default" or omit to enable, or "none" to suppress the Universal Menu. eg if game must be black-and-white or low-resolution. |

`freedom` is `limited` if unspecified, and is only a convenient summary of your game's real license:
- `free`: Assume you are allowed to reuse assets and redistribute freely.
- `intact`: Assume you are allowed to redistribute but only if unmodified. Assume extracting assets is forbidden.
- `limited`: Don't assume anything; look for the license.
- `restricted`: You're not allowed to do anything except play, and maybe not even that. See license.

`rating`: TODO

`required` and `optional` are comma-delimited lists:
- `audio`: Fail fast if audio disabled. eg for rhythm games where it's absolutely necessary.
- `store`: Fail fast if permanent storage will not be available.

`incfgMask` and `incfgNames` influence the interactive input configurer.
`incfgMask` must be some combination of "dswen123lrLR", where "d" means the D-pad, "123" are AUX, "lr" are the first triggers, and "LR" the second triggers.
List buttons the game uses, in order of importance. We won't prompt the user for ones you don't use.
`incfgNames` is an index in strings:1 corresponding to the first char in `incfgMask`.
Must be followed by as many buttons as you list, with a name for each.
It's fine to omit button names. The configurer presents them visually. But it's an opportunity to say "Jump" instead of just "That button over there".
