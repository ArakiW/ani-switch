# ani-switch

在 Nintendo Switch 上跑一个 Bangumi(番组计划)客户端。  
本仓库是 **open-ani/animeko** 的 Switch 移植项目(从零重写,不是 source recompile)。

## 状态

**业务功能 + 网络代理优先阶段**(2026-09-10 v20 起)。

- **v20.0 NRO ship**: 代理优先 + Ani API 数据面 + 首页 rail IA。NRO 33,724,764 B, SHA `2a5a43fbb1b1...`, dist `ani-switch-sd-v16.zip`。**核心转向**: 不再修 raw DNS→Cloudflare 直连(实机证明 TCP:443 被挡); 改为 `HTTP::applyProxy` 走用户 PC 上的 Clash (Allow LAN), 有代理时 **跳过 raw DNS rewrite**。设置页可填 `http://<PC-IP>:7897`。数据面优先 `api.animeko.org` `/v1/trends` + `/v1/schedule/airing`, 失败回退 Bangumi。主题 seed 改 Animeko 紫 `#4F378B`。config 真正落盘 `httpProxy` + ani JWT(以前是死字段)。详见 `docs/v19-network-p0-status-20260910.md` 重审结论。
- **v20.1 NRO ship**: 封面图走代理 + 推荐页接 `/v1/trends`。NRO SHA `C3C2C99D...`。**v20.1 实机闪退** (startup.8): `api.animeko.org` **直连 443 + TLS + 200 已通**(Railway 69.46.46.113, 无需代理), 但 2 帧后 `userAppExit`。嫌疑: (1) ImageLoader 多线程 `createSession` + `NET_DEBUG_VERBOSE` 并发写 log (2) home 同步回退 Bangumi 嵌套 (3) v20.0 自定义 rail layout。
- **v20.2 NRO ship (修闪退)**: NRO SHA `E05BC684...`, dist v18。仍闪退 (startup.9): trends 200 → 2 帧 → exit。**真凶候选**: `SubjectCell::setSubject` 调 `setImageFromRes("")` → `romfs::get("")` 在 `performSyncTasks` 里炸。这是首次真正拿到非空列表才走到的路径。
- **v20.4 诊断 (startup.11)**: 纯文本列表 **成功** — `HOME: onTrending n=24 cap=8 done`。随后 `player: mpv init` → 闪退。**结论**: 首页列表渲染已通; 新阻塞是 **mpv 播放器初始化** (PlayerActivity/VideoView 首次 `MPVCore::instance()`), 与网络无关。
- **v21.0 NRO (全功能一次交付)**: 封面改为单线程 ImageLoader 队列; 搜索优先 `Ani /v2/subjects/search`; 本地视频只扫 `videos/`; 设置代理; 主页三 tab + 顶栏。NRO SHA `270E4EB7C423...`, dist `ani-switch-sd-v34.zip` + `videos/test-local.mp4`。播放器 mpv 仍有 breadcrumb, 待实机定位。

- **v18.0 ~ v18.9**: 主题切换 / 详情 / 评分 / 收藏 / 启动引导 / 本地视频 / 历史 v2 / 启动日志 / BangumiSyncTab / EmailLogin / 设置 4 tab / 主题预览 / 启动日志查看器 / 我的收藏 v2 全 ship,16 个版本。
- **v19.0.1 NRO ship**: 33,593,692 B, SHA `260E665C71B1...`,dist `ani-switch-sd-v12.zip`。带 `HTTP::NET_DEBUG_VERBOSE` 抓 mbedTLS `infof()` + SSL record hex dump 写 `startup.log` — 为定位 P0 网络问题。
- **v19.0.2 NRO ship**: 33,581,404 B (Δ -12,288 B vs v19.0.1), SHA `39B32F22F1BA...`, dist `ani-switch-sd-v13.zip` (15,842,899 B, SHA `5A59307F25BA...`)。**Case A 修复**: 删 `kHardcodedHosts[]` 表 (6 入口) + `lookupHardcodedHost()`,改走 `aniswitchResolveHost` 每次 raw DNS query 8.8.8.8:53 + 进程级 cache。Build 用 `devkitpro/devkita64:20260219` 一次过 (cold 链接 ~3 min)。Patch 来自 `docs/v19.0.2-case-A.patch`,手动 Edit apply (git apply 失败: repo 无 HEAD base),`src/net/http.cpp` 删 39 行 + 改 2 处 caller (`_cpr_post` + `_cpr_get`) 把 hardcoded 短路径去掉。**Patch 文件已补 NOTE block**(`docs/v19.0.2-case-A.patch` 末尾 50+ 行), 别人 git apply 时跟着 NOTE 改 `_cpr_post` / `_cpr_get` 不会 compile fail。
- **v19.0.2 case B' skeleton in NRO**: `src/net/mbedtls_direct.{hpp,cpp}` 21,408 B 总,直接 mbedTLS 2.28 API 重建 HTTPS fetch,4 cipher list (ECDHE-RSA/ECDSA + AES-256-GCM + CHACHA20) + 5 sig_hash 显式 (排除 SHA1) + `MBEDTLS_SSL_VERIFY_REQUIRED`。`HTTP::directFetch()` + `HTTP::USE_DIRECT_TLS` toggle 加进 http.hpp/cpp(默认 false,不接 production)。**v19.0.2 NRO 已 link 进去** — case A worker build 路径上 mbedtls_direct.o 顺手编了,NRO 多 ~30KB 没用代码但 readiness+1。**skeleton 已知小问题** (2026-09-10 修): `mbedtls_x509_crt_parse_file` 返回负值时 `rc` 不显式 reset 到 0(虽然 `mbedtls_ssl_config_defaults` 下一行会覆盖 rc,功能上等价,只是意图不明显);TLS 版本 pin 注释写错 (说 "Cloudflare 1.3 edge 用 X25519MLKEM768",实际 mbedTLS 2.28 根本不支持 TLS 1.3 才 pin 1.2)。修后未重 ship — NRO 字节变了 (SHA `6B212BF...`) 但 case A 路径 / case B' runtime 行为完全一致,不交付。
- **v19.0.3 NRO ship**: `platform/switch/switch_wrapper.c:81-215` `aniswitchResolveHost` 改走 **3 nameserver fallback**: 8.8.8.8 (Google) → 223.5.5.5 (AliDNS) → 119.29.29.29 (DNSPod), 各自 5s timeout, 第一个成功的 wins。修 v19.0.2 case A 修复在 8.8.8.8 国内被屏蔽时的盲点。`aniswitchTryResolver` static helper 抽出 nameserver 独立逻辑。预编译的 network byte order 值 (LE-encoded `0x08080808`/`0x050505DF`/`0x1D1D1D77` for aarch64, 因 htonl() 不是 C 常量表达式)。NRO 33,581,404 B, SHA `69F11E09E3BF...`, dist `ani-switch-sd-v15.zip` 15,842,676 B, SHA `37FCFE6822F9...`。**Ship 理由** (2026-09-10): v13 实机 case A 修复 work (DNS OK) 但 TCP:443 timeout — 换 nameserver 可能拿到不同 anycast IP, 5 分钟 ship + 1 次实机, 比改 case B' 1-2 天快。
- **v19.0.4 case B' 激活 pre-staged (不 ship)**: `src/net/http.cpp` 把 v19.0.2 case B' skeleton 的 `HTTP::directFetch` 真接到 `_cpr_get` / `_cpr_post`。当 `HTTP::USE_DIRECT_TLS = true` (默认 false) 时,两个 cpr 入口都改走 mbedTLS-direct 路径,cpr/curl 不被调用。**新加 helper `directCallBypass`**(~70 行,namespace 匿名)走 `cpr::CurlHolder` URL-encode + `parameters.GetContent(holder)` / `payload.GetContent(holder)`,跟 cpr 自己 encode 出来的 wire 形式 1:1 一致。结果映射回 `cpr::Response` (`status_code` / `text` / `elapsed` / `error.code=SSL_CONNECT_ERROR` on transport fail) — `parseJson<T>` / `postVoid` / `bgm_client` / `dandanplay_client` 全部 caller 不需要改。Status 200-299 inclusive 算 success (比 cpr path 的 200/204 更宽松)。**Out of scope** (deliberately): `bgm_client.cpp::authedGet/Post`、`dandanplay_client.cpp::dandanGet/Post`、`bgm_auth.cpp` 4 处、`ani_client.cpp` 2 处 — 这些直接用 cpr `GetCallback`/`PostCallback`,不走 `_cpr_*`,**v19.0.4 不动**。Settings UI 切换 `USE_DIRECT_TLS` 是另个 sprint 的活(后端就绪,UI 没动)。**已 build verify**: NRO 33,585,500 B (Δ +4,096 B vs v19.0.3,正好 ~100 行新 C++),SHA `64943949...` 在 `release/20260910-015817/`,不 ship 不打 dist zip — 等 v13 (case A) + v19.0.3 (DNS fallback) 实机结果再决定 ship 哪一版。
- **mbedTLS 2.28 API 真名(2.28 跟 3.x 不一样)**: `mbedtls_ssl_conf_sig_hashes` (3.x 叫 `mbedtls_ssl_conf_group_supported_sig_algs`) + `mbedtls_ssl_conf_curves` (2.28,3.x 改 `mbedtls_ssl_conf_groups`)+ `mbedtls_ssl_set_hostname` 必须在 `mbedtls_ssl_setup` 之后调。`MBEDTLS_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384` = 0xC030 等(2.28 header 写明)。
- **v19.0.2 预案**(`docs/v19.0.2-playbook.md`): 4 case 决策树 + case A patch 备好(`docs/v19.0.2-case-A.patch`)。最可能是 case A — hardcoded IP `104.26.8.23` 是 2026-09-08 解析的,Cloudflare anycast edge 每天 rotate 失效导致 TCP RST。
- **网络 P0(待 user 实机)**: 主页 tab `api.bgm.tv/calendar` TCP:443 timeout 进 setEmpty。v13 (v19.0.2) 实机: DNS 解析 OK (拿到 172.67.73.67 / 173.255.209.47), 但 TCP SYN 5s 无响应。**v19.0.3 已 ship (dist v15)**, 等 user 拷 SD 卡跑 + 贴 `startup.log` — 若换 DNS 拿到不同 IP 能连上则收手; 若仍 timeout 几乎肯定 port 443 DPI, 走方案 C (国内 VPS 反代)。完整分析见 `docs/v19-network-p0-status-20260910.md`。

详细路线见 `docs/REUSE_PLAN.md` + `docs/v19.0.2-playbook.md`。

## 关键依赖

所有第三方都从 [xfangfang/wiliwili](https://github.com/xfangfang/wiliwili) 的 `library/` 复用:

- `borealis` v2.1.0+ — UI 库 + Switch 平台层(deko3d 输入/视频/音频/字体)
- `cpr` — libcurl 的 C++ 包装
- `lunasvg` — SVG 渲染
- `OpenCC` — 简繁转换
- `QR-Code-generator` — 二维码
- `pystring` — string 工具
- `mongoose` — WebSocket + 自定义 DNS
- `libpdr` — Switch system getter

## 视频解码

走 wiliwili 同款路径:**averne/ffmpeg (含 Tegra NVDEC 硬解) + libmpv + deko3d**。
预编译包从 `https://github.com/xfangfang/wiliwili/releases/download/v0.1.0/` 下载。

## 工具链

- devkitpro / devkita64(跟 AC-SWITCH 同一个)
- CMake 4.4+
- Docker 构建(推荐):`docker run --rm -v $(pwd):/data devkitpro/devkita64:20251117`

## 重要约束

1. **许可证**:本仓库为 AGPL-3.0。vendored 自 wiliwili 的文件需保留 SPDX 头。
2. **不是 fork**:本仓库不基于 wiliwili fork,而是独立仓库 + git submodule 引用 wiliwili 的
   第三方库。理由:wiliwili 业务层(全部 B 站 API 代码)我们完全不用,fork 会带进来大量
   死代码和 license 纠纷。
3. **必须真机验证**:每次 platform/switch 改动,SD 卡装机跑过才算 done。`build.log`
   通过不等于 NRO 在 Switch 上能跑过。
4. **Phase 0 没过的方案不要进 Phase 1**:spike 失败的话整个方案要重新评估。

## 网络层构建坑(从 v19.0.1 学到的)

- **cpr 1.10.5 `FetchContent_Declare(curl GIT_REPOSITORY/URL)` 从 GitHub 拉
  curl/zlib-ng/mongoose 在 devkitpro docker 内被 HTTP/2 限流(2026-09-09 起)。
  **修法**:docker 内从 `https://curl.se/download/curl-8.4.0.tar.xz` /
  `https://github.com/zlib-ng/zlib-ng/archive/refs/tags/2.0.6.tar.gz` /
  `https://github.com/cesanta/mongoose/archive/refs/tags/7.7.tar.gz`
  拉(sha256 验证),解压到 `third_party/{curl-8.4.0,zlib-ng-2.0.6,mongoose-7.7}/`,
  改 cpr `CMakeLists.txt` `FetchContent_Declare(... SOURCE_DIR ...)` 跳过 GitHub。
- **curl 8.4.0 cmake `try_run()` 在 cross-compile mode 默认会报错**。修法:在
  `ani-switch/CMakeLists.txt` PLATFORM_SWITCH 块预设 cache vars:
  `HAVE_H_ERRNO_ASSIGNABLE_EXITCODE=0`, `HAVE_H_ERRNO_ASSIGNABLE=TRUE`,
  `HAVE_H_ERRNO=TRUE`(curl `OtherTests.cmake` line 175 调 check_c_source_runs
  → try_run → 没 cache cross-compile fail)。
- **cpr 1.10.5 `SetDebugCallback` 需要 lambda 3 参 + 包装**:
  `session->SetDebugCallback(cpr::DebugCallback{[](InfoType, std::string, intptr_t){...}});`
  不是 `SetDebugCallback(lambda)`(compile fail "cannot convert lambda to DebugCallback&")。
- **mount 写入 docker 容器会 SMB corrupt**(2026-09-09 出现一次):从 GitHub
  release 下 tarball 到 docker `/tmp/`,在 docker 内解压 + `cp -a` 到
  `/proj/third_party/<name>/`,不要 docker run 期间 host 直接 cp。

## 工具链

- devkitpro / devkita64:20260219(2026-09-09 验证,跟 AGENTS.md 写的不一样
  — 旧的 20251117 升级到 20260219)
- CMake 4.4+
- Docker 构建:`docker run --rm -v $(pwd):/data -w /data devkitpro/devkita64:20260219`
- 内部 portlibs mbedTLS 2.28.10(没 TLS 1.3 支持 — 100% TLS 1.2 only)

## 目录速查

```
docs/REUSE_PLAN.md       # 复用方案(必读)
platform/switch/         # NRO 工具链
core/                    # 业务核心(自写)
player/                  # 视频播放器(从 wiliwili 复用)
net/                     # HTTP + Bangumi/dandanplay API(自写)
ui/                      # 业务 UI(activity/fragment/presenter,自写)
utils/                   # 工具(从 wiliwili 复用 90%)
third_party/             # 9 个 vendored 库(都是 wiliwili 的子模块)
resources/               # 字体/图标/字符串/XML 布局
scripts/                 # build_switch.sh 等
```

## 跟 AC-SWITCH 的关系

- 同样的 devkitpro 工具链
- 同样的 NRO + hbmenu 部署
- 同样的 deko3d 坑(已踩)
- 但 **不是同一份代码**,ani-switch 的平台层是 borealis(XITRIX fork),
  AC-SWITCH 的平台层是 libnx + nvn 直接调用

## 沟通语言

中文优先,代码注释中英双语。

---

详细的"什么复用、什么新写"看 `docs/REUSE_PLAN.md`。
