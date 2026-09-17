// SPDX-License-Identifier: AGPL-3.0
#include "utils/image_helper.hpp"
#include "utils/thread_helper.hpp"
#include "net/http.hpp"
#include <borealis/extern/nanovg/stb_image.h>
#include <borealis/core/logger.hpp>
#include <limits>
#include <memory>

namespace aniswitch {

void fetchAndDecodeImage(const std::string& url, ImageCallback cb) {
    submit_detached([url, cb = std::move(cb)] {
        DecodedImage image;
        try {
            auto session = HTTP::createSession();
            // v20.0: skip raw-DNS rewrite when a LAN proxy is set —
            // the proxy resolves the hostname and SNI must stay a name.
            std::string effectiveUrl = url;
            std::string hostForHeader;
#if defined(__SWITCH__)
            if (!HTTP::hasProxy()) {
                // v16.10.8.5: pre-resolve DNS — same root cause as
                // v16.10.8.2 / v16.10.8.3 / v16.10.8.4 (cpr/curl's
                // newlib gethostbyname hangs in hbmenu applet mode).
                HTTP::rewriteUrlForIP(url, effectiveUrl, hostForHeader);
            }
#endif
            session->SetUrl(cpr::Url{effectiveUrl});
            if (!hostForHeader.empty()) {
                session->UpdateHeader(cpr::Header{{"Host", hostForHeader}});
            }
            auto response = session->Get();
            if (!response.error && response.status_code == 200 &&
                response.text.size() <= static_cast<size_t>(std::numeric_limits<int>::max())) {
                std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
                    stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(response.text.data()),
                        static_cast<int>(response.text.size()), &image.width, &image.height,
                        &image.channels, STBI_rgb_alpha), stbi_image_free);
                if (pixels) {
                    image.channels = STBI_rgb_alpha;
                    size_t size = static_cast<size_t>(image.width) * image.height * image.channels;
                    image.pixels.assign(pixels.get(), pixels.get() + size);
                } else {
                    image = {};
                }
            }
        } catch (const std::exception& e) {
            brls::Logger::error("Image load failed: {}", e.what());
            image = {};
        }
        if (cb) cb(std::move(image));
    });
}

}  // namespace aniswitch
