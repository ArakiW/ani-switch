// SPDX-License-Identifier: AGPL-3.0
// Empty translation unit kept so CMake can continue to list the old
// file path if a downstream branch hasn't removed it yet.  All real
// code lives in net/myani_client.cpp; this TU is intentionally empty.
namespace aniswitch {
// placeholder so the TU isn't "completely empty" under -Wpedantic.
[[maybe_unused]] static int danmaku_ws_shim_dummy = 0;
}  // namespace aniswitch
