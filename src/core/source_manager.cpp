// SPDX-License-Identifier: AGPL-3.0

#include "core/source_manager.hpp"
#include <mutex>
#include <iterator>
#include <algorithm>

namespace aniswitch {

SourceManager& SourceManager::instance() {
    static SourceManager s;
    return s;
}

void SourceManager::registerProvider(std::shared_ptr<ISourceProvider> p) {
    if (!p) return;
    providers_.push_back(std::move(p));
}

void SourceManager::enumerate(int32_t episodeId,
                              SourceListCb callback,
                              std::function<void(const std::string&, int)> error) {
    if (providers_.empty()) {
        if (callback) callback({});
        return;
    }
    struct Pending {
        std::mutex mutex;
        size_t remaining = 0;
        size_t successes = 0;
        std::vector<VideoSource> sources;
        std::string message;
        int code = 0;
    };
    auto pending = std::make_shared<Pending>();
    pending->remaining = providers_.size();
    auto complete = [pending, callback, error](std::vector<VideoSource> sources, std::string message, int code, bool success) {
        std::unique_lock<std::mutex> lock(pending->mutex);
        if (success) ++pending->successes;
        else { pending->message = std::move(message); pending->code = code; }
        pending->sources.insert(pending->sources.end(), std::make_move_iterator(sources.begin()), std::make_move_iterator(sources.end()));
        if (--pending->remaining != 0) return;
        auto result = std::move(pending->sources);
        auto failure = pending->message;
        int failureCode = pending->code;
        bool failed = pending->successes == 0;
        lock.unlock();
        if (failed) { if (error) error(failure, failureCode); }
        else if (callback) callback(std::move(result));
    };
    for (auto& provider : providers_) {
        provider->enumerate(episodeId,
            [complete](std::vector<VideoSource> results) { complete(std::move(results), "", 0, true); },
            [complete](const std::string& message, int code) { complete({}, message, code, false); });
    }
}

std::shared_ptr<VideoSource> SourceManager::pickBest(const std::vector<VideoSource>& sources) {
    if (sources.empty()) return nullptr;
    std::vector<std::shared_ptr<VideoSource>> best;
    best.reserve(sources.size());
    for (auto& s : sources) best.push_back(std::make_shared<VideoSource>(s));
    std::sort(best.begin(), best.end(),
        [](const std::shared_ptr<VideoSource>& a, const std::shared_ptr<VideoSource>& b) {
            if (a->kind != b->kind) {
                // HTTP preferred over BT/CACHE for v0.1
                if (a->kind == SourceKind::HTTP && b->kind != SourceKind::HTTP) return true;
                if (b->kind == SourceKind::HTTP && a->kind != SourceKind::HTTP) return false;
            }
            if (a->bitrate != b->bitrate) return a->bitrate > b->bitrate;
            return a->priority > b->priority;
        });
    return best.front();
}

}  // namespace aniswitch
