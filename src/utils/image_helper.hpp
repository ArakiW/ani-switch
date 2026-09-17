// SPDX-License-Identifier: AGPL-3.0
// Adapted from xfangfang/wiliwili (GPL-3.0)
//
// Image loader. Pulls JPEG/PNG from a URL, caches on disk, hands the
// raw RGBA bytes to a callback. Uses curl (via cpr) for fetch and
// stb_image for decode (vendored separately if needed).

#pragma once

#include <functional>
#include <string>
#include <cstdint>
#include <vector>

namespace aniswitch {

struct DecodedImage {
    int width  = 0;
    int height = 0;
    int channels = 0;
    std::vector<uint8_t> pixels;  // RGBA, top-left origin
};

// Fetch + decode in one step. On failure pixels is empty.
using ImageCallback = std::function<void(DecodedImage)>;
void fetchAndDecodeImage(const std::string& url, ImageCallback cb);

}  // namespace aniswitch
