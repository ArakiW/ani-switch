---
feature: all-pages-content
status: in-progress
updated: 2026-09-17
branch: compose-next/all-pages-content
commits: 8bb8498..HEAD
---

# All Pages Content + Demo Fallback

## Report

## [S1] Problem
User opens Search / Collection / History / Subject Detail / Daily and sees
empty or gray screens when APIs fail, login is missing, or lists are empty.
Only Home (trends) reliably shows content. Every screen the user can reach
must show a populated layout.

## [S2] Design

### S2.1 Demo catalog
`src/ui/demo_data.hpp` — static tables of `SearchSubject`, `CalendarItem`,
`SQLiteStore::CollectionEntry`, `SQLiteStore::HistoryEntry`. Cover URLs are
empty (color-band posters); titles are real-looking CN anime names with
plausible ids (negative sentinel ids so A-click does not open a real subject).

### S2.2 Fallback rule
For each screen: try real data first. On empty result **or** error callback,
render the same UI with demo rows and a non-focusable banner label
`演示数据（网络/账号不可用）` in `ani_ink_muted` / `kTypeCaption`.

| Screen | Real source | Demo trigger |
|---|---|---|
| Search results | Ani search | empty or error after query |
| Search idle | history chips | no history → show 6 demo cards under “猜你想看” |
| Collection | SQLite / remote | empty list or not logged in |
| History | SQLite | empty list |
| Daily | Ani `/v1/schedule/airing` | empty calendar |
| Subject Detail | Bangumi subject API | subject fetch error / id < 0 |

### S2.3 Layout invariants (already in tree, must keep)
- Detail HUD shell keeps `setWidth(1280)` so ScrollingFrame does not collapse.
- Daily two-column cells use fixed pixel width.
- Demo rows reuse `makePosterCard` / 88px rows / `applyFocusStyle`.

## [S3] Out of Scope
- Player OSD (P3).
- Persisting demo entries into SQLite.
- Real login flows.

## Tasks
- [x] T1: Add `src/ui/demo_data.hpp` with shared demo lists — acceptance: compiles, no network (covers: S2.1)
- [ ] T2: Search idle + empty/error → demo grid + banner — acceptance: open Search with no history shows 6 cards (covers: S2.2; depends: T1)
- [ ] T3: Collection empty → demo grid + banner — acceptance: not-logged-in Collection shows 6 posters + 5 chips (covers: S2.2; depends: T1)
- [ ] T4: History empty → demo rows + banner — acceptance: empty history shows 3 rows with thumbs (covers: S2.2; depends: T1)
- [ ] T5: Daily empty → demo week schedule — acceptance: schedule fail still shows day headers + cards (covers: S2.2; depends: T1)
- [ ] T6: Detail error / demo id → filled shell — acceptance: opening a demo card shows title/summary/episodes not a gray page (covers: S2.2; depends: T1)
- [ ] T7: Docker NRO build — acceptance: `aniswitch.nro` links (covers: S2)
- [ ] T8: Eden smoke: Home + Search + Collection + Daily have visible content — acceptance: screenshots non-empty (covers: S2)
