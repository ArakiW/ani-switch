# Third-Party Licenses

ani-switch stands on the shoulders of giants.  All third-party
dependencies are vendored in `third_party/`.  This file lists them in
alphabetical order; the canonical license for each is the LICENSE
file shipped inside the dependency's directory.

| Library                  | Version / commit  | License           | Notes |
| ------------------------ | ------------------ | ----------------- | ----- |
| borealis (xfangfang)     | 5f08b286           | GPL-3.0           | UI framework + Switch platform layer (deko3d video, hid input, applet hook) |
| cpr                      | 1.10.x             | MIT               | C++ wrapper over libcurl |
| curl                     | via devkitpro      | MIT-like          | HTTP client used by cpr |
| fmt                      | 12.1.0 (vendored by borealis) | MIT | String formatting used throughout |
| ldifflib                 | via borealis       | BSD-3             | Diff utility (for shasum of test fixtures) |
| libmpv                   | via devkitpro      | GPL-3.0+          | mpv client API used for the player |
| libtorrent-rasterbar     | not linked         | BSD-3             | Considered for P3 BT; not active in v0.1 |
| lunasvg                  | via borealis       | MIT               | SVG rendering (used by the Borealis XML inflater) |
| mbedtls                  | via devkitpro      | Apache-2.0        | SHA-256, AES-CTR_DRBG (PKCE), Switch TLS |
| mongoose                 | 7.x                | MIT               | Embedded HTTP / WebSocket / DNS; used by the legacy danmaku_ws shim (now unused) |
| OpenCC                   | via borealis       | Apache-2.0        | Simplified ↔ Traditional Chinese conversion |
| pystring                 | via borealis       | BSD-3             | Python-style string utilities |
| QR-Code-generator        | via wiliwili       | MIT               | Login QR codes |
| tinyxml2                 | via borealis       | zlib              | XML parsing for the dandanplay danmaku format |
| wiliwili                 | reference          | GPL-3.0           | The 7 vendored source files under `third_party/` that are direct copies from xfangfang/wiliwili |

## Software derived from wiliwili

The following source files in `src/` are derivatives of files
from the wiliwili project (GPL-3.0, © 2024 xfangfang).  The original
project lives at <https://github.com/xfangfang/wiliwili>.  This
project keeps the same GPL-3.0 license and the same attribution:

  src/player/mpv_core.{hpp,cpp}        — wiliwili source/app/view/video_view.cpp
  src/player/danmaku_core.{hpp,cpp}    — wiliwili source/app/view/danmaku_view.cpp
  src/player/video_view.{hpp,cpp}      — wiliwili source/app/view/video_view.hpp
  src/player/video_progress_slider.{hpp,cpp} — wiliwili library
  src/player/recycling_grid.{hpp,cpp}  — wiliwili library
  src/utils/{activity,gesture,gesture_helper,image,number,register,string,version}_helper.{hpp,cpp} — wiliwili library
  src/main.cpp (entry point + libnx init) — derived from wiliwili source/main.cpp

All other source files in `src/` are original to ani-switch and
licensed under AGPL-3.0.

## Animeko reference

The dandanplay client in `src/net/dandanplay_client.{hpp,cpp}` mirrors
the protocol and the auth scheme used by the open-ani/animeko Kotlin
client at <https://github.com/open-ani/animeko/blob/main/danmaku/dandanplay/src/commonMain/kotlin/DandanplayClient.kt>.
The algorithm (X-AppId + X-Timestamp + X-Signature = Base64(SHA-256(appId
+ timestamp + path + appSecret))) is documented inline in
`dandanplay_auth.hpp`.

The myani client in `src/net/myani_client.{hpp,cpp}` is a clean-room
re-implementation of the open-ani/animeko AniDanmakuProvider /
AniDanmakuSender contract.  The wire shape (REST, not WebSocket) is
documented in `myani_client.hpp`.

The Bangumi OAuth flow in `src/net/bgm_auth.{hpp,cpp}` is a clean-room
re-implementation of the open-ani/animeko BangumiClient.
