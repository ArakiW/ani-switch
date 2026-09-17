# ani-switch UI Redesign v22 — Switch 10-foot Design System

> 目标：把当前「桌面侧栏 + 文本行」UI 重做成 Nintendo Switch / 客厅 10-foot 可用的海报驱动体验。  
> 锚点：**Netflix TV** 内容形态 + **Nintendo eShop** 顶栏/键位提示 + Animeko 紫品牌色。  
> 画布：**1280×720**（docked / handheld 同逻辑尺寸）。  
> 状态：设计定稿（已过 Explore / Architect / Critic 三路审查）。

---

## 0. 执行摘要（先读这个）

### 0.1 现状一句话

主壳是 280px 左侧栏 + 64px 文本行 + 11–18px 字号，封面在 `SubjectCell` 被禁用——这是**手机/桌面尺度**，不是 Switch。

### 0.2 Critic 硬约束（必须遵守，否则实现会踩坑）

| 约束 | 证据 | 设计应对 |
|---|---|---|
| borealis `ScrollingFrame` **仅垂直**；横向轨用 **`HScrollingFrame`**（已确认存在：`contentOffsetX`） | `scrolling_frame.hpp` / `h_scrolling_frame.hpp` | P1 使用 `brls::HScrollingFrame` + 内容 ROW `Box`；padding 设在 content 上（HScrollingFrame 自身 setPadding 会 fatal） |
| `View` **无 `setScale`** | `view.hpp` 仅有 alpha/highlight | **交付焦点方案 = 3px `focus` 描边 + 未焦点 alpha 0.82 + 外扩 padding**；scale 1.08 仅作 P3 自绘 transform 备选，不进验收主路径 |
| `TabFrame` 多页 **实机 native crash** | Settings 已退回 flat list | Settings **永久 flat**，只抬行高/字号，禁止再引入 TabFrame |
| 封面开关不一致 | `SubjectCell` 禁用 vs `SubjectActivity` 仍 load | **P0 统一 `ANISWITCH_COVER_ENABLED` 开关**；未通过稳定性实验前两侧同关 |
| Player mpv init 曾闪退 | v20.4 breadcrumb | **Player 实机不闪退 > OSD 视觉**；OSD 排 P3 |
| 无页面方向动画 API | borealis Activity 切换 | 页面过渡固定 **淡入 160ms**，不做水平滑入 |
| 无 drop-shadow/glow | deko3d + borealis | 焦点「光晕」= 多一层半透明圆角矩形描边，非 blur |

### 0.3 真实实施顺序（修正版）

```
P0  封面单线程实验（3 次冷启）→ 全局字号 ≥16 → 焦点描边方案 → safe area 常量 → 根页 B 退出确认
P1  HorizontalScrollBox POC（1 轨）→ Home 去侧栏 + 顶 Tab + HUD（读 action map）
P2  Detail/Search/Collection 海报化 → VibrationHelper 焦点 tick → 焦点/滚动恢复
P3  Player OSD → 动效打磨 → Light 主题校对 → （可选）自绘 scale
```

---

## 1. 现状诊断

| 问题 | 现状 | Switch 目标 |
|---|---|---|
| IA | 顶栏 5 按钮 + 左 rail 280 + 内容 980 | 顶栏 + 顶 Tab，内容满宽 1184，**去侧栏** |
| 内容形态 | 64px 文本行，封面 44×44 且禁用 | 152×228 海报横轨 / 6 列网格 |
| 字号 | 11–18px 为主 | 可交互 ≥16，按钮/正文 ≥20，屏题 32 |
| 焦点 | borealis 默认 ring，无对比强化 | 3px `#A78BFA` 描边 + 未焦点降 alpha |
| 键位提示 | 无 | 常驻底部 HUD，**由 Activity action map 渲染** |
| 搜索 | 系统 IME 弹窗 | 保持系统键盘（swkbd），补历史 chips + 结果网格 |
| 设置 | flat（TabFrame 已 crash） | 保持 flat，行高 88，分区标题 |
| 播放器 | 一排文字 Button，无进度条 | 先保证不闪退，再做 OSD |

---

## 2. 设计原则

1. **手柄优先**：主路径零触摸依赖；Handheld 触摸是补充（如播放器 tap）。
2. **焦点三重冗余**：描边 + 透明度差 +（可选）尺寸差——不只靠颜色。
3. **封面驱动**：2:3 海报是主视觉；文字辅助。
4. **一屏一事**：首页只发现；账号/设置分层。
5. **廉价高级感**：实色阶梯 + 细描边；**禁止** blur / 多层阴影 / 玻璃拟态。
6. **稳定性优先于视觉**：封面与 mpv 未过实机前，不铺开海报轨 UI。

---

## 3. 调色板

### 3.1 Dark（默认 / Auto→Dark）

| Token | Hex | brls key（新增） | 用途 |
|---|---|---|---|
| `bg` | `#121018` | `ani_bg` | 全屏底（沿用现 `kChromeBg`） |
| `surface` | `#1C1A24` | `ani_surface` | 顶栏 / HUD / 面板 |
| `surface-elevated` | `#282436` | `ani_surface_elevated` | 卡片、行、弹层 |
| `surface-overlay` | `#0C0A12` | `ani_overlay` | Dialog 遮罩 / OSD 底 |
| `border-subtle` | `#FFFFFF1C` | `ani_border_subtle` | 1px 分隔 |
| `border-strong` | `#FFFFFF33` | `ani_border_strong` | 次按钮描边 |
| `ink` | `#F5F5F8` | `ani_ink` | 主文字 |
| `ink-secondary` | `#B9B9C3` | `ani_ink_secondary` | 次文字 |
| `ink-muted` | `#828291` | `ani_ink_muted` | 弱提示（≥16px 才可用） |
| `ink-disabled` | `#5A5A68` | `ani_ink_disabled` | 禁用 |
| `accent` | `#4F378B` | `ani_accent` | 主按钮 / 选中底（Animeko seed） |
| `accent-deep` | `#372864` | `ani_accent_deep` | 按压态 |
| `accent-soft` | `#938BDC` | `ani_accent_soft` | 次强调 |
| `accent-bright` | `#A78BFA` | `ani_accent_bright` | 焦点环、选中指示 |
| `focus-ring` | `#A78BFA` | `ani_focus_ring` | 焦点描边（与 bright 同值，语义独立） |
| `rating` | `#FFC850` | `ani_rating` | 评分 |
| `success` | `#3DDC97` | `ani_success` | 同步/完成 |
| `error` | `#FF6B78` | `ani_error` | 错误/删除 |
| `warning` | `#FFC850` | `ani_warning` | 警告 |
| `info` | `#6BA8FF` | `ani_info` | 信息 |
| `tag-genre` | `#3C5078` | 已有 `ani_tag_genre` | 类型 chip |
| `tag-cast` | `#503C64` | 已有 `ani_tag_cast` | 制作 chip |
| `tag-meta` | `#326E50` | 已有 `ani_tag_meta` | 元信息 chip |

**兼容**：旧 key `ani_card_bg` / `ani_card_row` / `ani_card_highlight` / `ani_text_*` 保留为别名，避免一次改爆 fragment。

### 3.2 Light（P3）

| Token | Hex |
|---|---|
| `bg` | `#F7F7FA` |
| `surface` | `#FFFFFF` |
| `surface-elevated` | `#F0F0F5` |
| `ink` | `#141418` |
| `ink-secondary` | `#50505A` |
| `accent` | `#4F378B` |
| `focus-ring` | `#5B3FA8` |

`ThemeChoice::Auto` → Dark（无系统色板钩子，与 v17 一致）。

---

## 4. 排版

字体：`Noto Sans CJK SC` / `Source Han Sans SC`；字重 Regular 400 + Bold 700（不引第三字重）。

| Token | Size | Weight | LH | 用途 |
|---|---:|---|---:|---|
| `type-display` | 36 | 700 | 1.20 | Hero 片名、欢迎 |
| `type-h1` | 32 | 700 | 1.25 | 屏标题、详情主名 |
| `type-h2` | 24 | 700 | 1.30 | Rail 标题、Dialog 题、Tab |
| `type-h3` | 20 | 700 | 1.35 | 区块题、按钮字、列表主文 |
| `type-body` | 20 | 400 | 1.40 | 简介、说明正文 |
| `type-card` | 18 | 700 | 1.25 | 海报卡标题（单行截断） |
| `type-caption` | 16 | 400 | 1.35 | 副标题、meta、HUD |
| `type-micro` | 14 | 400 | 1.30 | 评分旁注（**绝对下限**，非交互） |

**硬规则**

- 可交互文字 ≥ **16**；按钮/Tab ≥ **20**。
- 现状 10–13px 全部作废。
- 中文正文 LH **1.40**。
- 卡标题单行 `…`；详情标题可 2 行。

---

## 5. 布局网格

### 5.1 画布与安全区

| 项 | 值 |
|---|---|
| 逻辑分辨率 | 1280×720 |
| `margin-x` | **48** |
| `margin-top` | **40** |
| `margin-bottom` | **40** |
| 内容宽 | **1184** |
| HUD 高 | **56**（常驻时占底部，内容 padding-bottom 至少 72） |
| 顶栏高 | **72** |
| Tab 高 | **56** |

安全区常量收进 `theme.hpp` / `kLayoutSafe*`，禁止各 Activity 再散落 `setWidth(1280)`。

### 5.2 主壳（替换左 rail）

```
┌──────────────────────────────────────────────────────┐
│ Header 72   ani-switch          搜索 · 设置 · 账号     │
├──────────────────────────────────────────────────────┤
│ TabBar 56   探索 | 每日放送 | 推荐                      │
├──────────────────────────────────────────────────────┤
│ Content (vertical ScrollingFrame, pad 0 48 40 48)    │
│   Rail 1  标题                                       │
│   [==== HorizontalScrollBox 海报轨 ====]              │
│   Rail 2 …                                           │
├──────────────────────────────────────────────────────┤
│ HUD 56   A 确认  B 返回  …（按 action map）            │
└──────────────────────────────────────────────────────┘
```

- **L/R** 在主壳切 Tab。
- Header 右侧收敛为：搜索 / 设置 / 账号（收藏、历史进 Tab 或「我的」）。
- **禁止** 280px 左侧栏。

### 5.3 海报 Rail

| Token | 值 |
|---|---|
| poster | **152 × 228**（严格 2:3） |
| rail-gap-x | **16** |
| rail-gap-y | **32** |
| rail 标题区 | 40（h2 + mb 12） |
| 焦点（交付版） | **3px `focus-ring` 描边**；卡容器外扩 **8–12px** 防描边裁切 |
| 未焦点 | 1px `border-subtle` + **alpha 0.82** |
| 一屏完整卡数 | ~7 + 右缘 peek |
| 滚动实现 | 自定义 `HorizontalScrollBox`：手写 `contentOffsetX`，焦点进入时平滑 scroll-into-view ≤200ms |

> scale 1.08 写在 P3 备选；**验收不以 scale 为准**。

### 5.4 网格（搜索结果 / 收藏）

- 6 列 × poster 152×228，列 gap 16，行 gap 24。
- 单元下方标题 `type-card` 18。

### 5.5 列表行（历史 / 设置 / 备用）

| 项 | 值 |
|---|---|
| 行高 | **88** |
| 缩略图 | 56×84 r6 |
| 主文 | `type-h3` 20 |
| 副文 | `type-caption` 16 |
| 焦点 | 左 4px `focus-ring` 竖条 + 底升到 `surface-elevated` |
| 行尾键位区 | 48px，放 A/Y/X 提示 |

### 5.6 详情顶区

| 项 | 值 |
|---|---|
| 封面 | **200×300** r8 |
| 主名 | `type-h1` 32（≤2 行） |
| 评分 | `type-h3` 20 `rating` |
| CTA | 播放 Primary 56h / 收藏 Secondary 56h |
| 简介 | `type-body`，默认 ≤6 行，Y 展开 |

### 5.7 圆角

海报/行/按钮 **8**；Dialog **12**；chip 全圆；缩略图 **6**。

---

## 6. 组件规格

### 6.1 Poster Card

| 状态 | 描边 | Alpha | 底 |
|---|---|---:|---|
| Unfocused | 1px `border-subtle` | 0.82 | 图或 `surface-elevated` |
| Focused | **3px `focus-ring`** | 1.00 | 同上 |
| Pressed | 3px + `accent-deep` 叠底 | 1.00 | |

- 评分角标：海报右下 `type-micro` 14 + `rating`。
- Rail 上下 padding ≥ **16**，避免描边被裁。
- 图片失败：`surface-elevated` 色块 + 居中「无封面」`type-micro`。

### 6.2 Button

| 变体 | 底 | 字 | 高 | pad-x |
|---|---|---|---:|---:|
| Primary | `accent` | `ink` 20 Bold | 56 | 28 |
| Secondary | `surface-elevated` + 1px `border-strong` | `ink` 20 | 56 | 28 |
| Ghost | 透明 | `accent-soft` | 48 | 20 |
| Danger | `surface` + `error` 描边/字 | `ink` | 56 | 28 |

焦点：3px `focus-ring`；禁用不可焦点。

### 6.3 Tab

- 未选中：`ink-muted` 20 Bold。
- 选中：`ink` + 底边 3px `accent-bright`。
- **焦点在 Tab 上**：底 `surface-elevated` + 焦点环——与「选中」分开表达。

### 6.4 HUD（底部键位条）

- 高 56，底 `surface` / `#121010E6`，顶 1px `border-subtle`。
- **必须由当前 Activity 的 `ActionMap` 生成**，禁止各屏写死 chip 文案。
- 格式：`[圆点 A] 确认   [B] 返回   …`
- 字 `type-caption` 16 `ink-secondary`。

**ActionMap 示例**

| Activity | A | B | X | Y | L/R |
|---|---|---|---|---|---|
| Home | 详情 | 退出确认 | — | 收藏 | 切 Tab |
| Subject | 播放/详情 | 返回 | 收藏 | 简介全文 | — |
| List/Search | 打开 | 返回 | 删除? | 收藏 | — |
| Player（OSD 关） | 唤起 OSD | 退出 | 暂停 | 弹幕 | 上/下集 |
| Player（OSD 开） | 确认 | 收起 OSD | 暂停 | 弹幕 | 上/下集 |
| Dialog | 默认钮 | 取消 | — | — | — |

> 现状 Player 已 `X=暂停`，必须进 map，禁止全局覆盖成「X=收藏」。

### 6.5 Dialog

- 宽 720，r12，底 `surface-elevated`，遮罩 `#0C0A12` α **0.82**（纯色，无 blur）。
- pad 32；题 `type-h2`；文 `type-body` ≤4 行可滚。
- 默认焦点：非破坏→确认；删除/清空→**取消**。

### 6.6 Toast

- 底边距 48，max-w 640，min-h 56，r8，`surface-elevated` + 1px border。
- 停留 2400ms（错误 3600ms）；不可焦点。

### 6.7 Skeleton / Empty / Error

- Skeleton 与真组件同尺寸，opacity 0.40↔0.70，1200ms。
- Empty：图 + `type-body` + 可焦点「重试」。
- Error：`error` 左条 + 文案 + `HTTP::proxyHint()` + 重试。

### 6.8 评分

- `8.4` `type-h3` 20 Bold `rating`；样本量 `type-micro` `ink-muted`。

---

## 7. 交互模型

### 7.1 全局

| 键 | 规则 |
|---|---|
| A | 确认 / 打开（播放器 OSD 关时**仅唤起 OSD**，防误暂停） |
| B | 返回；根页栈深==1 → **退出确认 Dialog**（现状仅 pop，需补） |
| X / Y | **上下文**，见 ActionMap |
| + | 全局菜单（设置/账号/退出）——可选 |
| L/R | 主壳切 Tab；播放器切集 |
| ZL/ZR | 播放器音量 |
| Stick/D-pad | 焦点移动 |

### 7.2 焦点规则

1. 空间最近邻，不按 DOM 绕圈。
2. Rail 横向到边不竖跳；竖向换 rail 尽量保持水平索引。
3. scroll-into-view 边距 24px，≤200ms。
4. 默认焦点 = **主内容第一项**（不是 Header）。
5. Dialog 陷阱焦点；B 恢复打开前焦点。
6. 返回二级页恢复原焦点与滚动位置。

### 7.3 Hold-Repeat

延迟 **400ms** → 间隔 **80ms**；仅用于焦点移动/seek/音量，不用于 A/B/X/Y。

### 7.4 触摸分层

- 播放器：保留现有 Tap 暂停。
- 海报轨：**不依赖** touch-scroll；手柄必须能完成全部浏览。
- 验收：docked 手柄全流程 + handheld 至少各一轮。

### 7.5 震动（接入现成 `VibrationHelper`）

| 事件 | 强度 |
|---|---|
| 焦点进入新卡 | tick 1（可关） |
| 收藏成功 | success 2 |
| 破坏性确认 | 轻 impact |

### 7.6 休眠 / 底座热插拔

回前台（applet focus）时：刷新 token、校验播放进度 checkpoint、必要时重拉当前 rail。

---

## 8. 动效（受 borealis 能力约束）

| 场景 | 参数 | 备注 |
|---|---|---|
| 焦点环 | opacity 0→1 **80ms** | 必做 |
| 未焦点 alpha | 0.82↔1.0 **120ms** | 与描边同步 |
| 按钮 | 可选 scale 1.04 **100ms** | 若无 API 则仅描边 |
| Tab 内容切换 | 淡入 **160ms** | 无横滑 |
| Activity | 淡入 **160ms** | **无方向滑入** |
| Dialog | 淡入+可选 0.96→1 **160ms** | 遮罩 120ms |
| OSD | 淡入 + y±12 **200ms** | 实色 scrim |
| Skeleton | 1200ms 呼吸 | |
| Rail scroll | ≤200ms ease-out | |

单元素 ≤200ms；无 spring；`reduceMotion` 设置预留（关 scale，只留描边/透明度）。

---

## 9. 屏幕清单

| # | 屏 | 结构要点 | 默认焦点 |
|---|---|---|---|
| 1 | **Home** | 顶 Tab（探索/每日/推荐）+ 多条海报轨；数据 Ani trends/schedule，失败回退 Bangumi | 第一 rail 第一卡 |
| 2 | **Search** | 全宽输入（A→系统键盘）+ 历史 chips + 6 列结果网格 | 输入框 |
| 3 | **Subject Detail** | 200×300 封面 + 信息 + CTA + 简介 + 剧集网格 + 关联 rail | 播放（无源则收藏） |
| 4 | **Episode** | 4 列大卡 280×158 或内嵌网格；已看/进度条 | 下一集未看 |
| 5 | **Player** | 先稳 mpv；P3 再 OSD：顶/底实色 scrim、进度条 6px、控制行 | 视频区（OSD 关） |
| 6 | **Collection** | 状态 chip + 海报网格；Y 改状态 | 第一卡 |
| 7 | **History** | 分组头 + 88 行 + 进度；X 删除需确认 | 第一可续播 |
| 8 | **Settings** | **Flat** 分区列表（播放/网络/外观/账号/关于/调试），行 88 | 第一项 |
| 9 | **Login** | 邮箱 OTP / Bangumi PKCE / PAT 分卡；QR ≥220 | 邮箱登录 |
| 10 | **Onboarding** | 3–4 步；大标题 36 | 主 CTA |

---

## 10. 无障碍

### 10.1 对比（对 `bg #121018`）

| 前景 | 约比 | 规则 |
|---|---:|---|
| `ink` | ~17:1 | 任意 |
| `ink-secondary` | ~9:1 | 任意 |
| `ink-muted` | ~4.6:1 | 仅 ≥16px |
| `focus-ring` | ~7:1 | +3px 实心 |
| `ink` on `accent` | ~8:1 | 主按钮 |

### 10.2 焦点可辨（≥3 项同时）

1. 3px `focus-ring`  
2. 未焦点 alpha 0.82  
3. 底色提升一级  
4. （可选）键位 hint / 震动 tick  

### 10.3 其它

- 焦点目标 ≥ 48×48。
- 错误必须有文字，不只红点。
- 长文可滚且焦点自动滚入。

---

## 11. borealis 实现映射

| 设计 | 实现 |
|---|---|
| 垂直内容 | `ScrollingFrame`（允许） |
| 海报横轨 | **自定义 `HorizontalScrollBox`** |
| 焦点环 | `onFocus/onLayout` 画 3px stroke / 子节点描边框 |
| 焦点降权 | `setAlpha(0.82)` |
| Scale | P3 自绘 transform；默认不做 |
| Dialog | `brls::Dialog` / `EditTextDialog`，默认钮按破坏性规则 |
| Toast | `Application::notify` 外观对齐 §6.6 |
| 大列表回收 | vendored `RecyclingGrid`，单元锁 152×228 |
| 封面 | 单线程 `ImageLoader`；**主线程** nvg 上传；失败占位 |
| 主题 | 扩展 `theme.hpp` token + 旧 key 别名 |
| 设置 | Flat list，**禁用 TabFrame** |
| 搜索输入 | `ImeManager::openForText`（系统键盘） |
| 震动 | `VibrationHelper` |
| 安全区 | `kLayoutSafeMarginX/Y` 等常量 |

---

## 12. P0 前置实验（设计验收门禁）

在任何「海报轨上线」UI 之前：

1. **封面稳定性**  
   - 统一开关；单线程下载；texture 仅主线程创建。  
   - 失败占位不塌陷布局。  
   - 实机 **3 次冷启** + 详情页进出无闪退。  
2. **字号批量**  
   - 全局扫描 `setFontSize`，交互路径抬到 ≥16/20。  
3. **焦点描边**  
   - 列表行与按钮 3px ring + alpha，实机 3m 可辨。  
4. **退出确认**  
   - 栈深==1 时 B → Dialog。  
5. **HorizontalScrollBox POC**  
   - 1 轨假数据，焦点左右 scroll-into-view 无裁切。

全绿才进 P1 主壳重构。

---

## 13. 验收清单

- [ ] 720p 下 3 米能指出当前焦点  
- [ ] 手柄可完成：浏览 → 详情 → 播放 → 收藏 → 设置 → 退出  
- [ ] 无 <16px 交互文字  
- [ ] HUD 与 ActionMap 一致（Player X=暂停不被覆盖）  
- [ ] 返回恢复焦点与滚动  
- [ ] 封面失败有色块占位  
- [ ] Settings 无 TabFrame  
- [ ] docked + handheld 各实机一轮  
- [ ] 列表焦点移动 ≥30fps，同屏 texture 可控  

---

## 14. Token / 数字速查

```text
bg #121018 · surface #1C1A24 · elevated #282436
ink #F5F5F8 · ink-2 #B9B9C3 · ink-3 #828291
accent #4F378B · bright/focus #A78BFA · rating #FFC850 · error #FF6B78

type-display 36/700
type-h1 32 · h2 24 · h3 20 · body 20 · card 18 · caption 16 · micro 14

margin-x 48 · margin-y 40 · header 72 · tab 56 · hud 56
poster 152×228 · gap 16/32 · row 88 · btn 56 · dialog-w 720
focus-stroke 3 · unfocus-alpha 0.82
motion-focus 80–120ms · page-fade 160ms · hold 400/80
```

---

## 15. 交付物与协作说明

| 文件 | 说明 |
|---|---|
| 本文件 | 设计系统 + 屏幕 + 实施顺序 |
| `screens/00-tokens-components.svg` | Token / 字号 / 组件板 |
| `screens/01-home-explore.svg` | 首页海报轨（焦点为描边方案示意；scale 仅视觉示意） |
| `screens/02-subject-detail.svg` | 详情 |
| `screens/03-search.svg` | 搜索网格 |
| `screens/04-player-osd.svg` | 播放器 OSD（P3） |

**Figma**：当前会话 Figma MCP 已配置但未加载工具，无法直接写入 Figma 文件。SVG 为 1280×720 可直接 Import；恢复 Figma MCP（新对话或重载引擎）后可再建 Variables/Components。

---

*Chief 裁决：吸收 Explore 现状清单 + Architect 完整 token/屏单 + Critic 对 borealis 能力与优先级的硬伤修正，以本文件为准。*
