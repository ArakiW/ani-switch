// SPDX-License-Identifier: AGPL-3.0
//
// Cached source provider. When the user has previously downloaded an
// episode (e.g. via the BT source), the file lives under
// <config_dir>/cache/<bangumiId>/<episodeNumber>.<ext>.
//
// In v0.1 we don't have a BT source, so this is mostly a stub that
// returns an empty list. The interface is in place so a later addition
// is mechanical.

#pragma once

#include "core/source_manager.hpp"

namespace aniswitch {

class CacheSourceProvider : public ISourceProvider {
public:
    SourceKind kind() const override { return SourceKind::CACHE; }
    void enumerate(int32_t episodeId,
                   SourceListCb callback,
                   std::function<void(const std::string&, int)> error) override;
};

}  // namespace aniswitch
