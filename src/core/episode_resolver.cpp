// SPDX-License-Identifier: AGPL-3.0
#include "core/episode_resolver.hpp"
#include "core/http_source.hpp"
#include "core/web_selector_provider.hpp"
#include "net/bgm_client.hpp"
#include "net/http.hpp"
#include "utils/sqlite_store.hpp"
#include <borealis/core/logger.hpp>
#include <fmt/format.h>
#include <cstdio>
#if defined(__SWITCH__)
extern "C" void aniswitchStartupLog(const char* message);
#endif

namespace aniswitch {

namespace {
// The single WebSelectorProvider instance is owned by SourceManager
// registration; we hold a non-owning pointer here.  SourceManager
// outlives any EpisodeResolver callback, so the pointer is stable.
WebSelectorProvider* gWebSelector = nullptr;

void resolveLog(const char* msg) {
    brls::Logger::info("{}", msg);
#if defined(__SWITCH__)
    aniswitchStartupLog(msg);
#endif
}

void logStage(int32_t ep, int32_t sid, size_t n, const char* stage) {
    char b[160];
    snprintf(b, sizeof(b), "RESOLVE: ep=%d sid=%d nSources=%zu stage=%s",
             ep, sid, n, stage);
    resolveLog(b);
}

// Classify transport / HTTP failures into actionable Chinese text.
// Avoid the bare "解析失败" the user complained about.
std::string classifyError(const std::string& msg, int code, int32_t ep,
                          int32_t sid) {
    const std::string m = msg.empty() ? std::string{} : msg;
    auto has = [&](const char* needle) {
        return m.find(needle) != std::string::npos;
    };
    const std::string ids = fmt::format("ep={} sid={}", ep, sid);

    if (code == 404 || has("404") || has("Not Found")) {
        return fmt::format(
            "集数 ID 无效或 Bangumi 无此集（404）。{}；sources.json 也无映射。",
            ids);
    }
    if (code == 403 || has("403")) {
        return fmt::format("Bangumi 拒绝访问（403）。{}。可稍后重试或配置代理。", ids);
    }
    if (code == 28 || has("Timeout") || has("timeout") || has("timed out")) {
        return fmt::format(
            "网络超时。{}。请检查网络/Clash 代理（设置页 httpProxy），"
            "或把该集写入 sources.json。",
            ids);
    }
    if (code == 6 || has("resolve") || has("Resolve") || has("Could not resolve") ||
        has("host")) {
        return fmt::format(
            "域名解析失败。{}。DNS 不可用；可配置代理或写入 sources.json。", ids);
    }
    if (has("SSL") || has("ssl") || has("TLS") || has("certificate")) {
        return fmt::format("TLS/SSL 错误。{}。", ids);
    }
    if (has("Connection") || has("connection") || has("refused") || code == 7) {
        return fmt::format(
            "连接失败（TCP 不通）。{}。实机常见为 443 被挡；请设代理或写入 "
            "sources.json。",
            ids);
    }
    if (has("API parse") || has("parse")) {
        return fmt::format("接口返回无法解析。{}；原始错误: {}", ids, m);
    }
    if (!m.empty()) {
        return fmt::format("在线解析失败。{}；原始错误: {}", ids, m);
    }
    return fmt::format("在线解析失败（未知原因）。{}。", ids);
}

std::string emptySourcesNote(int32_t ep, int32_t sid) {
    return fmt::format(
        "无可用播放源（ep={} sid={}）。sources.json 无此 key，在线源也未解析到 "
        "m3u8/mp4。可写入 sources.json、配置代理，或从本地视频播放。",
        ep, sid);
}

ResolvedEpisode makeResult(int32_t ep, int32_t sid, std::string title,
                           double epNum, std::vector<VideoSource> sources,
                           std::string note) {
    ResolvedEpisode r;
    r.episodeId = ep;
    r.bangumiSubjectId = sid;
    r.title = std::move(title);
    r.episodeNumber = epNum;
    r.sources = std::move(sources);
    r.resolveNote = std::move(note);
    return r;
}

// Final SourceManager pass after Bangumi metadata (or as last online try).
void enumerateOnline(int32_t ep, int32_t sid, ResolvedEpisode base,
                     ResolvedCb callback,
                     std::function<void(const std::string&, int)> error) {
    SourceManager::instance().enumerate(
        ep,
        [ep, sid, base, callback](std::vector<VideoSource> sources) mutable {
            if (sources.empty()) {
                // One more sources.json pass with both ids (web-selector
                // may have soft-returned empty while json had a subject key).
                sources = HTTPSourceProvider::lookup({ep, sid});
            }
            base.sources = std::move(sources);
            base.resolveNote = base.sources.empty()
                                   ? emptySourcesNote(ep, sid)
                                   : "来源: 在线解析 / sources.json";
            logStage(ep, sid, base.sources.size(), "online-done");
            if (callback) callback(std::move(base));
        },
        [ep, sid, base, callback, error](const std::string& msg, int code) mutable {
            auto extra = HTTPSourceProvider::lookup({ep, sid});
            if (!extra.empty()) {
                base.sources = std::move(extra);
                base.resolveNote = "来源: sources.json（在线源失败后回退）";
                logStage(ep, sid, base.sources.size(), "online-fail-json-fallback");
                if (callback) callback(std::move(base));
                return;
            }
            logStage(ep, sid, 0, "online-fail");
            if (error) error(classifyError(msg, code, ep, sid), code);
        });
}

}  // namespace

void EpisodeResolver::setWebSelectorProvider(WebSelectorProvider* p) { gWebSelector = p; }

void EpisodeResolver::resolve(int32_t subjectId, int32_t episodeId,
                              ResolvedCb callback,
                              std::function<void(const std::string&, int)> error) {
    logStage(episodeId, subjectId, 0, "begin");

    // Demo / sentinel ids must never hit the network claiming a live parse.
    if (episodeId < 0) {
        logStage(episodeId, subjectId, 0, "demo-id");
        if (error) {
            error(fmt::format(
                      "演示条目（负数集 ID {}），不进行在线解析。"
                      "请选择本地测试视频，或打开真实番剧条目。",
                      episodeId),
                  1);
        }
        return;
    }
    if (episodeId == 0) {
        logStage(episodeId, subjectId, 0, "invalid-id");
        if (error) error("无效的集数 ID（0），无法解析播放源。", 2);
        return;
    }

    // ---- Stage 1: sources.json keyed by bangumi episode id (then subject).
    std::vector<int32_t> keys{episodeId};
    if (subjectId > 0 && subjectId != episodeId) keys.push_back(subjectId);
    auto local = HTTPSourceProvider::lookup(keys);
    logStage(episodeId, subjectId, local.size(), "sources.json");
    if (!local.empty()) {
        auto result = makeResult(episodeId, subjectId,
                                 "ep " + std::to_string(episodeId), 0,
                                 std::move(local), "来源: sources.json");
        if (callback) callback(std::move(result));
        return;
    }

    // ---- Stage 2: Bangumi episode metadata (title / subject / duration).
    const std::string url = BangumiClient::baseUrl() + "/v0/episodes/" +
                            std::to_string(episodeId);
    HTTP::getResult<Episode>(
        url, {},
        [subjectId, episodeId, callback, error](Episode episode) {
            const int32_t ep = episode.id ? episode.id : episodeId;
            const int32_t sid =
                episode.subjectID ? episode.subjectID : subjectId;
            const std::string title =
                !episode.nameCN.empty()
                    ? episode.nameCN
                    : (!episode.name.empty() ? episode.name
                                             : ("ep " + std::to_string(ep)));
            ResolvedEpisode base = makeResult(
                ep, sid, title, episode.sort, {},
                "来源: Bangumi 元数据 + 在线源");
            base.durationMs = static_cast<int64_t>(episode.duration) * 1000;
            if (auto progress = SQLiteStore::instance().getProgress(ep))
                base.resumePositionMs = progress->positionMs;
            if (gWebSelector) {
                gWebSelector->setContext(ep, episode.nameCN, episode.name,
                                         episode.sort, title);
            }
            logStage(ep, sid, 0, "bangumi-meta-ok");

            // Re-check sources.json with API-returned ids (may differ from
            // caller-supplied subjectId when history stored 0).
            auto again = HTTPSourceProvider::lookup({ep, sid});
            if (!again.empty()) {
                base.sources = std::move(again);
                base.resolveNote = "来源: sources.json（Bangumi 元数据后）";
                logStage(ep, sid, base.sources.size(), "sources.json-after-meta");
                if (callback) callback(std::move(base));
                return;
            }
            enumerateOnline(ep, sid, std::move(base), callback, error);
        },
        [subjectId, episodeId, callback, error](const std::string& msg, int code) {
            logStage(episodeId, subjectId, 0, "bangumi-meta-fail");
            // Last chance: subject-id key in sources.json.
            auto extra = HTTPSourceProvider::lookup({episodeId, subjectId});
            if (!extra.empty()) {
                auto result =
                    makeResult(episodeId, subjectId,
                               "ep " + std::to_string(episodeId), 0,
                               std::move(extra), "来源: sources.json（Bangumi 失败后）");
                logStage(episodeId, subjectId, result.sources.size(),
                         "sources.json-after-bangumi-fail");
                if (callback) callback(std::move(result));
                return;
            }
            if (error) error(classifyError(msg, code, episodeId, subjectId), code);
        });
}

}  // namespace aniswitch
