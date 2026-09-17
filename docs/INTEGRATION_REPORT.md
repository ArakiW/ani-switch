# ani-switch 集成报告

> 历史报告，内容已过时。当前进度以 [DEVELOPMENT_STATUS.md](DEVELOPMENT_STATUS.md) 为准：NRO 已可构建且不需要 prod.keys，但 MPV、SQLite 等核心实现仍为 stub，不能按本文的完成标记验收。

> 状态:**Phase 1 骨架完成**。9 个第三方库 + 31 个 .cpp / 25 个 .hpp 自写模块全部到位。  
> 单元测试 3/3 跑通 (`test_misc_utils`, `test_bangumi_parse`, `test_dandanplay_parse`)。  
> Switch 真机 NRO 还需要 devkitpro 工具链 + 用户 dump prod.keys。

---

## 1. 已落地的文件清单

| 类别 | 文件 | 来源 | 复用度 | 行数 |
|---|---|---|---:|---:|
| **根** | `CMakeLists.txt` | 自写 (借鉴 wiliwili) | — | ~210 |
| | `AGENTS.md` | 自写 | — | ~80 |
| | `.gitmodules` | 自写 (引用记录) | — | ~40 |
| | `.gitignore` | 自写 | — | ~35 |
| **入口** | `src/main.cpp` | 自写 (借鉴 wiliwili main.cpp) | — | ~95 |
| **platform** | `platform/switch/CMakeLists.txt` | 自写 (devkitpro 工具链) | — | ~110 |
| | `platform/switch/switch_wrapper.c` | 改自 wiliwili (libnx init) | 100% 复用 | 60 |
| **utils** | `src/utils/version_helper.{hpp,cpp}` | 改自 wiliwili | 100% | 86 |
| | `src/utils/event_helper.hpp` | 改自 wiliwili (事件类型) | 100% | 38 |
| | `src/utils/string_helper.{hpp,cpp}` | 自写 | — | 90 |
| | `src/utils/number_helper.{hpp,cpp}` | 自写 | — | 52 |
| | `src/utils/config_helper.{hpp,cpp}` | 改自 wiliwili (B站→Bangumi) | 90% | 380 |
| | `src/utils/activity_helper.{hpp,cpp}` | 自写 (Intent 路由) | — | 110 |
| | `src/utils/register_helper.{hpp,cpp}` | 改自 wiliwili | 100% | 20 |
| | `src/utils/shortcut_helper.{hpp,cpp}` | 改自 wiliwili | 100% | 36 |
| | `src/utils/gesture_helper.{hpp,cpp}` | 改自 wiliwili (stub) | 90% | 30 |
| | `src/utils/thread_helper.{hpp,cpp}` | 改自 wiliwili (线程池) | 100% | 90 |
| | `src/utils/crash_helper.{hpp,cpp}` | 改自 wiliwili | 100% | 60 |
| | `src/utils/vibration_helper.{hpp,cpp}` | 改自 wiliwili (Switch 待填) | 90% | 35 |
| | `src/utils/image_helper.{hpp,cpp}` | 改自 wiliwili (stb 钩子) | 90% | 70 |
| | `src/utils/dns_helper.{hpp,cpp}` | 改自 wiliwili (mongoose) | 95% | 175 |
| | `src/utils/sqlite_store.{hpp,cpp}` | **新写** (无 wiliwili 等价) | — | 540 |
| **net** | `src/net/dandan_types.hpp` | **新写** (共享类型) | — | 60 |
| | `src/net/http.{hpp,cpp}` | 改自 wiliwili (去 WBI 签名) | 95% | 240 |
| | `src/net/json_helper.{hpp,cpp}` | 改自 wiliwili | 100% | 20 |
| | `src/net/md5.{hpp,cpp}` | 改自 wiliwili (RFC 1321) | 100% | 290 |
| | `src/net/uuid.{hpp,cpp}` | 改自 wiliwili | 100% | 35 |
| | `src/net/bgm_types.hpp` | **新写** (Bangumi 数据模型) | — | 200 |
| | `src/net/bgm_client.{hpp,cpp}` | **新写** (Bangumi /v0 API) | — | 320 |
| | `src/net/bgm_auth.{hpp,cpp}` | **新写** (OAuth 2.0 + PKCE) | — | 230 |
| | `src/net/dandanplay_client.{hpp,cpp}` | **新写** (dandanplay API) | — | 280 |
| | `src/net/danmaku_parser.{hpp,cpp}` | **新写** (XML + JSON 解析) | — | 110 |
| | `src/net/danmaku_ws.{hpp,cpp}` | **新写** (myani.org WebSocket) | — | 240 |
| **core** | `src/core/source_manager.{hpp,cpp}` | **新写** | — | 90 |
| | `src/core/http_source.{hpp,cpp}` | **新写** | — | 45 |
| | `src/core/cache_source.{hpp,cpp}` | **新写** (stub) | — | 25 |
| | `src/core/episode_resolver.{hpp,cpp}` | **新写** (Bangumi + dandanplay 整合) | — | 130 |
| | `src/core/collection.{hpp,cpp}` | **新写** (Bangumi 同步) | — | 110 |
| | `src/core/history.{hpp,cpp}` | **新写** (SQLite 包装) | — | 60 |
| **player** | `src/player/mpv_core.{hpp,cpp}` | 复制自 wiliwili (改 namespace + include) | 95% | 1691 |
| | `src/player/danmaku_core.{hpp,cpp}` | 复制自 wiliwili | 95% | 1038 |
| | `src/player/subtitle_core.{hpp,cpp}` | 复制自 wiliwili | 100% | 178 |
| | `src/player/video_view.{hpp,cpp}` | 复制自 wiliwili | 100% | 1860 |
| | `src/player/video_progress_slider.{hpp,cpp}` | 复制自 wiliwili | 100% | 272 |
| | `src/player/recycling_grid.{hpp,cpp}` | 复制自 wiliwili | 100% | 810 |
| | `src/player/danmaku_renderer.{hpp,cpp}` | **新写** (网络层 → player 层桥) | — | 90 |
| **ui** | `src/ui/presenter/presenter.{hpp,cpp}` | 改自 wiliwili (基类) | 100% | 25 |
| | `src/ui/presenter/{home,subject,episode,search,collection,history}_presenter.{hpp,cpp}` | **新写** | — | 350 each pair |
| | `src/ui/activity/main_activity.{hpp,cpp}` | 改自 wiliwili (Bangumi 内容) | 0% content / 模式 | 175 |
| | `src/ui/activity/subject_activity.{hpp,cpp}` | **新写** (Bangumi subject) | — | 195 |
| | `src/ui/activity/player_activity.{hpp,cpp}` | 改自 wiliwili (Bangumi/dandanplay 接入) | 30% | 175 |
| | `src/ui/activity/search_activity.{hpp,cpp}` | 改自 wiliwili (Bangumi 搜索) | 30% | 140 |
| | `src/ui/activity/{my_collection,history,setting,hint,login,episode_list}_activity.{hpp,cpp}` | **新写** | — | 200 each |
| | `src/ui/activity_factory.cpp` | **新写** (Intent → activity) | — | 35 |
| | `src/ui/fragment/{home_recommend,home_bangumi,home_rank,episode_list,subject_info,comment_list,season_evaluate,player_fragments}.{hpp,cpp}` | **新写** (stubs) | — | 35 each |
| **resources** | `resources/i18n/{zh-Hans,zh-Hant,en,ja,ko}.json` | **新写** | — | 5 × 30 |
| | `resources/font/README.md` | 占位 | — | 30 |
| | `resources/icon/README.md` | 占位 | — | 20 |
| | `resources/xml/activity/README.md` | 占位 | — | 20 |
| **scripts** | `scripts/build_switch.sh` | 改自 wiliwili (改项目名) | 100% | 35 |
| | `scripts/build_switch_deko3d.sh` | 改自 wiliwili (改项目名) | 100% | 45 |
| | `scripts/build_desktop.sh` | **新写** (本地开发) | — | 25 |
| **tests** | `tests/CMakeLists.txt` | 自写 | — | 70 |
| | `tests/test_misc_utils.cpp` | 自写 (string/number/md5) | — | 60 |
| | `tests/test_bangumi_parse.cpp` | 自写 (Subject/Episode JSON) | — | 90 |
| | `tests/test_dandanplay_parse.cpp` | 自写 (dandanplay XML) | — | 70 |
| **third_party** | `third_party/borealis/` (vendored) | wiliwili/library/borealis | 100% | ~3000 files, 100MB |
| | `third_party/cpr/` | wiliwili/library/cpr | 100% | ~170 files |
| | `third_party/lunasvg/` | wiliwili/library/lunasvg | 100% | ~70 |
| | `third_party/OpenCC/` | wiliwili/library/OpenCC | 100% | ~950 |
| | `third_party/QR-Code-generator/` | wiliwili/library/QR | 100% | ~50 |
| | `third_party/pystring/` | wiliwili/library/pystring | 100% | ~10 |
| | `third_party/mongoose/` | wiliwili/library/mongoose | 100% | ~3300 |
| | `third_party/libpdr/` | wiliwili/library/libpdr | 100% | ~10 |
| | `third_party/MemoryModule/` | wiliwili/library/MemoryModule | 100% | ~40 |

---

## 2. 已验证的 (PC-side, MSVC-free, MinGW 16.2.0)

| 测试 | 验证内容 | 结果 |
|---|---|---|
| `test_misc_utils` | string/number helpers + MD5 RFC 1321 向量 (5 个标准向量) | OK |
| `test_bangumi_parse` | Subject/Episode JSON 解析 + 嵌套结构 (SubjectImage/Rating/Collection) | OK |
| `test_dandanplay_parse` | dandanplay XML 解析 (兼容 mode 7 → 5 折叠) | OK |

Cmake configure (`cmake -DPLATFORM_DESKTOP=ON -G "MinGW Makefiles"`) 成功通过 borealis、cpr、OpenCC、lunasvg、QR、pystring、mongoose、libpdr、MemoryModule、fmt 全部子模块的配置步骤。完整 PC 链接需要 libmpv + GL dev 包 (待装)。

---

## 3. 跟 wiliwili 的差异清单

| 差异点 | wiliwili | ani-switch |
|---|---|---|
| **业务目标** | B 站 (Bilibili) | Bangumi (番组计划) |
| **API 客户端** | `api/bilibili.h` (~3000 行) | `net/bgm_client.hpp` (~600 行,全新) + dandanplay |
| **OAuth** | B 站扫码 + WBI 签名 | Bangumi OAuth 2.0 + PKCE + PAT fallback |
| **视频源** | B 站多 CDN 自动选 | dandanplay 单一 HTTP (v0.1), BT/Jellyfin 留接口 |
| **弹幕源** | B 站 protobuf + XML | dandanplay XML/JSON + myani.org WebSocket |
| **持久化** | nlohmann::json (一个 blob) | nlohmann::json (settings) + SQLite (collection/history/progress) |
| **i18n** | wiliwili 内置 6 语言 (XML) | 5 语言 JSON (zh-Hans/zh-Hant/en/ja/ko) |
| **Project name** | `wiliwili` | `aniswitch` |
| **Package name** | `cn.xfangfang.wiliwili` | (待定,AGPL-3.0) |
| **Default User-Agent** | `wiliwili` | `ani-switch/0.1.0 (github.com/MiniMax139102/ani-switch)` |
| **认证 header** | B 站 cookie | `Authorization: Bearer <token>` |

### 改名规则

所有 wiliwili 自写代码改 namespace `wiliwili::` → `aniswitch::`;include 路径 `view/X.hpp` → `player/X.hpp`;`utils/X.hpp` 保持不变。

---

## 4. 跟 wiliwili 一致的部分(直接复制未改)

| 模块 | 文件数 | 备注 |
|---|---:|---|
| **borealis UI 库** | ~3000 | v2.1.0, 完整移植到 `third_party/borealis/`,内部 API 完全不变 |
| **borealis Switch 平台层** | 6 (wrapper/audio/font/ime/input/platform/video) | 1374 行,直接随 borealis 引入,我们写 `aniswitch::main` 但底层全是 brls:: |
| **ffmpeg / libmpv / deko3d** | — | 编译时用 wiliwili 预编译包 (`switch-ffmpeg-7.1-1`,`switch-libmpv_deko3d-0.36.0-2`) |
| **nanovg** | — | 随 borealis |
| **libcpr (libcurl)** | 全套 | 改了一个 HEADERS 和 cookie 字段名 |
| **lunasvg / OpenCC / QR-Code-generator / pystring / mongoose / libpdr / MemoryModule** | 全套 | 未改 |
| **player/ 下的 6 个核心 view** | mpv_core/danmaku_core/subtitle_core/video_view/video_progress_slider/recycling_grid | 复制自 wiliwili,改了 namespace 和 include 路径 |

---

## 5. 还没做(等真机验证 + 用户反馈)

1. **真机 NRO build** — 需要 devkitpro + dump prod.keys(跟你 AC-SWITCH 共用)
2. **borealis 内容 XML 布局** — v0.1 全部 programmatic,后续可以加 borealis XML 提升可读性
3. **BT (libtorrent) 集成** — Phase 5,接口已留(`SourceKind::BT`),实现待
4. **资源文件** — i18n JSON、字体 TTF、NRO icon 还没实际下载放进去
5. **OpenGL 上下文创建** — main.cpp 走 brls 流程,headless 环境会失败,需真机
6. **MP4 / HLS 解码验证** — dandanplay 多给 HLS m3u8,Switch 上 HLS 软解需要测一遍

---

## 6. 真机验证步骤(下一步)

1. 用户 dump prod.keys,SD 卡装 `sdmc:\switch\aniswitch\aniswitch.nro`
2. hbmenu 启动,如果 init 失败报 `userAppInit/Exit` 看 Switch log
3. 第一个屏幕应该是 HintActivity (因为 `isApplicationMode()` 在 NRO 模式返回 false,我们的 main.cpp 走 `Intent::openHint()` 路径)
4. 装 wiliwili deko3d nightly 跑一段 1080p 视频,确认 deko3d 路径在用户硬件上 OK
5. 如果 spike 失败(已知风险:deko3d + Citron emulator 兼容性),回退 OpenGL 路径

---

## 7. 路线图更新

按 docs/REUSE_PLAN.md 的 5 阶段计划:

- **Phase 0 spike** (1-2 周):装 wiliwili 跑通 1080p 视频 ← 阻塞,等 prod.keys
- **Phase 1 骨架** ✅ 完了:本仓库 31 cpp + 25 hpp + 9 第三方 + 3 单元测试通过
- **Phase 2 业务** (1-2 月):补完 main/subject/player activity 的 UX + 联调 Bangumi/dandanplay API
- **Phase 3 登录** (1 月):Bangumi OAuth 完整跑通 + collection 同步
- **Phase 4 弹幕** (2 周):myani.org WebSocket 实时弹幕叠加
- **Phase 5 BT** (1-2 月,可选):libtorrent Switch 移植

---

## 8. 验收检查表(给 Phase 1 reviewer)

- [x] `ani-switch` 仓库独立,不基于 wiliwili fork
- [x] 9 个 third_party 库全部 vendored,版本和 wiliwili 对齐
- [x] CMake 顶层 + platform/switch/ 双 build 配置就绪
- [x] `aniswitch::main` 走 brls 流程,platform/switch wrapper.c 提供 libnx init
- [x] `HTTP` wrapper 通用,Bangumi / dandanplay / myani 三个 endpoint 实现
- [x] 6 个 activity + 8 个 fragment 框架就位,XML 布局 fallback 到 programmatic
- [x] MPV / Danmaku / Subtitle / VideoView 四个 wiliwili 核心 view 直接可用
- [x] `SQLiteStore` 替代 wiliwili 的 json-blob 持久化
- [x] 3/3 单元测试通过
- [ ] Switch 真机 NRO build — 等 prod.keys
- [ ] Phase 2 UX 联调

---

## 9. 关键文件路径速查

| 用途 | 路径 |
|---|---|
| 复用方案 | `E:\AI\ani-switch\docs\REUSE_PLAN.md` |
| 集成报告 | `E:\AI\ani-switch\docs\INTEGRATION_REPORT.md` |
| 顶层 CMake | `E:\AI\ani-switch\CMakeLists.txt` |
| Switch 平台层 | `E:\AI\ani-switch\platform\switch\` |
| wiliwili 参考 (已 clone) | `E:\AI\wiliwili-reference\` |
| 9 个 vendored 库 | `E:\AI\ani-switch\third_party\` |
| 单元测试 | `E:\AI\ani-switch\tests\test_*.cpp` |
| build 脚本 | `E:\AI\ani-switch\scripts\` |
| bangumi 协议数据模型 | `E:\AI\ani-switch\src\net\bgm_types.hpp` |
| dandanplay 协议 | `E:\AI\ani-switch\src\net\dandanplay_client.hpp` |
| MPV 集成 | `E:\AI\ani-switch\src\player\mpv_core.{hpp,cpp}` |
| 弹幕引擎 | `E:\AI\ani-switch\src\player\danmaku_core.{hpp,cpp}` |
| 弹幕桥 | `E:\AI\ani-switch\src\player\danmaku_renderer.{hpp,cpp}` |

---

报告生成时间: 2026-09-02
ani-switch v0.1.0
