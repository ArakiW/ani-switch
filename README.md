# ani-switch

**Nintendo Switch 上的 Bangumi（番组计划）/ 番剧客户端**  
本仓库是 [open-ani/animeko](https://github.com/open-ani/animeko) 的 Switch 移植（**从零重写**，不是 source recompile）。UI 与平台层复用 [xfangfang/wiliwili](https://github.com/xfangfang/wiliwili) 的 borealis / mpv 等组件。

- 画布：1280×720（docked / handheld 同逻辑分辨率）
- 许可证：**AGPL-3.0**（vendored 自 wiliwili 的文件保留其 SPDX 头）
- 远程：https://github.com/ArakiW/ani-switch

---

## 功能概览（v22 线）

| 模块 | 说明 |
|------|------|
| 首页 | 探索 / 每日放送 / 推荐；顶栏 + Tab + 底部 HUD；海报轨 / 列表 |
| 搜索 / 详情 | Bangumi + Ani 数据面；评分、收藏、剧集、角色 |
| 在线播放 | 选源 → 下载/直连 → TsVitch OSD 播放器（mpv） |
| **无缝 HLS（默认）** | 应用层下载分片 → `ani://` 单流喂 mpv，**零分片跳变**；进度条用 **m3u8 整集时长**（对齐 wiliwili `REAL_DURATION`） |
| mpv 直连（实验） | 设置可切换：mpv `loadfile` 网络 m3u8 + 可选 `http-proxy`（本环境常因 Clash/DNS 失败） |
| 本地视频 | `sdmc:/switch/aniswitch/videos/`，**按动漫名/子文件夹分组** |
| 账号 | Bangumi PKCE / PAT、ani 邮箱 OTP；成功后自动存 token 并拉取账号信息 |
| 设置 | 硬解、弹幕、代理、主题、在线加载方式（seamless / mpv-direct）等 |
| 其它 | HOME 灯仅播放加载时指示、启动日志、首次启动输入登录码、autotour 压力/播放测试 |

播放器与加载方式的详细对照与实验结论见：

- [`docs/player-loading-wiliwili-report.md`](docs/player-loading-wiliwili-report.md)

UI 设计系统见：

- [`docs/ui-redesign-v22/DESIGN.md`](docs/ui-redesign-v22/DESIGN.md)

---

## 构建

### Switch（推荐 Docker）

```bash
docker run --rm -v "$PWD:/proj" -w /proj \
  -e BUILD_JOBS=8 -e ANISWITCH_SWITCH_DEBUG=ON \
  devkitpro/devkita64:20260219 bash scripts/docker_build_simple.sh
```

产物：`cmake-build-switch/aniswitch.nro`，并复制到 `release/<时间戳>/`。

增量编译可用：

```bash
docker run --rm -v "$PWD:/proj" -w /proj \
  -e BUILD_JOBS=8 -e ANISWITCH_SWITCH_DEBUG=ON \
  devkitpro/devkita64:20260219 bash scripts/docker_inc_nro.sh
```

### 依赖（`third_party/`）

第三方库从 wiliwili `library/` 复用并 **vendored 进本仓库目录**（见 `.gitmodules` 中的上游 URL 注释，**不是** git submodule）。构建还需要 devkitPro 上的 **switch-libmpv / averne ffmpeg** 等预编译包（与 wiliwili Switch 构建相同来源）。

> 本 Git 远程 **不包含** 完整 `third_party/` / `tools/` 二进制与 NRO 发行包；请按 `AGENTS.md` / wiliwili 文档准备依赖后再编译。

---

## 部署到 SD 卡

将下列内容放到 Switch SD：

```text
sdmc:/switch/aniswitch/aniswitch.nro
sdmc:/switch/aniswitch/sources.json      # 在线源（Bangumi 集 ID → m3u8）
sdmc:/switch/aniswitch/ca-bundle.crt    # 可选 TLS CA
sdmc:/switch/aniswitch/videos/           # 可选本地视频
```

hbmenu 启动 `aniswitch`。诊断日志：`sdmc:/switch/aniswitch/startup.log`（需 `ANISWITCH_SWITCH_DEBUG=ON` 构建）。

### 网络说明（重要）

- Switch 上 **mpv 默认不直连外网**（Clash fake-ip / DNS 问题）。
- 在线番剧：**应用层 HTTP 下载 → 本地单流播放（seamless）**。
- 可在 **设置 → 网络** 填写 PC 上 Clash Allow LAN 代理，例如：`http://192.168.x.x:7897`。
- 设置 → 播放 → **在线加载**：`seamless`（默认）/ `mpv-direct`（实验，需代理对 mpv 有效）。

### 在线源 `sources.json` 示例

```json
{
  "1227087": [
    {
      "label": "葬送的芙莉莲 EP1",
      "url": "https://example.com/frieren/ep01/index.m3u8",
      "priority": 10
    }
  ]
}
```

Key 为 **Bangumi 集 episode id**（不是条目 subject id）。

---

## 自动化测试（Eden 模拟器）

Eden 无法注入手柄时，用 autotour + 脚本：

| 脚本 | 用途 |
|------|------|
| `scripts/eden_stress_run.ps1` | 部署 NRO、写 autotour、轮询 `startup.log`、截图 |
| `scripts/eden_playtest.ps1` | 多番剧播放 + 程序化快进/快退 |
| `scripts/eden_experiment_direct.ps1` | seamless vs mpv-direct 对照 |

Autotour 模式（`sdmc:/switch/aniswitch/autotour`）：

```text
playtest
1227087 400602
```

第二行为 `<episodeId> <subjectId>`。其它模式：`settings` / `online` / `stress50` / `playtestd` 等。

---

## 目录结构

```text
docs/                 设计与报告
platform/switch/      libnx 入口 / DNS / startup.log
src/core/             业务核心（解析、本地扫描、web selector…）
src/net/              HTTP、Bangumi、Ani、代理
src/player/           mpv / TsVitch / seamless HLS
src/ui/               Activity、chrome、theme、HUD
resources/            字体、图标、XML 布局
scripts/              Docker 构建与 Eden 测试
third_party/          vendored 第三方（构建依赖）
```

---

## 许可证

- 本仓库：**AGPL-3.0**
- 自 wiliwili / borealis / TsVitch 等复用的代码保留原 SPDX 与许可证声明

---

## 相关链接

- Animeko（上游业务参考）：https://github.com/open-ani/animeko  
- wiliwili（Switch UI/播放参考）：https://github.com/xfangfang/wiliwili  
- Bangumi：https://bgm.tv  
