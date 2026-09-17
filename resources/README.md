# Resources directory

- `i18n/`     — JSON string tables (zh-Hans, zh-Hant, en, ja, ko)
- `font/`     — CJK-capable TTF files (see `font/README.md`)
- `icon/`     — NRO icon JPEG (see `icon/README.md`)
- `xml/`      — borealis layout XML files (see `xml/activity/README.md`)
- `img/`      — Image assets (cover thumbnails, default avatar, ...)

The build script copies this directory verbatim into the romfs image
so the NRO can access resources via relative paths (see
`platform/switch/CMakeLists.txt`).
