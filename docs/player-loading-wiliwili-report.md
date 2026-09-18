# ani-switch 播放加载方式与进度条问题 — 完整总结

> 范围：在线 HLS 番剧播放为何「只播约 1 分 12 秒 / 进度条对不齐」、wiliwili 如何做长视频、我们改了什么、实验结论与当前架构。
>
> 仓库：`E:\AI\ani-switch`  
> 设计参照：`docs/ui-redesign-v22/DESIGN.md`、`tools/wiliwili-src/wiliwili-1.6.0`（用户提供的 wiliwili 源码）  
> 文档日期：2026-09-18

---

## 1. 问题现象

| 现象 | 用户描述 |
|------|----------|
| 播放过短 | 在线番剧经常只播到约 **0:00 / 1:12**，或「很短就暂停」 |
| 进度条不对 | 无缝边下边播时，进度条总长随下载「变长」，**对不上整集时长** |
| 分片跳变 | 若用「多段 loadfile append」，段与段之间可能卡顿/黑一下 |
| 期望 | **无感零跳变** + 进度条长度与真实片长一致 + 边看边加载 |

---

## 2. 根因分析

### 2.1 为什么会出现 1:12（历史实现）

早期为批量试播、防止 Switch **256MB 堆**耗尽，在 `player_activity.cpp` 中写死了 HLS 安全帽：

| 限制 | 旧值 | 效果 |
|------|------|------|
| 最多下载分片 | **15 段** | 例如芙莉莲整集 373 段 → 只下前 15 段 |
| 总大小上限 | 80 MB | 15 段约 52MB，再下会被砍 |
| 单段上限 | 8 MB | 偶发大段被跳过 |

约 15 段 × 每段数秒 ≈ **72 秒 = 1 分 12 秒**。  
进度条显示 `00:00/1:12` 是「截断后本地文件总长」，播完即停——看起来像「源只有 1 分钟」或「播完就暂停」。

后续把上限提高到 **500 段 / 2 GiB / 单段 20MB**，并强制加载后 `AUTO_PLAY=true`，解决「整集下载后长度不对 / 加载完停住」。

### 2.2 为什么「分片 append」做不到无感零跳变

1. **结构**：每个 HLS `.ts` 作为独立 `loadfile` 条目，demuxer/decoder 在边界可能重置 → 短黑帧、音画微断。  
2. **硬 bug**：`mpv_core.cpp` 在 `MPV_EVENT_FILE_LOADED` 时无条件 `playlist-clear`（wiliwili 本意是清 **backup CDN URL**）。  
   在 progressive append 场景下：**首段 load 成功后会清掉已 append 的后续段** → 播放追上下载时 playlist 空 → 卡住/提前 EOF。

### 2.3 为什么进度条分母会「越来越长」

无缝流（`ani://`）边下边播时：

- mpv/ffmpeg 对 **尚未写入的字节** 只能算出「当前已 demux 到的时长」；
- `duration` 属性会从几秒涨到几十秒，直到接近整集；
- 若进度条公式是 `playback_time / mpv.duration`，就会出现 **总长不断变化、对不上真实片长**。

---

## 3. wiliwili 是怎么处理长视频的

对照用户提供的源码：`tools/wiliwili-src/wiliwili-1.6.0`（GitHub xfangfang/wiliwili 1.6.0）。

### 3.1 加载方式（媒体如何进 mpv）

**核心结论：wiliwili 不做「HLS 分片下载再 playlist-append」。**

| 源类型 | 做法 | 代码位置 |
|--------|------|----------|
| **DASH**（B 站默认） | `video->setUrl(v.base_url, start, end, audios)` — **一次 loadfile 视频 URL**，音轨走 extra `audio-file=` | `wiliwili/source/activity/player_base_activity.cpp` `onVideoPlayUrl` |
| **FLV 单段** | `setUrl(durl[0].url)` | 同上 |
| **FLV 多段** | 构造 **`edl://`** + 每段 `length=`，**一条 demuxer 时间轴** | `wiliwili/source/view/video_view.cpp` `setUrl(vector<EDLUrl>)` |
| CDN 备用 | `setBackupUrl` → `loadfile … append`；主文件 FILE_LOADED 后 `playlist-clear` **只清备份** | `video_view.cpp` / `mpv_core.cpp` |
| 直播 m3u8 | **直接** `video->setUrl(liveData.url)` — mpv 网络 HLS | TsVitch `live_player_activity.cpp` |

网络参数（wiliwili `genExtraUrlParam`）：

```text
referrer="https://www.bilibili.com",network-timeout=5
http-proxy="<ProgramConfig::getProxy()>"   // 可选
start=<sec>, end=<sec>
audio-file="<url>"                         // DASH 音轨
```

**Switch 特性**（wiliwili 构建脚本 / wiki）：

- ffmpeg **启用** protocol：`file,http,tcp,udp,rtmp,hls,https,tls,ftp,rtp,crypto,httpproxy`
- mpv 可 **直连网络**；Switch 上 `vd-lavc-threads=4`，硬解 `hwdec=auto`
- 缓冲：`demuxer-max-bytes`（内存）；**不把长视频落盘成一堆分片再拼**
- FAQ：播放「不会在本地留下完整媒体缓存」

### 3.2 进度条如何对齐「真实总时长」（REAL_DURATION）

wiliwili **不用**「mpv 当前 demuxer 时长」作为进度条唯一分母，而是：

```cpp
// player_base_activity.cpp
int time_sec = result.timelength / 1000;  // B 站 API 毫秒时长
APP_E->fire(VideoView::REAL_DURATION, (void*)&time_sec);

// video_view.hpp 注释
// 当 real_duration > 0 时，播放器进度条的总时长以此为准，
// 而不是以视频的实际（mpv）时长为准

// video_view.cpp
this->real_duration = *(int*)data;
this->setDuration(wiliwili::sec2Time(real_duration));
this->setProgress(playback_time / real_duration);

// 拖动进度条
if (real_duration > 0)
    mpvCore->seek((float)real_duration * progress);
else
    mpvCore->seekPercent(progress);

// 读取总长
float getRealDuration() {
    return real_duration > 0 ? real_duration : mpvCore->duration;
}
```

**要点**：API/播放列表给的 **整集时长** 与 mpv 流式加载时的 **瞬时 duration** 是两套数；UI 以前者为准。

### 3.3 与 ani-switch 环境的差异

| 条件 | wiliwili | ani-switch |
|------|----------|------------|
| 网络 | 系统 DNS / 可选 http-proxy，mpv 可直连 | Clash fake-ip、DNS 不稳，**mpv 网络栈常失败** |
| 视频形态 | DASH / FLV / EDL（连续 URL 或时间轴） | animeko 源多为 **HLS m3u8 + 大量 .ts** |
| 历史决策 | 网络给 mpv | v19 起：**禁止 mpv 拉外网**，一律应用层下载 |

因此：**进度条逻辑可以照抄 wiliwili；媒体通路不能简单照抄。**

---

## 4. 我们的解决方案演进

### 4.1 版本脉络

| 版本 | 变更 | NRO SHA（节选） |
|------|------|-----------------|
| v60 | HLS 上限 15→500 段 / 2GB；setUrl 后强制 AUTO_PLAY | `9D832097…` |
| v61 | 分片缓冲 12 段后 `loadfile` + `append`（会跳变 + 被 playlist-clear 误伤） | `7614EE8D…` |
| **v62** | **无缝单流**：`mpv_stream_cb` + `ani://`；progressive 时 **禁止 playlist-clear** | `1F19FC57…` |
| v63 | Eden playtest：3 番 × 6 次快进/快退自动 seek 通过 | `D8E91492…` |
| **v64** | **进度条 REAL_DURATION**：m3u8 EXTINF 求和 → `setRealDuration`；seek 等缓冲 | `2A867219…` |
| **v65** | **实验开关**：seamless / **mpv-direct（wiliwili 式）**；设置可切换 | `C5BB3498…`（最新） |

### 4.2 无缝 HLS（默认路径）— 架构

对齐 wiliwili「**一条连续 demuxer 时间轴**」的精神，但数据来源改为 **我们自己的 HTTP 栈**：

```text
sources.json / 在线解析得到 m3u8
        │
        ▼
HTTP::prepareFetchSession 按序下载 .ts 分片
        │  （Clash/DoH/DNS 由 ani-switch 处理，mpv 不碰外网）
        ▼
落盘增长文件  sdmc:/switch/aniswitch/vc_<hash>.ts
        │
        ▼
自定义协议 ani://<sessionId>   ← mpv_stream_cb_add_ro
        │  read() 阻塞等待更多字节；下载未完不报 EOF
        ▼
mpv loadfile ani://1   （单流，无 playlist 边界）
```

关键文件：

| 文件 | 职责 |
|------|------|
| `src/player/seamless_hls.hpp/.cpp` | 会话、下载线程、`ani://` open/read/seek/size、EXTINF 解析 |
| `src/player/mpv_core.cpp` | 注册协议；`ALLOW_NETWORK_URL`；FILE_LOADED 时按模式决定是否 `playlist-clear` |
| `src/ui/activity/player_activity.cpp` | 解析 m3u8 → 选 seamless 或 mpv-direct → REAL_DURATION |
| `src/player/tsvitch_video_view.cpp` | `setRealDuration`；进度条分母；seek 按真实时长 |

日志前缀：

- `SHLS:` — 无缝流（register / download / open ok / done）
- `player: hls playlist duration=N sec mode=seamless|mpv-direct`
- `PLAYTEST:` — 自动播放 + seek 脚本
- `PERF:` — 核心数/内存/加载策略（`no-FastLoad`）

### 4.3 进度条对齐（v64，对齐 wiliwili）

| 步骤 | 实现 |
|------|------|
| 总时长 | `SeamlessHls::parsePlaylistDurationSec(m3u8)` 对所有 `#EXTINF:` 数值求和 |
| 写入 UI | `video_->setRealDuration(playlistDur)`（例如芙莉莲 **1586 秒 ≈ 26:26**） |
| 进度公式 | `setProgress(playback_time / real_duration)`，**不用** mpv 短 duration |
| 右侧时长 | 固定显示 `mm:ss` 整集时长 |
| 拖动 seek | `seek(real_duration × 滑块比例)` |
| 前进超出已下载 | `stream_cb` seek **等待写入线程**（约 60s），再失败则钳制到已下载末尾 |
| size 估计 | 未下完时：`已下字节/已下段数 × 总段数` 供 demuxer 参考 |

**验收日志示例**：

```text
player: hls playlist duration=1586 sec mode=seamless
SHLS: download start id=1 segs=373
player: hls seamless start playback
SHLS: open ok id=1
PLAYTEST: DONE ep=1227087 seeks=6 …
```

### 4.4 mpv-direct 实验（v65，对齐 wiliwili 加载方式）

**设置项**：`playerStreamMode`

| 值 | 名称 | 行为 |
|----|------|------|
| 0（默认） | **seamless** | 应用下载 + `ani://`；`INMEMORY_CACHE=0`；`ALLOW_NETWORK_URL=false` |
| 1 | **mpv-direct** | `loadfile` 网络 m3u8；extra 含 referrer / network-timeout / **http-proxy**；`INMEMORY_CACHE=20MB`；`ALLOW_NETWORK_URL=true` |

**UI**：设置 → 播放 →「在线加载(无缝/直连)」。

**Autotour**：

- `playtest` — 默认 seamless + 自动 seek  
- `playtestd` — 强制 mpv-direct 后同样 playtest  

**mpv-direct 仍先用我们的 HTTP 拉一次 m3u8**（为了 REAL_DURATION 与失败提示），再 `loadfile` **网络 URL** 交给 mpv。

---

## 5. 实验记录（Eden 模模拟器）

### 5.1 环境限制

- Eden **无法向游戏窗口注入手柄**（日志 `processInput skipped`）。
- 导航/播放/seek 靠 **autotour + 应用内 `Intent` / `MPVCore::seekRelative`**。
- `startup.log` 需 **`ANISWITCH_SWITCH_DEBUG=ON`**（`docker_build_simple.sh` 默认 ON）。
- 主机脚本：`scripts/eden_stress_run.ps1`、`eden_playtest.ps1`、`eden_experiment_direct*.ps1`。

### 5.2 无缝流 + 快进快退（v63，3 番）

| 轮次 | 源 | 结果 |
|------|-----|------|
| 芙莉莲 | ep `1227087` | `SHLS register rc=0`，file loaded，6 次 seek 完成 |
| 间谍 S2 | ep `1124319` | 同上，seek 后时间点随缓冲推进 |
| 辉夜 | ep `1182322` | 同上 |

说明：seek 早期受「已缓冲范围」限制属预期；整集下完后应可全程拖动。

### 5.3 进度条时长（v64）

- 日志：`playlist duration=1586 sec`（整集）。
- UI：`setRealDuration(1586)` → 进度条分母 / 右侧时长应对齐 **~26 分钟**，而不是 1:12。
- 自动脚本里 `MPVCore.duration` 在大 TS 刚打开时可能仍为 0（demuxer 探针未完成），**以 OSD 上的 real_duration 为准**。

### 5.4 mpv-direct vs seamless（v65，A/B）

**A — mpv-direct（playtestd）**

```text
AUTOTOUR: force mpv-direct stream mode
player: mpv-direct network loadfile
VideoView: setUrl net=1 allowNet=1
player: setUrl … allowNet=1 path=https://c1.rrcdnbf5.com/.../index.m3u8
player: setUrl NETWORK allowed (mpv-direct experiment)
PLAYTEST: wait t=0…65  path=  stopped=1
（无 file loaded、无播放时间推进）
```

**结论**：开关与 loadfile 路径 **工作正常**；在本机网络下 **mpv 网络栈无法拉起该 CDN**（与 v19 Clash/DNS 问题一致）。

**B — seamless（playtest，对照）**

```text
PERF: stream mode=seamless allow_net=0
player: hls playlist duration=1586 sec mode=seamless
SHLS: download start … segs=373
player: hls seamless start playback
SHLS: open ok id=1
PLAYTEST: DONE seeks=6
```

**结论**：默认路径可用；进度条总长可按 1586s 对齐。

---

## 6. 两种加载方式对照（总结表）

| 维度 | wiliwili | ani-switch seamless（默认） | ani-switch mpv-direct（实验） |
|------|----------|------------------------------|-------------------------------|
| mpv 输入 | 网络 DASH/FLV/EDL URL | **`ani://` 本地单流** | 网络 m3u8 URL |
| 谁负责下载 | mpv/ffmpeg | **ani HTTP 栈** | mpv/ffmpeg |
| 分片边界 | 无（连续流/EDL） | 无（连续 TS 字节流） | 无（HLS demuxer） |
| 进度条总长 | API `timelength` | **m3u8 EXTINF 求和** | 同左（仍解析 m3u8） |
| 边下边播 | 网络 demuxer cache | **阻塞 read + 落盘增长文件** | mpv 网络 cache（若通） |
| 本环境实测 | （未在本机测 wiliwili） | **可通过** | **播不起来** |
| 风险 | 依赖 mpv 网络/代理 | SD 空间、seek 等缓冲 | DNS/Clash/TLS |

**裁决**：

1. **进度条 / REAL_DURATION**：与 wiliwili **同一套模型**，已落地。  
2. **媒体通路**：不能在本环境照搬 wiliwili 直连；**默认必须 seamless**。  
3. **mpv-direct** 保留为设置项实验：仅在实机网络/代理对 mpv 有效时启用。

---

## 7. 关键实现细节备忘

### 7.1 `playlist-clear` 语义（必须分清）

| 场景 | 是否 clear |
|------|------------|
| 普通单文件 / 网络主 URL 加载成功 | clear（丢掉 backup_url，wiliwili 原意） |
| progressive HLS append（旧方案） | **禁止** clear |
| seamless `ani://` | **禁止** clear（`KEEP_PLAYLIST` / `HLS_PROGRESSIVE`） |

### 7.2 FastLoad 禁令

libnx：`ApmCpuBoostMode_FastLoad` = 提升 CPU **同时 GPU 降到最低**。  
播放/解码路径 **禁止 FastLoad**；`perf::` 只记录 `cores` / `coreMask` / `procMem`，GPU 记 `gpu=n/a`。

Eden 曾测到：`cores=4 coreMask=0xf perfMode=Boost`，`hwdec=on`。

### 7.3 自定义协议与 mpv

- 头文件：toolchain `portlibs/switch/include/mpv/stream_cb.h`（**无需重编 mpv**）。
- `mpv_stream_cb_add_ro(mpv, "ani", …)` 在 `mpvInitialize` 之后注册。
- `read`：**下载未完成时阻塞等待**，禁止提前 `return 0`（0=EOF）。
- `seek`：允许范围内随机跳；超出已下载则等待或钳制。
- 回调内 **禁止** 再调同一 `mpv_handle` 的 libmpv API（防死锁）。

### 7.4 日志关键字（实机排障）

| 前缀 | 含义 |
|------|------|
| `SHLS:` | 无缝流协议/下载 |
| `PLAYTEST:` | 自动播放+seek |
| `PERF:` | 性能/加载策略/内存 |
| `RESOLVE:` / `HTTPSource:` | 选集与 sources.json |
| `player: hls playlist duration=` | 解析到的整集秒数 |
| `player: setUrl … allowNet=` | 是否走网络直连 |

### 7.5 数据源

`sdmc:/switch/aniswitch/sources.json`：key 为 **Bangumi 集 ID**（非条目 ID），例如：

- `1227087` 芙莉莲 EP1  
- `1124319` 间谍过家家 S2  
- `1182322` 辉夜 等  

`bySubject` 可作 subjectId 回退。Provider 对「无映射」应 **软失败**，避免整条 resolve 被 error 炸掉。

---

## 8. 实机验收清单

1. **时长**：打开一集 → OSD 右侧约 **24–26 分钟**（如 26:26），不是 1:12。  
2. **进度条**：从 0 平滑涨向整集比例；总长不随下载「跳变」。  
3. **跳变**：全程无明显段边界黑帧/卡顿（seamless）。  
4. **seek**：前半段可即时跳；拖到未缓冲处会短暂缓冲再播。  
5. **设置开关**：  
   - seamless：日志 `mode=seamless` + `SHLS:`  
   - mpv-direct：`allowNet=1` + `NETWORK allowed`；若一直 `stopped=1` → 网络不支持，改回 seamless。  
6. **代理**：设置页填写 Clash Allow LAN 后，再测 mpv-direct 是否变化。  
7. **LED**：仅播放加载时指示（模拟器上 hidsys 常 `rc=0x5f59` 失败属正常）。  
8. **回归**：本地 `videos/*.mp4`、选集、sources.json 解析仍正常。

---

## 9. 当前交付物（截至本文）

| 文件 | 说明 | SHA256 |
|------|------|--------|
| `release/20260918-191525/aniswitch.nro` | 含 seamless + REAL_DURATION + mpv-direct 开关 | `C5BB3498724FDF9B00AFF0BEAF6C5921B42CAB3FE63CC78CC238E0BFA64539F0` |
| `dist/ani-switch-sd-v65.zip` | SD 卡包（NRO + sources.json 等） | `8A28F9D4F0394C28AFAC47B7CB292D1E763D4B0C2050BCA17D6F3BC54F368E63` |

**默认推荐配置**：`playerStreamMode = seamless`。

复现命令示例：

```powershell
# 无缝 + 自动 seek
powershell -File scripts\eden_playtest.ps1

# mpv 直连实验
powershell -File scripts\eden_stress_run.ps1 -Mode playtestd -OnlinePair '1227087 400602' -Force
```

---

## 10. 未完成 / 后续可做

| 项 | 说明 |
|----|------|
| 实机完整一集 | Eden 已测 seek/时长解析；实机需连续看 ≥20 分钟确认零跳变与进度条 |
| mpv-direct 实机矩阵 | 不同 Clash/代理/热点下是否能直连；失败则设置项保留但文档标明「默认勿用」 |
| 边下边播进度条 | real_duration 已对齐；若还要「已缓冲范围」可视化，可加 cache 区间条（wiliwili profile 有 demuxer-cache-state） |
| 本地 HTTP 方案 | 若未来既要 mpv 网络模型又要稳：mongoose `127.0.0.1` 转发（调研备选，未实现） |
| 选集/网盘源扩展 | sources.json 覆盖更多集 ID；web selector 实机稳定性 |
| 焦点/OSD/主题 | UI v22 其它项与播放器 OSD P3 不在本文范围 |

---

## 11. 一句话结论

- **1:12** = 旧 HLS「只下 15 段」的试播帽子，不是源只有 1 分钟。  
- **进度条对不齐** = 分母误用了「已缓冲时长」；现已改为 wiliwili 式 **REAL_DURATION（m3u8 整集时长）**。  
- **零跳变** = 不能 playlist 拼段；应对齐 wiliwili 的「**一条连续流**」——在本机网络下用 **应用层下载 + `ani://` 单流** 实现。  
- **wiliwili 的 mpv 直连** 在本环境 **实验失败**（Clash/DNS）；已做成可切换实验项，**默认仍走 seamless**。

---

*本文由 ani-switch 会话内实验与源码对照整理；wiliwili 源码来自用户提供的 `tools/wiliwili-1.6.0.zip`。*
