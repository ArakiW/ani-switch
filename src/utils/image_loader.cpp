// SPDX-License-Identifier: AGPL-3.0
#include "utils/image_loader.hpp"
#include "utils/config_helper.hpp"
#include "net/http.hpp"
#include <borealis/core/logger.hpp>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <filesystem>

#if ANISWITCH_HAS_CPR
#include <cpr/cpr.h>
#endif

#if defined(__SWITCH__)
extern "C" void aniswitchStartupLog(const char* message);
#endif

namespace aniswitch {

namespace {

std::string guessExt(const std::string& url, const std::string& contentType) {
    const std::string lo = [&]{
        std::string out(url.size(), '\0');
        for (size_t k = 0; k < url.size(); ++k)
            out[k] = static_cast<char>(std::tolower(static_cast<unsigned char>(url[k])));
        return out;
    }();
    if (lo.find(".png") != std::string::npos) return ".png";
    if (lo.find(".webp") != std::string::npos) return ".webp";
    if (lo.find(".gif") != std::string::npos) return ".gif";
    if (lo.find(".jpg") != std::string::npos || lo.find(".jpeg") != std::string::npos) return ".jpg";
    std::string ct = contentType;
    std::transform(ct.begin(), ct.end(), ct.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    if (ct.find("png") != std::string::npos) return ".png";
    if (ct.find("webp") != std::string::npos) return ".webp";
    if (ct.find("gif") != std::string::npos) return ".gif";
    if (ct.find("jpeg") != std::string::npos || ct.find("jpg") != std::string::npos) return ".jpg";
    return ".bin";
}

std::string hashUrl(const std::string& url) {
    static const uint64_t kSalt = 0x9E3779B97F4A7C15ULL;
    uint64_t h = kSalt;
    for (unsigned char c : url) {
        h ^= c;
        h *= 0x100000001B3ULL;
    }
    static const char hex[] = "0123456789abcdef";
    std::string out(16, '\0');
    for (int i = 0; i < 16; ++i) {
        out[i] = hex[(h >> (4 * (15 - i))) & 0xF];
    }
    return out;
}

}  // namespace

ImageLoader& ImageLoader::instance() {
    static ImageLoader s;
    return s;
}

ImageLoader::ImageLoader() {
#ifdef __SWITCH__
    cacheDir_ = "sdmc:/switch/aniswitch/image_cache";
#else
    cacheDir_ = "./image_cache";
#endif
    try {
        std::filesystem::create_directories(cacheDir_);
    } catch (const std::exception& e) {
        brls::Logger::warning("ImageLoader: cannot create cache dir {}: {}",
                              cacheDir_, e.what());
    }
}

ImageLoader::~ImageLoader() {
    {
        std::lock_guard<std::mutex> lock(mu_);
        stop_ = true;
    }
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();
}

void ImageLoader::setCacheDir(const std::string& dir) {
    std::lock_guard<std::mutex> lock(mu_);
    cacheDir_ = dir;
    if (!cacheDir_.empty()) {
        try { std::filesystem::create_directories(cacheDir_); }
        catch (...) {}
    }
}

std::string ImageLoader::cachePathFor(const std::string& url) const {
    if (cacheDir_.empty() || url.empty()) return {};
    return cacheDir_ + "/" + hashUrl(url) + ".bin";
}

void ImageLoader::ensureWorker() {
    if (running_) return;
    running_ = true;
    stop_ = false;
    worker_ = std::thread([this]() { loop(); });
}

void ImageLoader::load(const std::string& url, LoadCb cb, const std::string& tag) {
    if (url.empty()) {
        if (cb) cb("");
        return;
    }
    const std::string path = cachePathFor(url);
    if (!path.empty()) {
        std::error_code ec;
        // cachePathFor returns <hash>.bin; downloads are renamed to
        // <hash>.jpg/.png/... so probe the stem, not the .bin path.
        const std::string stem = path.substr(0, path.size() - 4);
        for (const char* ext : {".jpg", ".jpeg", ".png", ".webp", ".gif", ".bin"}) {
            const std::string cand = stem + ext;
            if (std::filesystem::exists(cand, ec) &&
                std::filesystem::file_size(cand, ec) > 0) {
                if (cb) cb(cand);
                return;
            }
        }
    }
    const std::string key = tag.empty() ? url : tag;
    {
        std::lock_guard<std::mutex> lock(mu_);
        if (queued_.count(key)) return;  // already queued
        queued_[key] = true;
        queue_.push_back(Job{url, path, key, std::move(cb)});
        // Cap queue so a long list cannot pile up unbounded jobs.
        while (queue_.size() > 24) {
            auto dropped = std::move(queue_.front());
            queue_.pop_front();
            queued_.erase(dropped.key);
            if (dropped.cb) dropped.cb("");
        }
        ensureWorker();
    }
    cv_.notify_one();
}

void ImageLoader::loop() {
    for (;;) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(mu_);
            cv_.wait(lock, [this]() { return stop_ || !queue_.empty(); });
            if (stop_ && queue_.empty()) return;
            if (queue_.empty()) continue;
            job = std::move(queue_.front());
            queue_.pop_front();
        }
        downloadOne(job);
        {
            std::lock_guard<std::mutex> lock(mu_);
            queued_.erase(job.key);
        }
    }
}

void ImageLoader::downloadOne(const Job& job) {
#if ANISWITCH_HAS_CPR
    try {
        cpr::Session s;
        // startup.14: reuse the production Switch HTTPS profile
        // (TLS 1.2 pin + CA + raw-DNS resolve). The old bare
        // Session() skipped all of that, so covers to
        // static.myani.org failed TLS/DNS and the home rail
        // stayed on color-band placeholders.
        const int rc = HTTP::prepareFetchSession(s, job.url);
#if defined(__SWITCH__)
        {
            char _b[200];
            snprintf(_b, sizeof(_b), "IMG: fetch %s rc=%d",
                     job.url.c_str(), rc);
            aniswitchStartupLog(_b);
        }
#endif
        if (rc != 0) {
#if defined(__SWITCH__)
            aniswitchStartupLog("IMG: resolve failed, skip download");
#endif
            if (job.cb) job.cb("");
            return;
        }
        s.SetHeader(cpr::Header{{"User-Agent", "Mozilla/5.0 (ani-switch)"},
                                {"Accept", "image/*,*/*;q=0.8"}});
        auto r = s.Get();
#if defined(__SWITCH__)
        {
            char _b[200];
            snprintf(_b, sizeof(_b), "IMG: status=%ld err=%s bytes=%zu",
                     r.status_code,
                     r.error ? r.error.message.c_str() : "(none)",
                     r.text.size());
            aniswitchStartupLog(_b);
        }
#endif
        // Binary bodies live in r.text (size-aware); do not require
        // it to be printable. Status 200 + non-empty is enough.
        if (r.error || r.status_code != 200 || r.text.empty()) {
            brls::Logger::debug("ImageLoader: '{}' failed [{}]",
                                job.url, r.status_code);
            if (job.cb) job.cb("");
            return;
        }
        if (job.path.empty()) {
            if (job.cb) job.cb("");
            return;
        }
        const std::string tmp = job.path + ".part";
        std::ofstream f(tmp, std::ios::binary);
        if (!f) { if (job.cb) job.cb(""); return; }
        f.write(r.text.data(), static_cast<std::streamsize>(r.text.size()));
        f.close();
        const std::string finalPath =
            job.path.substr(0, job.path.size() - 4) +
            guessExt(job.url, r.header["content-type"]);
        std::error_code ec;
        std::filesystem::rename(tmp, finalPath, ec);
        if (ec) {
            std::filesystem::remove(tmp, ec);
            if (job.cb) job.cb("");
            return;
        }
#if defined(__SWITCH__)
        {
            char _b[200];
            snprintf(_b, sizeof(_b), "IMG: saved %s (%zu B)",
                     finalPath.c_str(), r.text.size());
            aniswitchStartupLog(_b);
        }
#endif
        if (job.cb) job.cb(finalPath);
    } catch (const std::exception& e) {
        brls::Logger::warning("ImageLoader: download failed: {}", e.what());
#if defined(__SWITCH__)
        {
            char _b[200];
            snprintf(_b, sizeof(_b), "IMG: exception %s", e.what());
            aniswitchStartupLog(_b);
        }
#endif
        if (job.cb) job.cb("");
    }
#else
    if (job.cb) job.cb("");
#endif
}

void ImageLoader::clearCache() {
    std::lock_guard<std::mutex> lock(mu_);
    if (cacheDir_.empty()) return;
    std::error_code ec;
    std::filesystem::remove_all(cacheDir_, ec);
    std::filesystem::create_directories(cacheDir_, ec);
}

}  // namespace aniswitch
