# Fonts

ani-switch needs a CJK-capable TTF that can cover the glyphs used by
zh-Hans, zh-Hant, ja, and ko.

Recommended setup for v0.1 (one font that covers all 4 locales):

1. Download **Source Han Sans / Noto Sans CJK SC** Regular and Bold:
   https://github.com/notofonts/noto-cjk/releases

2. Drop the two TTF files into this directory:

   - `NotoSansCJK-Regular.ttf`  (~16 MB)
   - `NotoSansCJK-Bold.ttf`     (~16 MB)

3. Edit `src/ui/register_helper.cpp` (or the new place where the
   borealis font system is initialised) to load them:

   ```cpp
   brls::FontLoader::loadFont("font/NotoSansCJK-Regular.ttf", "regular", 16);
   brls::FontLoader::loadFont("font/NotoSansCJK-Bold.ttf",    "bold",    16);
   ```

   The Switch has the system font available via `plInitialize` (see
   `library/borealis/library/lib/platforms/switch/switch_font.cpp`),
   which is what wiliwili uses; we follow that path so we don't need
   to ship 32 MB of fonts on the SD card.

The placeholders below exist so the resources/ tree is tracked even
when the font files are not yet present.
