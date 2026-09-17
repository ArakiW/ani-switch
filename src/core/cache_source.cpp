// SPDX-License-Identifier: AGPL-3.0

#include "core/cache_source.hpp"
#include "utils/config_helper.hpp"
#include <filesystem>
#include <borealis/core/logger.hpp>

namespace aniswitch {

void CacheSourceProvider::enumerate(int32_t episodeId,
                                    SourceListCb callback,
                                    std::function<void(const std::string&, int)> error) {
    // TODO Phase 5: integrate libtorrent and write downloaded blobs to
    // <config>/cache/<bangumiId>/<episodeId>.<ext>.
    (void)episodeId;
    (void)error;
    if (callback) callback({});
    brls::Logger::debug("CacheSourceProvider: stub, no cached sources yet");
}

}  // namespace aniswitch
