# ani-switch 复用方案

> 历史设计参考，不是已验证的实现状态。当前进度见 [DEVELOPMENT_STATUS.md](DEVELOPMENT_STATUS.md)。本文部分 API、视频源和直接复用假设尚未成立；重命名 namespace 不会消除开源许可证义务；NRO 打包不需要 prod.keys。

> 目标:把 open-ani/animeko 的"在 Switch 上看番"需求,基于 xfangfang/wiliwili 已有的 Switch homebrew
> 基建,做最小代价的二次开发。本文档基于对 wiliwili `dev` 分支 2026-09-02 拉取的源码
> (commit `5f08b286` borealis `v2.1.0-64-g5f08b286`) 真实读码分析,不是凭 README 推测。

---

## 1. 路由判定

按 source-recompile-porting skill 的方法论,本项目**不是 source recompile**(Compose 不能在 Switch
上跑),但属于 **partial source + clean-room reimplementation** 的混合体:

- 业务层(API 客户端、数据模型、UI 业务部分)→ 全新写,基于 Bangumi/dandanplay 协议
- 平台层、视频解码、UI 库、HTTP/DNS、工具链 → **从 wiliwili 直接 fork**,不重写

最终交付的 `ani-switch` 仓库会同时引入 wiliwili 的 `library/borealis`、`library/cpr`、
`library/lunasvg`、`library/OpenCC`、`library/QR-Code-generator`、`library/pystring`、
`library/mongoose` 作为子模块,以及 wiliwili 的 `wiliwili/source/view/mpv_core.{hpp,cpp}`、
`wiliwili/source/view/danmaku_core.{hpp,cpp}`、`wiliwili/source/utils/*` 等通用模块作为 vendored
代码(直接复制 + 重命名 namespace,避免 GPL 强传染到我们的命名空间)。

> **法律说明**:wiliwili 是 GPL-3.0,Animeko 是 AGPL-3.0,两者兼容。我们的目标也是
> AGPL-3.0。但 vendored 文件的 namespace 会从 `wiliwili::` 改成 `aniswitch::`,模块注释会标
> 明 "Adapted from wiliwili (GPL-3.0)"。任何 GitHub release 必须同步公开源代码(AGPL 要求)。

---

## 2. wiliwili 真实模块清单(代码级核对)

| wiliwili 模块 | 文件 | 行数 | 性质 | 我们的复用度 |
|---|---|---:|---|---|
| Switch wrapper (C entry) | `library/borealis/library/lib/platforms/switch/switch_wrapper.c` | 69 | libnx 初始化 | **100% 复用** (随 borealis 引入) |
| Switch platform impl | `library/borealis/library/lib/platforms/switch/switch_platform.cpp` | 262 | 平台抽象(电池/IP/语言/睡眠) | **100% 复用** |
| Switch video (deko3d) | `library/borealis/library/lib/platforms/switch/switch_video.cpp` | 249 | deko3d 设备/帧缓冲/nanovg | **100% 复用** |
| Switch input | `library/borealis/library/lib/platforms/switch/switch_input.cpp` | 560 | 手柄/触屏/键盘 | **100% 复用** |
| Switch audio / font / ime | `library/borealis/library/lib/platforms/switch/switch_*.cpp` | 92-142 | 音频/字体/输入法 | **100% 复用** |
| borealis UI 库 | `library/borealis/` (~2.1.0) | ~30k+ | 完整 UI 库 | **100% 复用** |
| cpr (libcurl C++ 包装) | `library/cpr/` | ~10k+ | HTTP 客户端 | **100% 复用** |
| lunasvg | `library/lunasvg/` | ~5k+ | SVG 渲染 | **100% 复用** |
| OpenCC | `library/OpenCC/` | ~3k+ | 简繁转换 | **100% 复用** |
| QR-Code-generator | `library/QR-Code-generator/` | ~1k | QR 码 | **100% 复用** |
| pystring | `library/pystring/` | ~1k | Python 风格 string | **100% 复用** |
| mongoose | `library/mongoose/` | ~5k+ | 嵌入式 HTTP/WebSocket/DNS | **100% 复用** |
| libpdr | `library/libpdr/` | header-only | Switch system getter | **100% 复用** |
| `wiliwili/source/main.cpp` | 入口 | 108 | 启动 + Intent 分发 | **80% 复用**(改 Intent) |
| `view/mpv_core.{hpp,cpp}` | MPV 集成 | ~700 | 视频播放(deko3d/OpenGL/D3D11) | **100% 复用**(接 URL 即可) |
| `view/danmaku_core.{hpp,cpp}` | 弹幕引擎 | ~900 | 弹幕渲染/防遮挡/速度/字号 | **95% 复用**(改数据格式) |
| `view/subtitle_core.{hpp,cpp}` | 字幕引擎 | ~200 | ASS 字幕 | **100% 复用** |
| `view/recycling_grid.{hpp,cpp}` | 列表 view | ~300 | 回收式 grid | **100% 复用** |
| `view/auto_tab_frame.{hpp,cpp}` | Tab 框架 | ~150 | 多 tab 切换 | **100% 复用** |
| `view/svg_image.{hpp,cpp}` | SVG view | ~150 | SVG 渲染 view | **100% 复用** |
| `view/text_box.{hpp,cpp}` | 文本 view | ~80 | 文本+样式 | **100% 复用** |
| `view/hint_label.{hpp,cpp}` | 提示 label | ~50 | 居中提示 | **100% 复用** |
| `view/button_close.{hpp,cpp}` | 关闭按钮 | ~40 | | **100% 复用** |
| `view/check_box.{hpp,cpp}` | 复选框 | ~50 | | **100% 复用** |
| `view/custom_button.{hpp,cpp}` | 自定义按钮 | ~100 | | **100% 复用** |
| `view/grid_dropdown.{hpp,cpp}` | 下拉选择 | ~150 | | **100% 复用** |
| `view/video_progress_slider.{hpp,cpp}` | 进度条 | ~150 | | **100% 复用** |
| `utils/config_helper.{hpp,cpp}` | 设置持久化 | ~700 | nlohmann::json 配置 | **90% 复用**(删 B 站相关) |
| `utils/dns_helper.{hpp,cpp}` | DNS resolver | ~200 | 基于 mongoose 的 UDP DNS | **100% 复用** |
| `utils/gesture_helper.{hpp,cpp}` | 手势识别 | ~200 | 触屏手势 | **100% 复用** |
| `utils/event_helper.{hpp,cpp}` | 事件总线 | ~50 | brls::Event 包装 | **100% 复用** |
| `utils/activity_helper.{hpp,cpp}` | Intent 系统 | ~150 | `Intent::openXxx()` 路由 | **100% 复用** |
| `utils/register_helper.{hpp,cpp}` | 注册自定义 view | ~30 | brls 注册 | **100% 复用** |
| `utils/shader_helper.{hpp,cpp}` | shader 工具 | ~100 | | **100% 复用** |
| `utils/string_helper.{hpp,cpp}` | string 工具 | ~50 | | **100% 复用** |
| `utils/number_helper.{hpp,cpp}` | 数字格式化 | ~30 | | **100% 复用** |
| `utils/thread_helper.{hpp,cpp}` | 线程池 | ~80 | | **100% 复用** |
| `utils/crash_helper.{hpp,cpp}` | 崩溃处理 | ~80 | | **100% 复用** |
| `utils/dialog_helper.{hpp,cpp}` | 对话框 | ~100 | | **100% 复用** |
| `utils/shortcut_helper.{hpp,cpp}` | 快捷键 | ~50 | | **100% 复用** |
| `utils/image_helper.{hpp,cpp}` | 图片加载 | ~200 | lunasvg/Image 加载 | **100% 复用** |
| `utils/vibration_helper.{hpp,cpp}` | 手柄震动 | ~30 | | **100% 复用** |
| `utils/version_helper.{hpp,cpp}` | 版本检测 | ~50 | | **90% 复用** |
| `api/bilibili/util/http.{hpp,cpp}` | HTTP wrapper | ~270 | cpr + JSON 解析 | **95% 复用**(去 WBI 签名) |
| `api/bilibili/util/json.hpp` | JSON helper | ~20 | nlohmann 宏 | **100% 复用** |
| `api/bilibili/util/md5.hpp` | MD5 | ~30 | | **100% 复用** |
| `api/bilibili/util/wbi.{hpp,cpp}` | WBI 签名 | ~150 | **B 站专属** | **0% 复用** |
| `api/bilibili/util/uuid.hpp` | UUID | ~30 | | **100% 复用** |
| `api/bilibili.h` | API facade | ~600+ | **B 站专属** | **0% 复用** |
| `api/bilibili/result/*` | B 站数据类型 | ~30+ 文件 | **B 站专属** | **0% 复用** |
| `api/bilibili.cpp` | B 站 API 实现 | ~3000+ | **B 站专属** | **0% 复用** |
| `api/home_api.cpp` 等 | B 站各 endpoint | ~2000+ | **B 站专属** | **0% 复用** |
| `api/live/*` | 直播 | ~500 | **B 站专属** | **0% 复用** |
| `api/dlna/*` | DLNA | ~300 | 不需要 | **0% 复用** |
| `activity/*` | 12 个 activity | ~5000+ | B 站业务 | **0-20% 复用** (UI 框架可借鉴,逻辑全重写) |
| `fragment/*` | 50+ fragment | ~5000+ | B 站业务 | **0-20% 复用** |
| `presenter/*` | MVVM presenter | ~3000+ | B 站业务 | **0-20% 复用** (模式可借鉴) |
| `view/video_card.{hpp,cpp}` | 视频卡片 | ~200 | 通用 | **80% 复用** (改数据绑定) |
| `view/video_view.{hpp,cpp}` | 视频 view | ~200 | MPV 容器 | **100% 复用** |
| `view/video_profile.{hpp,cpp}` | 视频详情 | ~200 | | **80% 复用** |
| `view/video_comment.{hpp,cpp}` | 评论 view | ~200 | | **80% 复用** |
| `view/hots_card.{hpp,cpp}` | 热门卡片 | ~100 | | **100% 复用**(只换数据) |
| `view/dynamic_video_card.{hpp,cpp}` | 动态视频 | ~100 | 不需要(没"动态"概念) | **0%** |
| `view/auto_tab_frame.{hpp,cpp}` | tab 框架 | ~150 | | **100% 复用** |
| `view/live_core.{hpp,cpp}` | 直播 | ~200 | 不需要 | **0%** |
| `view/gallery_view.{hpp,cpp}` | gallery | ~150 | 通用 | **100% 复用** |
| `view/grid_dropdown.{hpp,cpp}` | 下拉 | ~150 | | **100% 复用** |
| `view/animation_image.{hpp,cpp}` | 动画图 | ~80 | 通用 | **100% 复用** |
| `view/up_user_small.{hpp,cpp}` | UP 主卡片 | ~80 | 不需要(对应 Bangumi "制作人员") | **0%** |
| `view/user_info.{hpp,cpp}` | 用户信息 | ~150 | Bangumi 化 | **50% 复用** |
| `view/inbox_msg_card.{hpp,cpp}` | 消息 | ~80 | 不需要 | **0%** |
| `view/qr_image.{hpp,cpp}` | QR view | ~50 | 通用 | **100% 复用** |
| `view/live_danmaku_item.{hpp,cpp}` | 直播弹幕 | ~80 | 不需要 | **0%** |
| `view/dynamic_article.{hpp,cpp}` | 动态图文 | ~80 | 不需要 | **0%** |
| `view/button_refresh.{hpp,cpp}` | 刷新按钮 | ~30 | 通用 | **100% 复用** |
| `view/svg_image.{hpp,cpp}` | SVG view | ~150 | 通用 | **100% 复用** |
| `view/video_snapshot_core.{hpp,cpp}` | 视频快照 | ~150 | 通用 | **100% 复用** |

**汇总估算**:
- C++ 源文件总数:wiliwili ~150 个文件,~60k 行
- **完全可复用(100%)**:约 70 个文件,~25k 行 (主要是 `library/*` 和 `utils/*`)
- **大部分可复用(50-95%)**:约 15 个文件,~3k 行
- **完全不复用(0%)**:约 65 个文件,~32k 行 (主要是 B 站 API 业务层)
- **净复用率**:约 **47%** 代码 + **70%** 关键平台基建

---

## 3. 关键复用模块深度分析

### 3.1 MPV 集成 (`view/mpv_core.hpp/cpp`)

**复用度:100%**

`MPVCore` 是一个 libmpv 的 C++ singleton wrapper,内部已抽象 5 个后端:

```cpp
// 编译期根据宏选择渲染后端
#if defined(MPV_SW_RENDER)        // CPU 软件渲染
#elif defined(BOREALIS_USE_DEKO3D) // Switch 原生 (deko3d) + Tegra NVDEC
#include <mpv/render_dk3d.h>
#elif defined(BOREALIS_USE_D3D11)  // Win/UWP
#elif defined(BOREALIS_USE_GXM)    // PSVita
#elif defined(BOREALIS_USE_OPENGL) // OpenGL (Switch 走这个)
```

Switch 走 deko3d + libmpv_deko3d 路径,内部用 `mpv_deko3d_fbo` 桥接,`DkFence`
做 GPU 同步。averne 编译的 `switch-libmpv_deko3d-0.36.0-2` 自带 NVDEC 硬解,
**真机已确认 4K@60 跑通**。

对外接口完全是 URL-driven:
```cpp
MPVCore::instance().setUrl("https://...", "referrer=https://...");
MPVCore::instance().play();
```

**对我们的意义**:写完 Bangumi API 拿到视频 URL 后,一行 `setUrl()` 就播了,
不需要碰任何 ffmpeg/mpv/deko3d 的代码。这是整个项目最重要的复用项。

### 3.2 弹幕引擎 (`view/danmaku_core.hpp/cpp`)

**复用度:95%**

`DanmakuCore` 接收 `vector<DanmakuItem>`,内部实现:
- 4 种字体样式(stroke / incline / shadow / pure)
- 滚动/顶部/底部/高级(BAS) 4 种模式
- 防遮挡 mask(SVG 渲染,1/30s 一帧,10s 一个分片)
- 可配置字号/速度/区域/透明度/行高
- 智能 mask 开关
- 同步/异步加载
- 速度联动(视频倍速)

唯一 B 站专属的是输入数据格式 `DanmakuItem` 构造函数:
```cpp
DanmakuItem(std::string content, const char *attributes);
```
其中 `attributes` 是 B 站 XML 格式。

**我们的改造**:写一个 `dandanplay_to_danmaku(xml_or_json)` 转换器,转出来
`vector<DanmakuItem>` 喂给 `DanmakuCore::loadDanmakuData()` 即可,核心渲染逻辑
一行不改。

### 3.3 HTTP wrapper (`api/bilibili/util/http.hpp/cpp`)

**复用度:95%**

`HTTP` 类是基于 cpr + libcurl 的 wrapper,核心方法:

```cpp
// 完全通用,可以原样复用
static void _cpr_get(url, params, callback, error);
static void _cpr_post(url, params, payload, callback, error);

// 模板方法,自动 JSON 解析
template<typename T> static void getResultAsync<T>(url, params, callback, error);
template<typename T> static void postResultAsync<T>(url, params, payload, callback, error);
```

`parseJson<T>()` 假设返回 `{code: 0, data: {...}, message: "..."}` 结构。
Bangumi 公开 API 返回结构是 `{request:/.../, code:0, message:"ok", result:{...}}`,
把 `parseJson` 的 `res.contains("data")` 改成 `res.contains("result")` 即可。
**或者**写个 Bangumi 专用 `parseBangumiJson<T>()` 替代品。

唯一 B 站专属的是 `signParameters` (APP_KEY/SECRET WBI 签名),直接删掉。

**Headers** 也需要改:
```cpp
// 原:B 站专属
static inline cpr::Header HEADERS = {
    {"User-Agent", "wiliwili"},
    {"Referer", "https://www.bilibili.com/client"},
    {"Origin", "https://www.bilibili.com"},
};
// 改:Animeko 风格
static inline cpr::Header HEADERS = {
    {"User-Agent", "ani-switch/0.1.0"},
    {"Accept", "application/json"},
};
```

### 3.4 Switch platform (`library/borealis/library/lib/platforms/switch/*`)

**复用度:100% (随 borealis 子模块)**

borealis 仓库里 `library/lib/platforms/switch/` 完整实现了:
- `switch_wrapper.c` (69 行) — `userAppInit()` / `userAppExit()`,包括 socket 初始化
- `switch_platform.cpp` (262 行) — 平台服务(电池、IP、DNS、locale、theme)
- `switch_video.cpp` (249 行) — deko3d 设备初始化 + 帧缓冲 + nanovg 绑定
- `switch_input.cpp` (560 行) — 手柄/触屏/键盘
- `switch_audio.cpp` (142 行) — audin 音频输出
- `switch_font.cpp` (92 行) — 系统字体加载
- `switch_ime.cpp` (102 行) — 系统输入法

**我们对 Switch 平台层完全不需要写代码**。我们的 `main.cpp` 跟 wiliwili 一样:
```cpp
brls::Application::init();   // 内部已经处理了 Switch 初始化
brls::Application::createWindow("ani-switch");
brls::Application::mainLoop();
```

### 3.5 build 工具链

wiliwili 用 **devkitpro/devkita64**(跟 AC-SWITCH 同一个):
```bash
docker run --rm -v $(pwd):/data devkitpro/devkita64:20251117 \
  bash -c "/data/scripts/build_switch.sh"
```

我们直接复用 wiliwili 的 `scripts/build_switch.sh` + 它的 CMakeLists 模板。
需要预装的额外包:
- `switch-ffmpeg-7.1-1-any.pkg.tar.zst` (averne build)
- `switch-libmpv-0.36.0-3-any.pkg.tar.zst` (deko3d 路径用 `-deko3d` 后缀)
- `libuam-f8c9eef01ffe06334d530393d636d69e2b52744b-1-any.pkg.tar.zst` (deko3d only)
- `switch-nspmini-48d4fc2-1-any.pkg.tar.xz`
- `hacBrewPack-3.05-1-any.pkg.tar.zst`

全部从 `https://github.com/xfangfang/wiliwili/releases/download/v0.1.0/` 下载。

---

## 4. 不能复用的部分(需要新写)

### 4.1 Bangumi API 客户端

**目标接口**(open.bgm.tv):
- `GET /v0/subjects/{id}` — 番剧详情
- `GET /v0/subjects/{id}/episodes` — 剧集列表
- `GET /v0/episodes/{id}` — 单集详情(包含视频源信息)
- `GET /v0/search/subjects?keyword=...` — 搜索
- `GET /v0/users/{username}/collections` — 追番列表
- `POST /v0/users/{username}/collections` — 更新追番状态
- OAuth: `https://bgm.tv/oauth/authorize`

文件命名:
```
net/
├── bgm_client.hpp          # 类,BangumiClient::getXxx()
├── bgm_client.cpp
├── bgm_auth.hpp            # OAuth 流程(扫码/refresh token)
├── bgm_auth.cpp
├── bgm_types.hpp           # Subject, Episode, UserCollection 等
└── parse_bangumi.hpp       # JSON 解析
```

### 4.2 dandanplay / myani.org 弹幕客户端

**dandanplay**:
- `GET /api/v2/match` — 用文件 hash 匹配番剧
- `GET /api/v2/comment/{episodeId}?withRelated=true&chConvert=0` — 弹幕 xml
- `GET /api/v2/bangumi/{animeId}` — 番剧匹配

**myani.org** (Animeko 自家):
- WebSocket `wss://danmaku-cn.myani.org:9501`
- 协议类似 B 站直播弹幕 WS

文件命名:
```
net/
├── dandanplay_client.hpp
├── dandanplay_client.cpp
├── danmaku_ws.hpp          # myani.org WebSocket 客户端
├── danmaku_ws.cpp
└── danmaku_parser.hpp      # dandanplay XML -> DanmakuItem 转换
```

### 4.3 视频源抽象(对应 Animeko 的 `ani-subs` 数据源)

Animeko 通过 `creamycake-anime/ani-subs` 做数据源聚合。我们做一个简化版:
- HTTP 源(优先):dandanplay 直接返回视频 URL
- 缓存源:本地 SD 卡已下载文件
- BT 源:**Phase 4 才做**,不阻塞 P0

文件命名:
```
core/
├── source_manager.hpp      # 源选择策略
├── source_manager.cpp
├── http_source.hpp
├── http_source.cpp
├── cache_source.hpp        # 本地缓存管理
└── cache_source.cpp
```

### 4.4 持久化

wiliwili 用 `nlohmann::json` 存一个 `setting` 对象。够轻量,但 Bangumi 客户端
的"追番进度"等数据更结构化,需要 SQLite。

**方案**:沿用 wiliwili 的 `nlohmann::json` 做用户设置(主题/音量/弹幕样式),
加一个 `SQLite` (Switch 上有现成的)做追番列表/历史记录/收藏。

文件命名:
```
utils/
├── kv_store.hpp            # nlohmann::json 配置(从 wiliwili 复用)
├── sqlite_store.hpp        # SQLite 包装
└── sqlite_store.cpp
```

### 4.5 业务 UI(activity / fragment / presenter)

**整体借鉴 wiliwili 的 MVVM 模式**:
- `activity/` = 顶层屏(主页、详情、播放、设置)
- `fragment/` = 子屏(番剧列表、剧集列表、评论)
- `presenter/` = 业务逻辑 + API 调用 + 状态管理
- `view/` = 自定义控件

但 **content 全是 Bangumi 化的**,不复用 wiliwili 的 activity/fragment/presenter 代码。

需要的新增文件(估计):
```
ui/activity/
├── main_activity.hpp        # 主屏(替代 B 站首页)
├── subject_activity.hpp     # 番剧详情
├── episode_activity.hpp     # 单集
├── player_activity.hpp      # 播放器(基于 BasePlayerActivity 模式)
├── search_activity.hpp
├── my_collection_activity.hpp  # 我的追番
├── history_activity.hpp
└── setting_activity.hpp

ui/fragment/
├── home_recommend.hpp       # 番剧推荐
├── home_bangumi.hpp         # 每日放送
├── home_rank.hpp            # Bangumi 排行榜
├── episode_list.hpp
├── subject_info.hpp
└── (其他需要的子屏)

ui/presenter/
├── presenter.hpp            # 基础类
├── home_recommend_presenter.hpp
├── subject_detail_presenter.hpp
├── episode_presenter.hpp
├── search_presenter.hpp
└── ...
```

### 4.6 资源文件

- 字体:Noto Sans CJK(覆盖简繁日韩) — wiliwili 用系统字体,我们要打包
- 图标:自己设计,128x128 NRO icon
- XML 布局:borealis 的 `CONTENT_FROM_XML_RES` 机制
- i18n:简繁日韩英 5 种语言字符串(用 OpenCC 做简繁转换)

---

## 5. ani-switch 仓库结构

```
E:\AI\ani-switch\
├── CMakeLists.txt              # 顶层(借鉴 wiliwili,但改成 ani-switch)
├── AGENTS.md                   # 给 AI agent 的项目说明
├── README.md
├── LICENSE                     # AGPL-3.0
├── .gitmodules                 # 指向 wiliwili 的子模块
├── .gitignore
│
├── platform/                   # Switch 平台层(实际只有一行 CMake 引用)
│   └── switch/
│       ├── CMakeLists.txt      # 引入 devkitpro toolchain + libnx
│       └── link.ld             # NRO link 脚本(从 AC-SWITCH 借)
│
├── core/                       # 业务核心(自写)
│   ├── source_manager.{hpp,cpp}
│   ├── http_source.{hpp,cpp}
│   ├── cache_source.{hpp,cpp}
│   └── episode_resolver.{hpp,cpp}
│
├── player/                     # 视频播放(100% 复用 wiliwili mpv_core)
│   ├── mpv_core.{hpp,cpp}      # 直接从 wiliwili 复制,改 namespace
│   ├── video_player.{hpp,cpp}  # 自写,薄包装调用 MPVCore
│   └── danmaku_renderer.{hpp,cpp}  # 自写,转 dandanplay 数据给 DanmakuCore
│
├── danmaku/                    # 弹幕(95% 复用)
│   ├── danmaku_core.{hpp,cpp}  # 从 wiliwili 复制
│   └── danmaku_types.hpp       # 从 wiliwili 复制
│
├── net/                        # 网络(95% 复用 + 新增)
│   ├── http.{hpp,cpp}          # 从 wiliwili 复制(改 namespace + 改 HEADERS)
│   ├── json_helper.hpp         # 从 wiliwili 复制
│   ├── md5.hpp                 # 从 wiliwili 复制
│   ├── bgm_client.{hpp,cpp}    # 新写
│   ├── bgm_auth.{hpp,cpp}      # 新写
│   ├── bgm_types.hpp           # 新写
│   ├── dandanplay_client.{hpp,cpp}  # 新写
│   ├── danmaku_ws.{hpp,cpp}    # 新写(myani.org WebSocket)
│   └── danmaku_parser.hpp      # 新写(dandanplay XML -> DanmakuItem)
│
├── ui/                         # UI(框架 100% 复用,业务 0% 复用)
│   ├── activity/               # 自写,但用 brls::Activity
│   ├── fragment/               # 自写,但用 brls::Box 等
│   ├── presenter/              # 自写,但用 presenter.hpp 模式
│   └── view/                   # 部分复用 wiliwili view/*(改 namespace)
│
├── utils/                      # 工具(90% 复用)
│   ├── config_helper.{hpp,cpp} # 从 wiliwili 复制(去 B 站)
│   ├── dns_helper.{hpp,cpp}   # 从 wiliwili 复制
│   ├── event_helper.hpp        # 从 wiliwili 复制
│   ├── gesture_helper.{hpp,cpp}  # 从 wiliwili 复制
│   ├── activity_helper.hpp     # 从 wiliwili 复制(Intent)
│   ├── register_helper.{hpp,cpp}  # 从 wiliwili 复制
│   ├── shader_helper.{hpp,cpp}
│   ├── string_helper.hpp
│   ├── number_helper.hpp
│   ├── thread_helper.{hpp,cpp}
│   ├── crash_helper.{hpp,cpp}
│   ├── dialog_helper.hpp
│   ├── shortcut_helper.hpp
│   ├── image_helper.{hpp,cpp}
│   ├── vibration_helper.{hpp,cpp}
│   ├── version_helper.{hpp,cpp}  # 改 ani-switch 名字
│   ├── sqlite_store.{hpp,cpp}  # 新写
│   └── kv_store.hpp            # 从 config_helper 抽取
│
├── third_party/                # vendored 第三方
│   ├── borealis/               # = wiliwili/library/borealis (子模块)
│   ├── cpr/                    # = wiliwili/library/cpr (子模块)
│   ├── lunasvg/                # = wiliwili/library/lunasvg
│   ├── OpenCC/                 # = wiliwili/library/OpenCC
│   ├── QR-Code-generator/      # = wiliwili/library/QR-Code-generator
│   ├── pystring/               # = wiliwili/library/pystring
│   ├── mongoose/               # = wiliwili/library/mongoose
│   └── libpdr/                 # = wiliwili/library/libpdr
│
├── resources/                  # 资源
│   ├── icon/                   # NRO icon, PNG
│   ├── font/                   # Noto Sans CJK SC
│   ├── i18n/                   # zh_CN, zh_TW, ja, ko, en
│   ├── xml/                    # activity/fragment XML 布局
│   └── img/                    # 图片资源
│
├── scripts/                    # 构建脚本
│   ├── build_switch.sh         # 从 wiliwili 复制(改目标名)
│   ├── build_switch_deko3d.sh  # 从 wiliwili 复制
│   └── generate_dksh.sh        # deko3d shader 编译
│
├── docs/                       # 文档
│   ├── REUSE_PLAN.md           # 本文档
│   ├── ARCHITECTURE.md
│   ├── API_PROTOCOLS.md
│   └── SWITCH_BUILD.md
│
└── tests/                      # 单元测试
    ├── test_bgm_parse.cpp
    ├── test_dandanplay_parse.cpp
    ├── test_config.cpp
    └── ...
```

---

## 6. 实施路线(基于复用方案的更新)

### Phase 0: Spike (1-2 周,风险验证)

**不做新代码,只跑通 3 件验证**:

1. **下载 wiliwili deko3d nightly**:`wiliwili-NintendoSwitch-deko3d.zip` from
   `https://nightly.link/xfangfang/wiliwili/workflows/build.yaml/dev`
2. **SD 卡装 NRO**,在真机 Switch 上跑一段 B 站 1080p 视频,确认:
   - deko3d 路径不崩(否则需要切回 OpenGL)
   - 1080p 弹幕视频流畅
   - 网络连 B 站 OK(如果墙了,需要代理或换 dandanplay 测)
3. **记录真机表现**:frame time、卡顿点、温度

**如果你 dump 了 prod.keys**,可以直接装 wiliwili 的 NRO;否则在模拟器上测也行
(Citron 0x31790 仍会有 deko3d 兼容问题,这是已知风险)。

### Phase 1: 骨架 + 平台层 (1 周)

目标:在真机上跑一个空 ani-switch NRO,显示标题"ani-switch"。

具体:
1. `git init` + 写 `.gitmodules` 指向 wiliwili 9 个 third_party
2. 复制 wiliwili 的 `library/` 整目录到 `third_party/`,改 `.gitmodules`
3. 写顶层 `CMakeLists.txt`,借鉴 wiliwili 的 `cmake/build_switch.sh` 流程
4. 复制 wiliwili 的 `platform/switch/` 工具链(从 AC-SWITCH 也可借鉴)
5. 写 `main.cpp`,只调用 `brls::Application::init()` + 创建一个空 activity
6. 跑 `build_switch_deko3d.sh`,得到第一个 `ani-switch.nro`
7. SD 卡装机,验证能启动到主屏

**复用率:100%**(所有代码都来自 wiliwili 或 borealis)

### Phase 2: 业务层 (3-4 周)

- `net/bgm_client.{hpp,cpp}`:Bangumi 公开 API(用 `http::getResultAsync` 模板)
- `net/dandanplay_client.{hpp,cpp}`:dandanplay match + comment
- `net/danmaku_parser.hpp`:`dandanplay XML -> vector<DanmakuItem>`
- `core/source_manager.{hpp,cpp}`:HTTP 源选择(只接 HTTP 源,跳过 BT)
- `utils/config_helper.cpp` 改 Bangumi 化:删 B 站 cookie 项,加 Bangumi auth token
- 一个新 `activity/main_activity.hpp`:番剧推荐列表(grid + Bangumi API)
- 一个新 `activity/subject_activity.hpp`:番剧详情 + 剧集列表
- 一个新 `activity/player_activity.hpp`:套用 wiliwili `BasePlayerActivity` 模式,
  调 `MPVCore::setUrl(dandanplay_url)`,调 `DanmakuCore::loadDanmakuData(...)`

**复用率:80%**(MVP:能搜番 → 看番 → 看弹幕)

### Phase 3: Bangumi 登录 + 追番管理 (2-3 周)

- `net/bgm_auth.{hpp,cpp}`:OAuth 2.0 + Bangumi access_token
- `utils/sqlite_store.{hpp,cpp}`:SQLite 包装
- `core/collection.{hpp,cpp}`:追番列表增删改
- `core/history.{hpp,cpp}`:历史记录
- `activity/my_collection_activity.hpp`
- `activity/history_activity.hpp`

**复用率:75%**

### Phase 4: myani.org 弹幕 (1-2 周)

- `net/danmaku_ws.{hpp,cpp}`:WebSocket 客户端
- `player/danmaku_renderer.{hpp,cpp}`:实时弹幕叠加
- 弹幕发送(Bangumi OAuth token 鉴权)

**复用率:90%**

### Phase 5: BT(可选,3-4 周)

- libtorrent-rasterbar Switch 移植
- magnet 解析 + 边下边播协议对接
- 缓存策略

**复用率:50%**(libtorrent 移植是新工作)

### Phase 6: 收尾 (1-2 周)

- i18n 字符串
- 主题色(用 Animeko 的粉/紫主色,替换 wiliwili 的 B 站粉)
- 资源优化(NRO 体积、加载速度)
- 稳定性测试
- NSP forwarder

---

## 7. 风险与缓解(更新版)

| 风险 | 概率 | 影响 | 缓解 |
|---|---|---|---|
| Phase 0 deko3d 跑崩 | 30% | 阻塞 | 切 OpenGL(1080p@30 已足够) |
| libtorrent Switch 移植(若做) | 60% | +1 月 | 跳过,只做 HTTP 源(Phase 5 删) |
| dandanplay 速率限制 | 20% | 中 | 缓存 + 自家弹幕 server 兜底 |
| Bangumi API 变更 | 10% | 低 | 锁版本,自己 fork 协议 |
| Switch 16MB heap 限制(默认) | 已解决 | — | 沿用 AC-SWITCH 的 256MB linker flag |
| 真机 3D deko3d 兼容性(AC-SWITCH 已知问题) | 50% | 中 | OpenGL fallback 完整测试 |
| GPL 传染法律风险 | 0% | — | vendored 文件统一加 SPDX header,AGPL 兼容 |

---

## 8. 决策点(等你回复)

1. **仓库形态**:要 `E:\AI\ani-switch\` 跟 wiliwili 平级独立仓库(用 submodule 引用 wiliwili 的
   `library/`),还是要直接 `git remote add wiliwili` 改造为 wiliwili fork?
2. **Phase 0 时机**:要不要现在装 wiliwili deko3d nightly 真机跑一下,确认 1080p 视频流畅?
   (需要 prod.keys 装机)
3. **AC-SWITCH 优先级**:Phase 0 期间 AC-SWITCH 要不要暂停(避免双线分心)?
4. **MP4 vs HLS**:dandanplay 给的源大多是 HLS(m3u8),要测试 Switch 上 HLS 解码稳定性
5. **Bangumi auth**:你已有 Bangumi 账号吗?Phase 3 需要 Bangumi app id/secret 申请 OAuth

---

## 9. 下一步(等你确认后立刻可做)

- 选 1 → 选 5 后,我可以:
  - 写 `.gitmodules` + 顶层 `CMakeLists.txt`
  - 写 `platform/switch/CMakeLists.txt`(从 AC-SWITCH 借)
  - 复制 wiliwili 的 9 个 third_party 引用
  - 写 `main.cpp` + `activity/main_activity.hpp`(空壳,显示 ani-switch 标题)
  - 跑通第一个 NRO build(预计 30-60 分钟第一次 build)
  - 如果你给 prod.keys,装机跑一下

不写任何 Bangumi 业务代码,**只验证平台层复用方案**。
