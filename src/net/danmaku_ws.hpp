// SPDX-License-Identifier: AGPL-3.0
//
// COMPATIBILITY SHIM — this file used to declare DanmakuWebSocket
// (the speculative "myani WebSocket" client that the upstream repo
// animeko actually never exposed in open source).  Animeko's
// danmaku relay is plain HTTPS REST; the real client lives in
// net/myani_client.hpp.  This file is kept so old `#include
// "net/danmaku_ws.hpp"` lines don't break the build; new code should
// use net/myani_client.hpp directly.
#pragma once
#include "net/myani_client.hpp"
