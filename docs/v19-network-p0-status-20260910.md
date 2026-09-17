# ani-switch v19.0.x Network P0 状态报告

**时间**: 2026-09-10 02:56 (CST)
**作者**: Mavis
**目标**: 把 v19.0.x 全部 ship / pre-stage 状态 + v13 实机 startup.log 分析 + 下一步决策树固化下来

---

## 1. TL;DR

| 字段 | 值 |
|---|---|
| **v19.0.2 (case A) NRO** | 已 ship, `dist/ani-switch-sd-v13.zip` 15,842,899 B, SHA `5A59307F25BA...` |
| **v19.0.3 (DNS fallback) NRO** | **已 ship** 2026-09-10, dist `ani-switch-sd-v15.zip` 15,842,676 B, SHA `37FCFE6822F9...`, NRO SHA `69F11E09E3BF...` |
| **v19.0.4 (case B' 激活) NRO** | 已 build 完, `release/20260910-015817/aniswitch.nro` 33,585,500 B, SHA `64943949...`, **不 ship** |
| **v13 实机结果** | DNS 解析 OK, **TCP:443 timeout**, 不是 DNS / TLS 问题, 是 port 443 在某层被挡 |
| **当前阻塞** | 等 user 拷 v15 zip 实机 + 贴 `startup.log` — 换 DNS 拿不同 anycast IP 可能解 |

---

## 2. 背景

ani-switch (`E:\AI\ani-switch`) 是把 Bangumi 客户端 (open-ani/animeko) 移植到 Nintendo Switch 的项目。NRO ship 累计 v17.0 ~ v18.9 共 16 个版本。v19.0 系列专门修网络 P0 (主页 tab `api.bgm.tv/calendar` 5s timeout 一直 timeout 进 setEmpty)。

---

## 3. v19.0.1 (debug build) — 已 ship

- NRO 33,593,692 B, SHA `260E665C71B1...`
- dist `dist/ani-switch-sd-v12.zip` 15,837,622 B
- 关键改动: 加 `HTTP::NET_DEBUG_VERBOSE` 静态开关, 抓 mbedTLS `infof()` 文字 + `SSL_DATA_IN/OUT` 16 进制 dump 到 `startup.log`
- 目的: 拿 mbedTLS 实际 client hello / server hello 字节, 定位 P0 网络问题
- 状态: 等待 user 拷 SD 卡跑 + 贴 startup.log 验证

---

## 4. v19.0.2 (case A fix) — 已 ship ✓

- NRO `E:\AI\ani-switch\aniswitch.nro` 33,581,404 B, SHA `39B32F22F1BA77E5811A12E8E543364906888499ADC79B59202F691861F98E7C`
- dist `E:\AI\ani-switch\dist\ani-switch-sd-v13.zip` 15,842,899 B, SHA `5A59307F25BA45BD26E90C56A60214B50DB1468E3B5E9E7ADD27D5820EB0F097`
- release dir `E:\AI\ani-switch\release\20260910-011258\`
- 关键改动: **case A** — 删 `kHardcodedHosts[]` 6 入口 + `lookupHardcodedHost()`, 改走 `aniswitchResolveHost` 每次 raw DNS query 8.8.8.8:53 + 进程级 `kDnsCache` cache
- case A patch 文件: `docs/v19.0.2-case-A.patch` 138 行 (已自包含 + NOTE block + `_cpr_post` / `_cpr_get` 2 caller 模板)
- 额外补: `_cpr_post` (L304) + `_cpr_get` (L388) 2 处 v16.10.8 inline hardcoded 短路径 (patch 没覆盖, 不补会 `undefined reference` 编译挂)
- 同时 link 进 case B' skeleton: `mbedtls_direct.{hpp,cpp}` 21,408 B (dormant, `USE_DIRECT_TLS=false`)
- Build: docker `devkitpro/devkita64:20260219` 一次过, cold link ~3 min, 0 warning
- 实机 log (`sdmc:/switch/aniswitch/startup.log` user 2026-09-10 02:48 贴):
  - `HTTP: RESOLVE api.bgm.tv -> 172.67.73.67 (raw DNS)` ✓ (case A 修复 work)
  - `HTTP: RESOLVE api.bgm.tv -> 173.255.209.47 (raw DNS)` ✓ (不同 anycast IP, 正常)
  - `CURL: TEXT ipv4 connect timeout after 4989ms, move on!` ✗ (TCP:443 timeout)
  - `HTTP: GET https://api.bgm.tv/calendar -> status=0 error=Failed to connect to api.bgm.tv port 443 after 5001 ms`

**结论**: case A 修复 work (DNS 通过 8.8.8.8 拿到 Cloudflare 真实 IP), 阻塞是 **TCP connect to port 443 timeout**, 不是 DNS / TLS。

---

## 5. v19.0.2 case B' skeleton — 已 build (dormant in NRO)

- `src/net/mbedtls_direct.hpp` 6,254 B + `.cpp` 15,154 B = 21,408 B
- `HTTP::directFetch(url, method, body, headers, timeout_ms)` + `HTTP::USE_DIRECT_TLS` static inline bool
- 4 cipher list (ECDHE-RSA-AES-256-GCM 0xC030 + ECDHE-RSA-CHACHA20 0xCCA8 + ECDHE-ECDSA-AES-256-GCM 0xC02C + ECDHE-ECDSA-CHACHA20 0xCCA9)
- 5 sig_hash (排除 SHA1, 跟 RFC 9155)
- `MBEDTLS_SSL_VERIFY_REQUIRED`
- `aniswitchResolveHost` 拿 IP → 喂 `mbedtls_net_connect` IP literal 走 `getaddrinfo` 数字 fast path, 不撞 newlib broken gethostbyname
- TLS 1.2 pin (mbedTLS 2.28 根本不支持 1.3)
- 默认 `USE_DIRECT_TLS = false`, 不接 production path
- 已 link 进 v19.0.2 NRO, 死代码 ~30KB, readiness+1

**已知小 bug** (2026-09-10 修, **不 ship**, runtime 行为等价):
- `mbedtls_x509_crt_parse_file` 返回负值时 `rc` 不显式 reset 到 0 (虽然下一行 `mbedtls_ssl_config_defaults` 会覆盖 rc, 功能等价, 只是意图不明显)
- 修后 NRO SHA `6B212BF044355F440E1E2CB4D8922FCF51B3F4994C1886BCB4D5224174C79798` (vs 原 `39B32F22F1BA...`), size 同 33,581,404 B, 在 `release/20260910-014320/`

---

## 6. v19.0.3 (DNS fallback) — 已 build, 不 ship

- 关键改动: `platform/switch/switch_wrapper.c:81-215` `aniswitchResolveHost` 改走 **3 nameserver fallback**:
  1. 8.8.8.8 (Google) — LE uint32 `0x08080808u` (palindrome)
  2. 223.5.5.5 (AliDNS, China-friendly anycast) — LE uint32 `0x050505DFu` (bytes DF 05 05 05)
  3. 119.29.29.29 (DNSPod, China-friendly anycast) — LE uint32 `0x1D1D1D77u` (bytes 77 1D 1D 1D)
- 各自 5s timeout, 第一个成功 wins
- 抽 `aniswitchTryResolver` static helper, 单一 nameserver 独立逻辑
- 修 v19.0.2 case A 修复在 8.8.8.8 国内被屏蔽时的盲点
- NRO `release/20260910-015012/aniswitch.nro` 33,581,404 B, SHA `69f11e09e3bffaae01854b72c9cfbdf4fdb7f078c989116ddd6aaa3b6d7e21fb`
- size 同 v19.0.2 (C 代码 + nameserver 表 + helper 抽出互相抵消), SHA 不同 (binary bytes 因 link order 变)

**build 坑 (1)**: C 不允许 `htonl()` 用在 static 数组初始化 (不是 constant expression), 改用预编译 LE-encoded 网络字节序值, Switch 永远 LE, 固定表够。

**不 ship 决定 (3 理由)**:
1. user 之前明说 "别让我反复真机测试"
2. v13 case A 实机前不知道是否真修 (8.8.8.8 可能 work)
3. v19.0.3 走 DNS fallback + (可能要) case B' 切换, 真要 ship 应该合并这两个

**但 v13 实机后**: case A 修复 work (DNS OK), 阻塞是 TCP:443, 不是 DNS。换 nameserver 可能拿到不同 anycast IP, **有合理概率解**。5 分钟 ship + 1 次实机, 比改 case B' 1-2 天 ship 快。

---

## 7. v19.0.4 (case B' 激活) — 已 build, 不 ship

- 关键改动: `src/net/http.cpp` +132 行, 新加 `directCallBypass()` helper 在匿名 namespace (57-164), `_cpr_post` 顶端 (378-389) + `_cpr_get` 顶端 (482-493) 各加 12 行 `#if __SWITCH__ + if(USE_DIRECT_TLS) { directCallBypass(...); return; }`
- 默认 `USE_DIRECT_TLS = false` 不变, production cpr 路径 100% 跟 v19.0.3 一致
- cpr 1.10.5 API 实际修正 (worker 发现 directive 几个误假设):
  - `cpr::Parameters` 容器内 `containerList_` 是 **protected**, 必须 `parameters.GetContent(CurlHolder&)` 拿 URL-encoded query string
  - `cpr::Payload` 没有 `.str()` 方法 (那是 cpr::Url/StringHolder 才有的), 走 `payload.GetContent(CurlHolder&)`
  - `cpr::Error::operator bool()` = `code != ErrorCode::OK`
- NRO `release/20260910-015817/aniswitch.nro` 33,585,500 B (Δ +4,096 B vs v19.0.3 = 4KB for ~100 行 C++), SHA `649439496023B585C54DE88B11F000A59EBF098E1BA21A07C748E03D80C60BA6`
- 没 ship dist zip, 没发 deliver-assets, NRO 跟 v19.0.3 NRO 并列

**没解决边角 (AGENTS.md 写明)**:
1. `cpr::Error::error` 的 fake: 用 `fakeR.error.code = cpr::ErrorCode::SSL_CONNECT_ERROR; fakeR.error.message = r.error;` 直接 field assignment
2. Status 码 200-299 inclusive 比 cpr 路径 (200/204 only) 更宽松
3. Out of scope: `bgm_client.cpp` / `dandanplay_client.cpp` / `bgm_auth.cpp` 8 处不走 `_cpr_*` 路径, v19.0.4 没接
4. Settings UI 待 v19.0.5 — 当前 `USE_DIRECT_TLS` 切换只能改 hardcoded bool (用户得手动改源码 rebuild)

**关键评估**: **TCP:443 timeout 的话, v19.0.4 也救不了**。因为 case B' (mbedTLS direct) 走的还是 TCP, 同样 SYN timeout 的话换 TLS 库没用。真要绕开 port 443 阻塞, 需要走 HTTP/80 (Bangumi.tv 没这端口) 或 VPN / proxy。

---

## 8. v13 实机 log 详细分析

`C:\Users\L\.minimax\v2\assets\2026\09\10\02-48-44-523-asset_20260910-024844-523_3d52dad035a6_8f2893f5-startup.log` 关键 6 行:

```
NETDIAG: profile ok, ip=0.0.0.0 sn=0.0.0.0 gw=0.0.0.0       ← Line 11: 0.0.0.0 IP
NETDIAG2: TCP connect 1.1.1.1:80 OK                            ← Line 15: TCP:80 OK
NETDIAG3: DNS api.bgm.tv -> no-A-record                        ← Line 14: boot 诊断, 跟实际 HTTP 路径不同
ANISWITCH v19.0.2 debug build marker                           ← Line 46: 确认 v19.0.2 NRO
HTTP: RESOLVE api.bgm.tv -> 172.67.73.67 (raw DNS)             ← Line 47: case A 修复 work
main: ... [UI 渲染循环 ~ 120 行]
HTTP: GET https://api.bgm.tv/calendar                          ← Line 168: 主页触发的真实请求
HTTP: RESOLVE api.bgm.tv -> 173.255.209.47 (raw DNS)           ← Line 170: 不同 anycast IP, 正常
CURL: TEXT WARNING: failed to open cookie file ""              ← Line 171: cookie 文件路径空, 不是阻塞
CURL: TEXT Added api.bgm.tv:443:173.255.209.47 to DNS cache    ← Line 172: curl 把我们 pre-resolved IP 加到自己 cache
CURL: TEXT Hostname api.bgm.tv was found in DNS cache         ← Line 173
CURL: TEXT   Trying 173.255.209.47:443...                      ← Line 174
CURL: TEXT ipv4 connect timeout after 4989ms, move on!         ← Line 175: TCP SYN 5s 无响应
CURL: TEXT Failed to connect to api.bgm.tv port 443 after 5001 ms: Timeout was reached  ← Line 176
HTTP: GET https://api.bgm.tv/calendar -> status=0 error=Failed to connect to api.bgm.tv port 443 after 5001 ms: Timeout was reached  ← Line 178
```

### 关键对比

| 目标 | 端口 | 结果 |
|---|---|---|
| 1.1.1.1 (Cloudflare DNS, IP literal) | TCP 80 | ✓ OK |
| 173.255.209.47 (api.bgm.tv 解析结果) | TCP 443 | ✗ 5s timeout |
| 172.67.73.67 (api.bgm.tv 另一 anycast) | TCP 443 | ? (cache miss 后才用, 没真连过) |

### 排除项

- ❌ **不是 DNS 问题** — 拿到 Cloudflare 真实 IP 172.67.73.67 / 173.255.209.47
- ❌ **不是 TLS 问题** — TCP SYN 都没回, 还没到 handshake
- ❌ **不是 cpr/curl 问题** — curl 在等 TCP, 等不到
- ❌ **不是 mbedTLS 问题** — 没轮到

### 嫌疑

1. **port 443 在某层被挡** (家用路由器 / 公司网 / ISP 防火墙 / 国内 ISP 对 443 做 DPI 干扰)
   - 验证: 换 DNS 拿不同 IP, 还是连不上 → 几乎肯定
   - 验证: 改用 HTTP/80 或代理 → 应该能通
2. **特定 anycast IP 路由黑洞** (Cloudflare 边缘 IP 偶发不可达)
   - 验证: 换 DNS 拿不同 IP, 连上了 → 是这个
   - 验证: 同一 IP 重试多次, 还是连不上 → 几乎肯定
3. **Switch 的 IP 是 0.0.0.0** (Line 11 提示)
   - 但 1.1.1.1:80 通, 所以这个 0.0.0.0 应该是占位 (nifm 没真分配但 BSD socket 层能用)
   - 排除

---

## 9. 下一步决策树 (按 v19.0.3 实机结果)

### 决策 0: ship v19.0.3 (DNS fallback) 试一次 ✅ 已 ship
- NRO `release/20260910-015012/aniswitch.nro` 33,581,404 B, SHA `69f11e09e3bffaae01854b72c9cfbdf4fdb7f078c989116ddd6aaa3b6d7e21fb`
- dist `ani-switch-sd-v15.zip` 15,842,676 B, SHA `37FCFE6822F9FD26538B0B18C682E500A320174296010566DEAE0B45FADAB8BF`
- **理由**: 8.8.8.8 国内 anycast 路径可能返回黑洞 IP, 223.5.5.5 / 119.29.29.29 在国内 anycast 路径不同, 很可能返回不同 IP 就能 work

### 决策 1: v19.0.3 实机结果
- **case A 通了 (主页 tab 加载到日历数据)**: 收手, 走下个 sprint (v19.0.3 = 收藏 v3 / 主题预览 / 邮箱登录 等任挑)
- **case A 没通 + 8.8.8.8 屏蔽 (`NETDIAG3` 跟 `HTTP: RESOLVE` 都 timeout)**: 已经 ship 了 v19.0.3 DNS fallback, 这时候基本是 port 443 DPI, case B' 也救不了 (TCP 都连不上), 需要绕路:
  - **方案 A**: HTTP proxy 走 80 端口 (但 Bangumi.tv 没 80)
  - **方案 B**: SSH tunnel / WireGuard VPN
  - **方案 C**: 自建 TLS terminator, 在国内 VPS 上 80/443 reverse proxy → Bangumi.tv:443 (绕开本地 ISP DPI)
  - **方案 D**: Cloudflare WARP 客户端 (UDP 走 443, 但 Switch 上没 WARP)

### 决策 2: 如果 v19.0.3 仍 TCP:443 timeout, 但不同 IP 也都 timeout
- 几乎肯定 port 443 DPI
- 走方案 C (国内 VPS 反代) 是最干净的
- 1-2 day ship: VPS 上 nginx stream + Switch 端加 cpr::Proxies 字段

### 决策 3: 如果 v19.0.3 拿到不同 IP 但**还是 timeout**
- 可能 Cloudflare 该 IP 段特定被挡
- 同样走方案 C (国内 VPS 反代)

---

## 10. 问题清单 (P0 阻塞 + 已知 bugs + 工具链坑)

### 10.1 P0: TCP:443 timeout (当前阻塞)

| 字段 | 内容 |
|---|---|
| **症状** | `CURL: TEXT ipv4 connect timeout after 4989ms, move on!`, `HTTP: GET ... -> status=0 error=Failed to connect to api.bgm.tv port 443 after 5001 ms: Timeout was reached` |
| **触发场景** | v13 NRO (case A 修复) 实机, 主页"热门动画" tab 触发的 `GET /calendar` |
| **根因** | Switch 当前网络 (WiFi / 路由) 对 `api.bgm.tv:443` 的 SYN 包 5s 无响应 |
| **可能性** | (1) port 443 在家用路由器 / 公司网 / ISP 防火墙被挡 (国内 ISP 经常对 443 做 DPI 干扰)<br>(2) 8.8.8.8 返回的 anycast IP 路由黑洞 (Cloudflare 边缘偶发)<br>(3) Switch 的 IP 是 0.0.0.0 (Line 11 `NETDIAG: profile ok, ip=0.0.0.0`) 但 1.1.1.1:80 通, 排除 |
| **当前 workaround** | 改 DNS nameserver 试不同 anycast IP (v19.0.3 已经 pre-stage 好) |
| **修复状态** | **等 v19.0.3 实机结果** (5 min ship + 1 实机) |
| **影响** | v13 NRO 主页完全不能加载, 用户进 setEmpty 兜底页 |

### 10.2 P1: Switch hbmenu applet 模式网络限制 (root cause)

| 字段 | 内容 |
|---|---|
| **症状** | Switch 在 hbmenu / homebrew-launcher applet 模式下, nifm profile 显示 `ip=0.0.0.0` (Line 11), BSD socket 拿到一个伪网络状态 |
| **根因** | Switch OS 限制 applet 模式下 nifm 拿不到真 IP, 旧版 `socketInitialize(&cfg)` 完全不 attach, 升级 `socketInitializeDefault()` (3 sessions, sb_efficiency 1) 后部分能通 (1.1.1.1:80 OK) 但不是 100% |
| **影响** | DNS UDP 走 raw socket 能通 (return 真实 IP), 但 TCP connect 走 BSD socket 受限 (具体 port 表现不一致) |
| **当前 workaround** | (1) 用 raw UDP DNS resolver 绕过 gethostbyname 走 8.8.8.8:53 (v16.10.8 ship, 现在还有效)<br>(2) 期望走 full title mode 拿到真 IP, 而不是 applet mode |
| **修复方向** | (1) 加 `nifmInitialize(NifmServiceType_User)` 显式申请 network profile (v16.10.4 试过但没真生效, 待重试)<br>(2) 用户跑 full title mode (Atmosphere hbmenu → launch NRO as title) |
| **状态** | 已知问题, root cause 已识别, 待验证 full title mode 是否 work |

### 10.3 P1: cpr/curl 8.4 mbedTLS backend 写死 (case B 根因)

| 字段 | 内容 |
|---|---|
| **症状** | cpr 1.10.5 vendored curl 8.4.0 + devkitpro portlibs mbedTLS 2.28.10, backend 写死 cipher list + VERIFY_OPTIONAL + 不实现 `CURLOPT_SSL_CTX_FUNCTION` hook |
| **根因** | curl 8.4 mbedtls.c line 591: `mbedtls_ssl_conf_authmode(OPTIONAL)` 写死, line 600-601: `mbedtls_ssl_list_ciphersuites()` 忽略 `CURLOPT_SSL_CIPHER_LIST` |
| **影响** | 想 pin 特定 cipher / 强制 VERIFY_REQUIRED / 注入 custom mbedTLS hook 都做不到, 只能全 bypass cpr 整套栈 |
| **当前 workaround** | 写 mbedtls_direct.{hpp,cpp} 直接调 mbedTLS 2.28 API 重建 HTTPS fetch, 不经 cpr/curl (case B' skeleton 已 ship) |
| **修复方向** | 切到 OpenSSL backend (Switch portlibs 没装, 不可行) 或 BoringSSL (没现成 portlibs, 自己 build 风险大) |
| **状态** | case B' skeleton + 激活 都 pre-staged, 默认 `USE_DIRECT_TLS=false`, user 需手动改 bool 或加 settings UI (待 v19.0.5) |

### 10.4 P1: case A patch 文件不完整 (历史坑, 已修)

| 字段 | 内容 |
|---|---|
| **症状** | `docs/v19.0.2-case-A.patch` 只 patch 了 `rewriteUrlForIP` 一个函数, 没 patch `_cpr_post` (L304) + `_cpr_get` (L388) 两处 v16.10.8 inline hardcoded 短路径 |
| **根因** | patch 是基于 v19.0.1 的 http.cpp 写的, 但 v16.10.8 时 `_cpr_post` / `_cpr_get` 各自 inline 复制了一份 hardcoded 短路径, 没被 patch 覆盖 |
| **影响** | 别人 `git apply` 这个 patch 会 compile fail: `undefined reference to lookupHardcodedHost` |
| **当前 workaround** | patch 文件 138 行, 末尾加 NOTE block + 2 caller 模板, 跟 NOTE 改完就 work |
| **修复方向** | 生成 v2 patch 把 3 处改动全包含, 或直接 ignore (host 上已手动改完) |
| **状态** | ✅ patch 文件已加 NOTE block, AGENTS.md 同步, 写明手动 follow-up 步骤 |

### 10.5 P2: mbedtls_direct.cpp ca-bundle parse `rc=0` 冗余

| 字段 | 内容 |
|---|---|
| **症状** | `mbedtls_x509_crt_parse_file` 返回负值时 `rc` 不显式 reset 到 0 |
| **根因** | 原代码假设下一行 `mbedtls_ssl_config_defaults` 会覆盖 rc, 但意图不明显, code review 容易误解 |
| **影响** | 功能等价 (ssl_config_defaults 总是会 set rc), 但 code clarity 差 |
| **当前 workaround** | 加显式 `rc = 0;` 在 log 后 |
| **修复状态** | ✅ source 修了, build 验证 (NRO SHA `6B212BF...`), 不 ship (runtime 行为等价, user 看不到区别, 没事找事重测浪费) |

### 10.6 P2: mbedtls_direct.cpp TLS 1.2 pin 注释误描述

| 字段 | 内容 |
|---|---|
| **症状** | 注释写 "Cloudflare 1.3 edge uses X25519MLKEM768" |
| **根因** | 我 (Mavis) 写代码时记错: Cloudflare 1.3 edge 接受 X25519 (classical) 跟 X25519MLKEM768 (post-quantum) 两种 key share, 不是只能用 MLKEM768。pin TLS 1.2 的真正原因是 mbedTLS 2.28 根本不支持 TLS 1.3 |
| **影响** | code reader 误解 pin TLS 1.2 的原因 |
| **当前 workaround** | 改注释, 明确说 "mbedTLS 2.28 不支持 TLS 1.3" |
| **修复状态** | ✅ source 修了, 不 ship (跟 P2.5 同一个 rebuild) |

### 10.7 P2: mbedtls_direct.cpp 缺 per-call timeout

| 字段 | 内容 |
|---|---|
| **症状** | `DirectTLS::send` / `recv` 用 `mbedtls_ssl_write` / `mbedtls_ssl_read` 默认阻塞, 没接 `mbedtls_ssl_set_bio` + `mbedtls_net_set_nonblock` 套 select/poll |
| **影响** | 真要 ship case B' 的话, 一个 misbehaving server 能让整个 fetch_https 永久 hang |
| **当前 workaround** | `DirectTLSConfig::timeout_ms` 字段已加, 但 **没实现** — skeleton 范围合理 |
| **修复方向** | 用 `mbedtls_ssl_set_bio` + `mbedtls_net_set_nonblock` 配 select/poll, 整个 fetch 包一层 total timeout |
| **状态** | ⏸️ 等真要 ship case B' 时再实现 (v19.0.5+) |

### 10.8 P2: mbedtls_direct.cpp 缺 chunked transfer-encoding 解码

| 字段 | 内容 |
|---|---|
| **症状** | `parse_http_response` 直接 `*body_out = raw.substr(hdr_end + 4)`, 不解 chunk markers |
| **影响** | 任何用 `Transfer-Encoding: chunked` 的 endpoint 返回都会拿带 chunk markers 的 body, JSON parse 失败 |
| **当前 workaround** | Bangumi.tv 跟 dandanplay.net 都用 `Content-Length`, 不发 chunked, 暂时 work |
| **修复方向** | 解析 chunked 头 + 解码, ~30 行 |
| **状态** | ⏸️ 等真要 ship case B' 时再实现 (v19.0.5+) |

### 10.9 P2: mbedtls_direct.cpp 缺 HTTP redirect follow

| 字段 | 内容 |
|---|---|
| **症状** | `fetch_https` 拿到 3xx 不自动 follow |
| **影响** | 任何 301/302 重定向的 endpoint 失败 |
| **当前 workaround** | Bangumi.tv 跟 dandanplay.net 主 endpoint 都不重定向, 暂时 work |
| **修复方向** | 解析 `Location:` 头, 重新 fetch, max redirect 5 次 |
| **状态** | ⏸️ 等真要 ship case B' 时再实现 (v19.0.5+) |

### 10.10 P2: mbedtls_direct.cpp 缺 keepalive

| 字段 | 内容 |
|---|---|
| **症状** | 每次 `fetch_https` 都新开 TCP+TLS, 串 10 个请求就 10 次 handshake (~3x RTT) |
| **影响** | 性能: 30 个搜索 query 多 ~90 个 RTT, 慢 2-3s |
| **当前 workaround** | n/a |
| **修复方向** | DirectTLS 对象复用, 只在 `close()` 时关连接, 配合 HTTP/1.1 `Connection: keep-alive` |
| **状态** | ⏸️ 性能优化, 不紧急 (v19.0.5+) |

### 10.11 P2: v19.0.4 status 码 200-299 比 cpr 宽

| 字段 | 内容 |
|---|---|
| **症状** | `directCallBypass` 走 `r.status >= 200 && r.status < 300`, cpr 路径只接 200/204 |
| **影响** | 201/202/203 等以前 cpr 路径当 error 的, v19.0.4 当 success |
| **当前 workaround** | n/a (semantic change) |
| **修复方向** | 改 `r.status == 200 || r.status == 204` 跟 cpr 路径对齐, 1 行 |
| **状态** | ⏸️ 等真要 ship case B' 激活时改 |

### 10.12 P2: v19.0.4 out-of-scope 8 处 cpr 路径

| 字段 | 内容 |
|---|---|
| **症状** | v19.0.4 directCallBypass 只在 `_cpr_get` / `_cpr_post` 顶端加, 8 处直接 cpr 调用没接:<br>1. `bgm_client.cpp::authedGet` / `authedPost`<br>2. `dandanplay_client.cpp::dandanGet` / `dandanPost`<br>3. `bgm_auth.cpp` 4 处 `PostCallback` / `GetCallback`<br>4. `ani_client.cpp` 2 处直接 `std::make_shared<cpr::Session>()->Post()` |
| **影响** | 这 8 处即使 `USE_DIRECT_TLS=true` 也仍走 cpr/curl/mbedTLS backend, 享受不到 case B' |
| **当前 workaround** | n/a |
| **修复方向** | v19.0.5 把这 8 处都接 directFetch, 跟 _cpr_get/_cpr_post 同模式 |
| **状态** | ⏸️ 留给 v19.0.5 |

### 10.13 P2: v19.0.4 settings UI 缺失

| 字段 | 内容 |
|---|---|
| **症状** | `USE_DIRECT_TLS` 是 `static inline bool`, user 切换要改源码 + rebuild |
| **影响** | 实机 toggle 不可能, 真要 ship v19.0.4 激活得加 settings UI |
| **当前 workaround** | n/a |
| **修复方向** | v19.0.5 加 borealis toggle in settings, runtime 写一个持久化文件 (类似 firstRun flag), HTTP::createSession 读这个 flag |
| **状态** | ⏸️ 留给 v19.0.5 |

### 10.14 P3: 5 个冷 build 阻塞 (v19.0.1 ship 时撞)

| 字段 | 内容 |
|---|---|
| **症状** | (1) cpr FetchContent 拉 curl-8.4.0.tar.xz / zlib-ng-2.0.6 / mongoose-7.7 全部 GitHub 限流<br>(2) curl 8.4.0 `try_run()` cross-compile fail (`HAVE_H_ERRNO_ASSIGNABLE` in `OtherTests.cmake` line 175)<br>(3) cpr `SetDebugCallback` 编译错 ("cannot convert lambda to `const cpr::DebugCallback&`") |
| **根因** | GitHub HTTP/2 限流 (2026-09-09 起), curl 8.4 cmake 没考虑 cross-compile, cpr 1.10.5 API 文档不清晰 |
| **当前 workaround** | (1) docker 内从 `curl.se/download` / `github.com/{zlib-ng,zlib-ng}/archive/refs/tags/...` / `github.com/cesanta/mongoose/archive/refs/tags/...` 拉 + sha256 verify + 解压到 `third_party/<name>/` + 改 cpr `FetchContent_Declare(SOURCE_DIR)` 跳过 GitHub<br>(2) 在 ani-switch `CMakeLists.txt` PLATFORM_SWITCH 块预设 cache vars: `HAVE_H_ERRNO_ASSIGNABLE_EXITCODE=0` + `HAVE_H_ERRNO_ASSIGNABLE TRUE` + `HAVE_H_ERRNO TRUE`<br>(3) 用 `cpr::DebugCallback{}` 包装 lambda + lambda 收 3 参数 `(InfoType, std::string, intptr_t)` |
| **修复状态** | ✅ 5 个全部一次性修完, v19.0.1 NRO 一次 ship 成功, AGENTS.md 写明 |

### 10.15 P3: C `htonl()` 不是 constant expression (v19.0.3 build 撞)

| 字段 | 内容 |
|---|---|
| **症状** | `static const uint32_t kNameservers[] = { htonl(0x08080808), ... };` 编译报 `initializer element is not constant` |
| **根因** | C 标准要求 static 数组初始化是 constant expression, `htonl()` 是函数不是常量 |
| **当前 workaround** | 预编译 LE-encoded 网络字节序值, Switch 永远 LE (aarch64) 所以固定表够:<br>`0x08080808u` (8.8.8.8 palindrome) / `0x050505DFu` (223.5.5.5 LE uint32 of bytes DF 05 05 05) / `0x1D1D1D77u` (119.29.29.29 LE uint32 of bytes 77 1D 1D 1D) |
| **修复方向** | n/a (平台特定, 跨平台需重新算 BE/LE) |
| **修复状态** | ✅ v19.0.3 build 一次过 |

### 10.16 P3: 3 docker 并行 build 撞车

| 字段 | 内容 |
|---|---|
| **症状** | v19.0.2 ship 时, worker 1 (case A) + worker 2 (case B') 同时跑 docker build, 加上之前残留的 touch 触发的 build, 3 个 docker 同时 mount `/proj` |
| **根因** | docker mount + build 路径冲突, link step 会写同一个 .elf / .nro, 后写的覆盖前写的 |
| **影响** | 浪费时间 (3 个 build 跑同一个东西), 不破坏结果 (谁最后写谁赢, 跟 link order 有关) |
| **当前 workaround** | 串行 dispatch worker + 主动 kill 冗余 docker |
| **修复方向** | 写一个 docker build queue, 一次只跑一个 build (复杂, 不值得) |
| **修复状态** | ⚠️ 没系统性修, 留意不再并行 dispatch build 任务 |

### 10.17 P3: NRO 33MB+ 体积

| 字段 | 内容 |
|---|---|
| **症状** | NRO 33,581,404 B (~32 MB), 比 18.x 系列大 1-2 MB |
| **根因** | vendored cpr + curl 8.4 + mbedTLS 2.28 + borealis + lunasvg + OpenCC + mongoose + fmt + pystring + qrcode + tinyxml2 + marisa + yogacore 全部 link 进 NRO |
| **影响** | Switch 加载 NRO 慢 ~1-2s, 内存占用多 ~10-20MB |
| **当前 workaround** | n/a |
| **修复方向** | (1) 切到 shared library 模式 (Switch portlibs 支持)<br>(2) strip 不用的 symbols (`strip --strip-unneeded`)<br>(3) 关掉 OpenCC (繁简转换不是所有用户用)<br>(4) 关掉 lunasvg (如果不用 SVG 渲染) |
| **修复状态** | ⏸️ 不是 P0, v19.0.5+ 优化 |

### 10.18 P3: mbedtls_direct skeleton 缺 chunked / redirect / keepalive / timeout

(合并到 10.7 - 10.10, 不重复)

---

## 11. 文件索引 (本 session 涉及)

### Ship artifacts
- `dist/ani-switch-sd-v13.zip` 15,842,899 B SHA `5A59307F25BA...` ← v19.0.2 官方 deliverable
- `release/20260910-011258/aniswitch.nro` 33,581,404 B SHA `39B32F22...` ← v19.0.2 source
- `release/20260910-014320/aniswitch.nro` 33,581,404 B SHA `6B212BF0...` ← v19.0.2 bugfix-only rebuild (不 ship)
- `release/20260910-015012/aniswitch.nro` 33,581,404 B SHA `69f11e09...` ← v19.0.3 DNS fallback (不 ship)
- `release/20260910-015817/aniswitch.nro` 33,585,500 B SHA `64943949...` ← v19.0.4 case B' 激活 (不 ship)

### Source
- `src/net/mbedtls_direct.hpp` 6,254 B (新)
- `src/net/mbedtls_direct.cpp` 15,154 B (新)
- `src/net/http.cpp` 改 case A 修复 + case B' skeleton + case B' 激活
- `src/net/http.hpp` 改 DirectFetchResult + USE_DIRECT_TLS + directFetch 声明
- `platform/switch/switch_wrapper.c` 改 aniswitchResolveHost 3 nameserver fallback
- `CMakeLists.txt` +`src/net/mbedtls_direct.cpp` to ANISWITCH_SOURCES

### Docs
- `docs/v19.0.2-playbook.md` 4 case 决策树
- `docs/v19.0.2-case-A.patch` 138 行 (自包含 + NOTE block)
- `docs/v19.0.2-mbedtls-direct.hpp` case B' API 草案
- `docs/v19-network-p0-status-20260910.md` ← **本文件**
- `AGENTS.md` v19.0.2 + v19.0.3 + v19.0.4 三行
- `C:\Users\L\.minimax\agents\mavis\memory\MEMORY.md` 4 段 (v19.0.2 + bugfix + v19.0.3 + v19.0.4) 2217 行

### Build logs
- `build_logs/build-v19.0.2.log` 27,436 B (case A ship)
- `build_logs/build-v19.0.2-bugfix-20260910-014111.log` 22,514 B (bugfix-only)
- `build_logs/build-v19.0.3-prep-dns-20260910-014654.log` 18,744 B (DNS fallback 第一次 build 失败)
- `build_logs/build-v19.0.3-prep-dns-fix2-20260910-014815.log` (DNS fallback 第二次 build 成功)

### User 实机 log (input)
- `C:\Users\L\.minimax\v2\assets\2026\09\10\02-48-44-523-asset_20260910-024844-523_3d52dad035a6_8f2893f5-startup.log` 180 行

---

## 12. 工具链 / build 坑备忘

- **devkitpro/devkita64:20260219** (从 20251117 升)
- **CMake 4.4.3**
- **PowerShell 5.1** (not bash)
- 写 `$ErrorActionPreference = 'Stop'` + 检查 `$LASTEXITCODE` 显式
- docker mount `E:\AI\ani-switch:/proj` (之前 `/src` 已弃用)
- cpr 1.10.5 vendored curl 8.4.0 + mbedTLS 2.28.10
- **5 个独立 build 阻塞 fix** (v19.0.1 ship 时撞):
  1. cpr FetchContent 拉 curl-8.4.0.tar.xz GitHub 限流 → docker 内下 + `FetchContent_Declare(SOURCE_DIR)` 跳过
  2. zlib-ng 2.0.6 git clone 限流 → 同上
  3. mongoose 7.7 tarball 限流 → 同上
  4. curl 8.4.0 `try_run()` cross-compile fail → CMakeLists.txt PLATFORM_SWITCH 块预设 cache vars (`HAVE_H_ERRNO_ASSIGNABLE_EXITCODE=0` 等)
  5. cpr `SetDebugCallback` 编译错 → `cpr::DebugCallback{}` 包装 lambda + lambda 收 3 参数 `(InfoType, std::string, intptr_t)`
- **C `htonl()` 不是 constant expression** — v19.0.3 build 撞, 改预编译 LE-encoded 网络字节序值

---

## 13. 下次 session 入口

1. **user 拷 v19.0.3 (dist v15 zip) 实机 + 贴 startup.log** ← 当前阻塞点
2. **case A 通了**: 跳过 v19.0.3/v19.0.4, 走下个 sprint (v19.0.5 = 收藏 v3 / 主题预览 / 邮箱登录 任挑)
3. **v19.0.3 case A 修复也通了**: 收手, 走下个 sprint
4. **v19.0.3 case A 修复也没通 + TCP:443 仍 timeout**:
   - 几乎肯定 port 443 DPI / ISP 干扰
   - 走方案 C (国内 VPS 反代)
   - 1-2 day ship: nginx stream + Switch 端 cpr::Proxies
   - 期间 case B' / mbedTLS direct 都救不了 (TCP 都连不上)
