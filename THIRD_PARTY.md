# Third-party notices

Original code and original UI/Everyday emoji artwork are covered by the root
[MIT license](LICENSE). The following data and font-derived resources retain
their own licenses. Preserve their notices when copying or packaging them.

## Oklahoma Bold bitmap font — Panic, Inc.

The current `Source/keyboard/glyphs-table-12-15.png` contains printable ASCII
font glyphs. The earlier implementation's source bitmap is retained at
`legacy/lua/tools/assets/Oklahoma-Bold-table-12-15.png`; archived font-derived
sprites and composed UI bitmaps are under `legacy/lua/Source/keyboard/art/`.
These resources use **Oklahoma Bold**, supplied with the Playdate SDK by
**Panic, Inc.**, under [Creative Commons Attribution 4.0 International](https://creativecommons.org/licenses/by/4.0/).
The bitmap was cropped into glyph sprites and composed into image tables and
UI bitmaps. Original surrounding UI shapes remain MIT licensed.

- [Playdate developer resources](https://play.date/dev/)
- [Official SDK legal information](https://sdk.play.date/inside-playdate/#_legal_information)

Keep this attribution and the license link with those font-derived resources.
The Playdate SDK itself is an external build dependency, not bundled here.

## Unicode data

`src/unicode_case_data.h`, `src/unicode_grapheme_data.h` and the Unicode data
under `tools/data/unicode/` are derived from the Unicode Character Database
and emoji data. License: **Unicode License v3**; see
[`Source/licenses/unicode.txt`](Source/licenses/unicode.txt) and
[`tools/data/unicode/LICENSE.txt`](tools/data/unicode/LICENSE.txt).
The Unicode corpus is used by the tests and does not need to ship in a game.

## GNU Unifont / Unifont Upper 18.0.01

The optional `Source/unicode/glyphs.bin` fallback atlas includes glyphs from
GNU Unifont and Unifont Upper 18.0.01. The original lower- and upper-plane source
files were identified by matching their SHA-256 checksums against the
[official release](https://unifoundry.com/pub/unifont/unifont-18.0.01/font-builds/).
The font is dual licensed; this distribution uses its **SIL Open Font License
1.1** option. Exact upstream attribution, source hashes and the license text
are retained in [`Source/licenses/unifont.txt`](Source/licenses/unifont.txt).
The packed atlas is a modified storage format and retains the font license.

## Noto Emoji

The same optional fallback atlas also includes monochrome rasterizations of
**Noto Emoji Regular, Version 3.002**, copyright **2013 Google LLC**, under the
**SIL Open Font License 1.1**. See
[`Source/licenses/noto-emoji.txt`](Source/licenses/noto-emoji.txt) and the
[upstream Noto Emoji project](https://github.com/googlefonts/noto-emoji).
The 21 hand-pixelled Everyday motifs embedded in `src/emoji_everyday_data.h`
are original artwork; they are separate from these font-derived fallback glyphs.

Playdate is a product of Panic. This is an independent community library and
does not imply endorsement by Panic or any font/data contributor.
