// SPDX-License-Identifier: AGPL-3.0
//
// User-supplied HTTP(S) URLs keyed by Bangumi episode ID in sources.json.

#pragma once

#include "core/source_manager.hpp"

namespace aniswitch {

class HTTPSourceProvider : public ISourceProvider {
public:
    SourceKind kind() const override { return SourceKind::HTTP; }
    void enumerate(int32_t episodeId,
                   SourceListCb callback,
                   std::function<void(const std::string&, int)> error) override;
};

}  // namespace aniswitch
