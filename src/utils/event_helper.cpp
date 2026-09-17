// SPDX-License-Identifier: AGPL-3.0
// Adapted from xfangfang/wiliwili (GPL-3.0)
//
// All event types are declared header-only via brls::Event<>. This
// translation unit exists to satisfy the build and could host future
// event-loop helpers (e.g. a global dispatcher) without changing the
// public headers.

#include "utils/event_helper.hpp"
namespace aniswitch {
// No-op: events are pure data, instantiated at the call site.
}
