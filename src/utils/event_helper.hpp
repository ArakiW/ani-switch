// SPDX-License-Identifier: AGPL-3.0
// Adapted from xfangfang/wiliwili (GPL-3.0)
//
// Lightweight event types wrapping brls::Event<T>. Used to decouple the
// presenter / view / activity layers without a heavy message-bus library.

#pragma once

#include <borealis/core/event.hpp>
#include <functional>
#include <string>
#include <vector>

namespace aniswitch {

enum class MpvEventEnum {
    MPV_LOADED, MPV_PAUSE, MPV_RESUME, MPV_IDLE, MPV_STOP, MPV_FILE_ERROR,
    LOADING_START, LOADING_END, UPDATE_DURATION, UPDATE_PROGRESS, START_FILE,
    END_OF_FILE, CACHE_SPEED_CHANGE, VIDEO_SPEED_CHANGE, VIDEO_VOLUME_CHANGE,
    VIDEO_MUTE, VIDEO_UNMUTE, RESET, RESTART
};
using MPVEvent = brls::Event<MpvEventEnum>;
inline brls::Event<std::string, void*> applicationEvent;

// Fired when danmaku data is loaded, refreshed, or reset.
typedef brls::Event<>            DanmakuEvent;

// Fired when the user's auth state changes (login / logout).
typedef brls::Event<bool>        AuthEvent;

// Fired when a video source finishes fetching its URL.
typedef brls::Event<std::string> VideoUrlEvent;

// Generic custom event (replaces wiliwili's CustomEvent). Subscribers can
// pass arbitrary string-tagged notifications.
struct CustomEventPayload {
    std::string tag;
    std::string data;
};
typedef brls::Event<CustomEventPayload> CustomEvent;

}  // namespace aniswitch
