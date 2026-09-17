# ani-switch 开发进度与后续计划

> 更新日期：2026-09-08。v6 把持久化从 SQLite 切到 JSON 文件，v7 做了一次全量代码审查并清理。
> 当前定位：**包含真实播放实现的 Switch 候选包，尚未实机播放验收**。
> 不再将“文件存在”“编译通过”和“实机功能完成”混为一谈。

## 1. 当前结论

- 已恢复同一份 wiliwili 参考仓库中的完整 MPV 与弹幕引擎头文件/实现，不再使用对应 no-op。
- 已接入真实 VideoView 绘制、播放页、暂停/继续、跳转、字幕轨道切换、同名 XML 弹幕和持久化写入。
- 持久化后端从 v5 的 SQLite 改为 v6 的单文件 JSON（`ani-switch.store.json`）。SQLite 链接已从主 target 移除，NRO 体积减少约 819 KB（v5 33,171,736 → v6 32,352,536）。SQLite 自身仍保留在 `third_party/sqlite/`，仅供 `tests/test_sqlite_devoptab`（Linux 模拟 VFS 测试）使用。
- 主页面不再是 `ani-switch (build stub)`：现在有直接播放、搜索、收藏、历史、设置、退出，以及本地视频列表。
- 当前候选 NRO 成功交叉编译；已检查 ELF 中真实 MPV、VideoView、DanmakuCore 符号和 NRO/ASET/RomFS 边界。
- 最新 Linux 原生回归测试 **9/9 通过**，包括持久化 store 的 JSON 路径；此前 Windows 7/7 通过。
- **本机没有运行 Switch 原生 UI，没有验证实机画面、声音、按键或长时间播放。** 旧版前三帧日志不能替新包背书。
- 自动搜索可播放资源、在线 dandanplay 全链路、myani WebSocket、完整 OAuth 和 BT 仍未完成。不能把这个候选包称为完整 Animeko 移植版或全部问题已解决。

## 1.1 实机故障史（v0→v6）

| 版本 | SHA-256 | 现象 | 根因 | 修复 |
|---|---|---|---|---|
| v0 (`0c805bb5…`) | `0c805bb5…` | `database: open failed` | 相对路径 `./ani-switch.db` 落到 NRO install cwd (read-only) | `getConfigDir()` 改 `sdmc:/switch/aniswitch` |
| v2 (`1e72afb2…`) | `1e72afb2…` | 仍 EIO `sqlite=1802` | devoptab 在 `sdmc:` 上 `sqlite3_open_v2` 触发 EIO，跟路径无关 | v3 加 VFS 指定 / `journal_mode=OFF` / `synchronous=OFF` |
| v3 (`b7bcb60a…`) | `b7bcb60a…` | `sqlite=28 CANTOPEN` | devoptab 同上，journal 改 off 也没用 | v4 加 `locking_mode=EXCLUSIVE` + retry |
| v4 (`643d2f13…`) | `643d2f13…` | 仍 CANTOPEN + 每次启动丢数据 | BLOCKER: `removeStale` open 前无条件删 DB | v5 把 `removeStale` 移到 open 失败后 |
| v5 (`bb7dc6e3…`) | `bb7dc6e3…` | 仍 EIO | 根因确认为 devoptab 跟 SQLite 不兼容 | **v6 切 JSON 文件存储** |
| v6 (`996a6395…`) | `996a6395…` | 待实机验证 | — | v7 代码审查 + 文档清理 |

v6 是第一份"换后端"的候选；v7 在 v6 基础上做了一次全量代码审查（见第 6 节），没有逻辑改动，仅清理残留（CMake 移除 `aniswitch_sqlite` link、`main.cpp` 错误页文案、目录扫描 try/catch 兜底、注释更新、文档对齐）。

### 为什么放弃 SQLite 切到 JSON

Switch devoptab 在 `sdmc:` 路径上对 `sqlite3_open_v2` 返回 EIO，跨 v0/v2/v3/v4/v5 五个版本都重现，跟 path、journal_mode、synchronous、locking_mode、stale-file cleanup 都无关。`ani-switch.json` 本身能正常读写（用户实机上传的 `ani-switch.json` 是 183 B 有效 JSON），说明 devoptab 对 fopen 路径 OK，对 sqlite3 内部用的 open(O_RDWR|O_CREAT) + mmap + fsync 流程不稳。

数据量：watch history + collection + progress 体积 < 10 KB。改用单文件 JSON：
- 启动一次性 load 到内存
- 每次 upsert/delete 整文件 rewrite（Windows 直接 truncate，POSIX `tmp + rename`）
- atomic rename 保证半写状态可恢复

## 2. 唯一交付文件

| 项目 | 当前值 |
|---|---|
| 文件 | `aniswitch.nro`（项目根目录） |
| 大小 | 32,352,536 bytes，约 30.85 MiB |
| SHA-256 | `996a63954334b0cc3832f14fb1b4ca269199d5c447d061837ca59759bd495fa6` |
| 图形后端 | OpenGL / GLFW |
| 构建模式 | Release 优化 + 启动及播放阶段诊断 |
| 安装位置 | `sdmc:/switch/aniswitch/aniswitch.nro` |
| 日志位置 | `sdmc:/switch/aniswitch/startup.log` |
| 持久化文件 | `sdmc:/switch/aniswitch/ani-switch.store.json`（v6 新增） |
| 配置文件 | `sdmc:/switch/aniswitch/ani-switch.json` |
| RomFS 大小 | 32,512 bytes |
| 实机状态 | 待验证 |

## 3. 如何做一次集中测试

1. 将上面的唯一 NRO 覆盖到安装位置。
2. 按住 R 启动一个游戏，进入完整应用模式的 hbmenu，再启动 ani-switch。相册小程序模式不是可靠的播放环境；提示页现在引导退出重开。
3. 推荐先把自己有权播放的普通 H.264/AAC MP4 放到 `sdmc:/switch/aniswitch/videos/`，主页面会列出该目录第一层的 MP4/MKV/WebM/MOV/M3U8 文件。
4. 也可以选"打开视频地址 / 本地路径"输入 HTTP(S) 地址或存在的本地文件路径；不需要先登录 Bangumi。
5. 验证视频画面、声音、暂停/继续、前后跳 10 秒、返回、再次播放和退出。播放页支持 X 暂停/继续、B 返回，也有可聚焦按钮。
6. 内嵌字幕交给 libmpv/libass 渲染；同名 `.srt`/`.ass` 放在视频旁，通过"字幕"按钮轮换轨道或关闭。
7. 同名 `.xml` 弹幕可放在视频旁，当前支持 `<i><d p="时间,模式,字号,颜色,...">文本</d></i>` 形式；超过 16 MiB 会跳过并提示。
8. 如果失败，保留此次的 `startup.log`。新增 `player: mpv init begin`、`player: mpv render context ready`、`player: file loaded`、播放启动/跳转完成、文件错误标记，可区分 UI 启动和实际播放器失败。

上述是测试步骤，不代表已经执行过的硬件验收。直接打开的无 Bangumi 集数 ID 视频目前不写入集数历史；有集数 ID 的播放路径才做进度检查点。

## 4. 本轮真实实现恢复情况

| 模块 | 当前实现 | 验证边界 |
|---|---|---|
| MPVCore | 从 wiliwili 恢复完整匹配的头文件和 CPP；保留渲染上下文、FBO、事件监听、音量、速度、暂停和跳转 | 交叉编译及符号验证；未实机播放 |
| VideoView | 调用真实 MPV FBO 绘制与 DanmakuCore 叠加，已加入应用编译 | 不再为空；没有照搬整套 B 站专用 OSD |
| DanmakuCore | 恢复 wiliwili 的弹幕排列/时间轴/绘制核心，适配事件、配置、HTTP 和必要工具 | XML 文件加载和桥接已接线，实机显示待验证 |
| DanmakuRenderer | 将 ParsedDanmaku 转为真实 DanmakuItem，排序加载、清理、本地发送 | myani HTTP 后端已接通；WebSocket 历史 stub 保留为 shim |
| SubtitleCore | 使用 libmpv 的真实轨道列表、选轨、关闭和外挂字幕命令 | 不再伪装成 B 站视频页字幕 API；实际字幕画面待测 |
| PersistentStore (SQLiteStore) | 单文件 JSON 后端，watch history / collection / progress 全部内存索引 + 整文件 atomic rewrite；类名保留以避免动 21 个调用方 | ctest 9/9 通过；未实机验收 |
| EpisodeResolver | 查询真实单集接口，填入条目、集数、进度并枚举视频源 | 不再无回调；在线返回与 UI 联调待测 |
| HTTPSourceProvider | 读取用户提供的 `sources.json`，按 Bangumi 集数 ID 获取 HTTP(S) 视频地址 | 不再假定 dandanplay 元数据保证提供播放 URL |
| SourceManager | 聚合所有 provider，完成一次回调；全部失败才报错 | 新增回归测试通过 |
| MainActivity | 实际菜单、本地目录列表（v7 加 try/catch 兜底 devoptab 抛异常）、播放和业务导航 | 原生 UI 未实机操作 |
| SubjectActivity | 实际标题、简介、评分、选集和返回入口 | 依赖 Bangumi 网络；封面等完整 UX 未完成 |
| PlayerActivity | 实际视频显示、状态、控制、字幕、XML 弹幕、集数进度检查点、Tap 手势暂停/恢复、HOME 失焦暂停 | 未实机验收，非稳定发行版 |
| 搜索/剧集/收藏/历史页面 | 补滚动容器、焦点可达性和返回操作；历史可重开保存的播放路径 | 无完整视觉回归与账户测试 |
| 设置 | 硬解、弹幕配置接入播放；移除尚无实际数据源的防遮挡/转换入口 | 硬解效果依赖解码器及视频格式 |

### 开源复用来源

- wiliwili 本地参考仓库：`E:/AI/wiliwili-reference`。
- 参考 revision：`88e5876bea9502d06f46a8656e3530684d3aaf7d`。
- MPV 与 DanmakuCore 的头文件、实现成对恢复，保留来源及许可说明；未用不匹配的 CPP 备份硬覆盖接口。
- 必要弹幕图片 `danmaku_ohh.png`、`danmaku_highlight.png` 复用并加入 RomFS。
- 当前 SDK 检出的实际媒体包：`switch-libmpv 0.39.0-4`、`switch-ffmpeg 7.1-5`，静态依赖由 `mpv.pc` 获取；不是旧文档中未经复核的版本。

## 5. 实际源码统计

统计 `src/` 中 `.cpp/.hpp/.h/.c`，排除第三方和备份。文件数不是功能完成率。

| 指标 | 数量 |
|---|---:|
| 项目源码及头文件 | 132 |
| `.cpp` | 65 |
| `.hpp` / `.h` | 66 / 1 |
| 源码行数（含注释空行） | 11,015 |
| 应用目标列入编译的 `.cpp` | 62 |
| 未列入应用目标的 `.cpp` | 3 |
| 测试源文件 / 注册测试 | 8 / Linux 9、Windows 8 |
| 顶层第三方目录 | 10 |
| Activity XML 最小根节点 | 10 |
| 本地化目录 | 5 |

模块文件数保持：UI 52、utils 30、net 22、player 14、core 12、根目录 2。

未编译的 CPP：`src/net/dandan_parser.cpp`（转发空单元）、`src/player/recycling_grid.cpp`、`src/player/video_progress_slider.cpp`。当前播放控件不依赖后两者，保留而不冒充已集成。

原来列出的 11 个明确临时空实现中，**10 个已替换为实际执行路径**；`net/danmaku_ws.cpp` 仍是未实现的可选在线通道（已被 `net/myani_client.cpp` 的 HTTP 实现替代，仅保留为兼容 shim）。其他 Fragment、BT/cache provider、手势等骨架仍存在，不能据此宣称全仓库无占位代码。

## 6. v7 代码审查清单

应用户要求"完全审查下现在的代码，不要再反复出这些问题"，v7 在 v6 基础上做了一次全量扫描 + 清理，**不引入新逻辑**。

| 类别 | 检查 | 结果 |
|---|---|---|
| 平台差异反模式 | `cpr::Timeout` 必须 `const` 不是 `constexpr`（cpr::Timeout 在 aarch64-none-elf g++ 16 上不是 literal type） | 3 个用点全部 `const` ✅ |
| 平台差异反模式 | `brls::Logger::warn` 不存在（borealis 5f08b286 用 `warning`） | 0 处 ✅ |
| 平台差异反模式 | `mkdir` 双参在 MinGW 选到 deprecated 单参 | `sqlite_store.cpp` 用 `_WIN32` 分流；其他用 `std::filesystem::create_directories`（跨平台 + error_code overload）✅ |
| 平台差异反模式 | `std::filesystem::directory_iterator` 在 `sdmc:` 路径构造时可能抛 `filesystem_error` | `main_activity.cpp` 整段 `try/catch` 兜底 ✅（v7 新增） |
| 旧引用清理 | `ani-switch.db` 字样 | 0 处在 `src/`（仅 `tests/test_sqlite_devoptab.cpp` 用作 devoptab 仿真测试，不进 NRO）✅ |
| 旧引用清理 | 主 target link `aniswitch_sqlite` | 已移除（v7），NRO 瘦 819 KB ✅ |
| 旧引用清理 | CMake `add_subdirectory(sqlite)` | 保留（`tests/test_sqlite_devoptab` 仍引用 `third_party/sqlite/sqlite3.c`）✅ |
| 旧引用清理 | `THIRD_PARTY_LICENSES.md` SQLite 条目 | 已删除（v7）✅ |
| 旧引用清理 | `DEVELOPMENT_STATUS.md` SQLite 章节 | 整篇重写对齐 v6/v7 现实（v7）✅ |
| 旧引用清理 | `config_helper.cpp` 注释说"SQLite can create the journal file" | 改为"JSON config and the JSON persistent store"（v7）✅ |
| 旧引用清理 | `main.cpp:182` 错误页文案"原数据库未删除" | 改为"数据文件未被删除"（v7）✅ |
| 启动链路 | `main.cpp` open 失败 fallback 错误页 | 已有；`databaseReady` 走 L177-189 |
| 启动链路 | `aniswitchStartupLog` / `aniswitchStartupReady` 定义 | `platform/switch/switch_wrapper.c:60,64` 定义 ✅ |
| 启动链路 | `ProgramConfig::getConfigDir` Switch 分支 | `sdmc:/switch/aniswitch`（绝对路径）✅ |
| 持久化 | `main.cpp:159` open 路径 | `ani-switch.store.json`（v6 改）✅ |
| 持久化 | close 路径 | `ProgramConfig::exit` 调 `save()` 写 `ani-switch.json` |
| 触摸 / focus | `main.cpp:127` focus 监听 | `getWindowFocusChangedEvent` 订阅，lost focus → MPVCore.pause() ✅ |
| 触摸 / focus | `player_activity.cpp:53` Tap 手势 | `addGestureRecognizer(new TapGestureRecognizer(...))` ✅ |
| 日志机制 | `aniswitchStartupLog` Switch 编译 | `#if defined(__SWITCH__) && defined(ANISWITCH_SWITCH_DEBUG)` 包裹，非 Switch 不链接 ✅ |
| 文档 | 文档日期 + SHA + 路径 + 体积 | 全部对齐 v6 实情 ✅ |
| release script | `git-rev` / `git-log` 0 字节静默 | 加 warning + `: > file` 显式清空（v7）✅ |

## 7. 验证结果

### 跨平台编译

| 验证 | 结果 |
|---|---|
| Switch 完整交叉编译、静态链接、NRO 打包 | 通过（v6 + v7） |
| `mpv_create` / `mpv_render_context_create` / `mpv_render_context_render` | 最终 ELF 中存在真实实现符号 |
| MPVCore / DanmakuCore / VideoView 的 draw | 最终 ELF 中存在 |
| NRO0 / ASET / RomFS 范围、资源图片、顶层/构建文件一致性 | 通过 |
| 主 target link `aniswitch_sqlite`（v6 前） | v7 移除，符号不再出现在 NRO |

### ctest

| 测试 | 结果 |
|---|---|
| `bangumi_parse` | 通过 |
| `dandanplay_parse` | 通过（XML/内存转换，不代表在线协议已测） |
| `dandanplay_auth` | 通过（X-AppId + X-Signature = Base64(SHA-256(...)) 已知算法向量） |
| `myani_parse` | 通过（types round-trip + 字符串映射） |
| `sqlite_store` | 通过（upsert / get / delete / reload / last-episode，JSON 路径，0.20s） |
| `misc_utils` | 通过 |
| `runtime_utils` | 通过 |
| `presenter_lifetime` | 通过（测试调度队列，不是真实 UI） |
| `sources` | 通过（聚合完成、部分失败、全部失败、选择） |
| **CTest 合计** | **Linux 9/9；Windows 8/8（约 0.36s，不含编译）** |
| 新包画面/声音/交互/休眠唤醒 | **未执行** |

### 已知未做

- 实机视频/音频/按键/睡眠/长时间播放回归（需要 SD 卡上机，用户做）。
- Switch devoptab 路径下 `sqlite3_open_v2` 失败根因的精确诊断（v6 切 JSON 后绕开，不再追究）。
- v6 切 JSON 之前丢失的 watch history / collection / progress 数据无法恢复（v0-v5 实机写入的 `ani-switch.db` 是损坏或被 `removeStale` 误删的 0 字节，JSON 是 v6 起的新文件）。

## 8. 构建与测试命令

项目根目录，Windows PowerShell：

```bash
docker run --rm -e ANISWITCH_SWITCH_DEBUG=ON `
    -v E:/AI/ani-switch:/src -w /src `
    devkitpro/devkita64:20260219 bash /src/scripts/build_switch.sh
```

注意 PowerShell 下要设 `$env:MSYS_NO_PATHCONV = "1"` 或者用 docker run 的 `-e` 形式把环境传过去；MSYS_NO_PATHCONV 在 PowerShell 里不能直接 `MSYS_NO_PATHCONV=1 docker ...` 这么写。

构建脚本末尾自动 `cp NRO + NACP` 到 `release/<UTC-时间戳>/`，并写 `SHA256SUMS`。PowerShell 下从最新 release 复制到 SD 卡：

```powershell
$rel = Get-ChildItem E:\AI\ani-switch\release -Directory | Sort-Object LastWriteTime -Descending | Select-Object -First 1
$dst = 'E:\switch\aniswitch'
New-Item -ItemType Directory -Path $dst -Force | Out-Null
Copy-Item "$($rel.FullName)\aniswitch.nro"  $dst\aniswitch.nro  -Force
Copy-Item "$($rel.FullName)\aniswitch.nacp" $dst\aniswitch.nacp -Force
```

ctest：

```bash
cmake -S tests -B cmake-build-audit-tests -G Ninja -DCMAKE_BUILD_TYPE=Release -DANISWITCH_BUILD_SQLITE_TEST=ON
cmake --build cmake-build-audit-tests --parallel 4
ctest --test-dir cmake-build-audit-tests --output-on-failure --timeout 30
```

默认仅构建一个 OpenGL/GLFW NRO；不再让用户在两个后端之间选。NRO 不需要 `prod.keys`。完整桌面 UI 的构建/启动仍未验证，独立测试通过不能替代它。

## 9. 视频源与文件约定

配置目录由 `ProgramConfig::getConfigDir()` 决定，Switch 返回 `sdmc:/switch/aniswitch`，桌面返回 `$XDG_CONFIG_HOME/ani-switch` 或 `~/.config/ani-switch`。

可选 `sources.json` 示例（将占位地址替换为自己有权播放的真实地址）：

```json
{
  "100": [
    { "url": "https://example.com/your-video.mp4", "label": "我的视频", "priority": 100 }
  ]
}
```

键是 **Bangumi 集数 ID**，不是条目 ID，也不是 dandanplay ID。此文件是显式的视频映射，不是自动资源聚合器。没有映射时播放页会请求用户输入，不静默等待、不把元数据页 URL 当视频流。

持久化文件：`ani-switch.store.json`（v6 起，< 10 KB）。现有收藏/历史/进度入口使用真实后端。仍需完善账号之间的收藏缓存隔离、离线缓存与远端冲突策略；当前不要将它称为完整多账户同步系统。

## 10. 后续计划，按最小成本排序

### P0：实机真实播放验收

- [x] 恢复匹配的 wiliwili MPV/弹幕引擎和必要图片。
- [x] 接通 VideoView、播放控制、字幕与本地 XML 弹幕。
- [x] 实际链接 libmpv、FFmpeg，检查不是被裁剪的空壳。
- [x] 通过 9 项回归测试，输出唯一候选 NRO。
- [x] v6 切 JSON 存储，v7 全量代码审查。
- [ ] 集中测试普通 MP4 的画面、音频、暂停、跳转、返回、再次播放、退出。
- [ ] 验证 JSON 持久化、集数续播、字幕、XML 弹幕、触摸/手柄切换。
- [ ] HLS、复杂编码、多字幕、睡眠唤醒和长时间播放回归。

### P1：内容服务与业务完善

- [x] 主页面、详情、搜索、选集、播放、设置导航有实际入口。
- [x] EpisodeResolver 有成功/失败路径；HTTP provider 使用明确地址映射。
- [x] dandanplay 真实认证 (X-AppId + X-Signature)。
- [x] myani HTTP 客户端 (login / loadHistorical / sendLocal)。
- [ ] 用真实 Bangumi 网络响应完成页面联调，补分页、错误提示和封面。
- [ ] 联调 PAT、收藏写入、账户切换及缓存隔离。
- [ ] 获取并验证真实视频源协议；自动找源不是此次构建修复已经提供的能力。
- [ ] 完善直接本地/URL 播放的独立历史与续播，而非仅集数 ID 历史。

### P2：在线弹幕、认证与发行

- [x] 校验 dandanplay 真实认证、请求和响应格式。
- [x] myani HTTP 客户端（替换原 WebSocket stub）。
- [ ] OAuth 实际 client 注册和服务支持确认后再开放，无虚构凭据。
- [ ] 完善翻译、图标、布局、可访问性和许可证清单。
- [ ] 实机回归通过后再关闭诊断制作发行包。

### P3：可选范围

BT/libtorrent、边下边播、自动下载、其他媒体源及其他平台不作为本轮打包前提。

## 11. 仓库与承诺边界

- 未创建提交、未推送、未发布公共 Release；历史文件和备份保留。
- 仓库无首个提交，本轮无法使用可靠提交差异作完整基线。
- 第三方主要检查接入与改动，不是对所有第三方几百万行代码逐行重新审计。
- 复用代码改 namespace 不消除许可证义务；正式分发需核对对应源码及版权说明。
- [REUSE_PLAN.md](REUSE_PLAN.md) 和 [INTEGRATION_REPORT.md](INTEGRATION_REPORT.md) 是历史材料，其中旧的"直接可用""无需适配""需要 prod.keys"等说法不覆盖当前代码和本报告。
- 当前是可供实机验收的真实实现候选，不是"全部功能已完成"。代码和测试能证明的部分已明确列出，不能执行的硬件/服务验证也保留在待办中。
