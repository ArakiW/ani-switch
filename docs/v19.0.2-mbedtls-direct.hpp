// SPDX-License-Identifier: AGPL-3.0
//
// v19.0.2 case B' + C' 备选方案: 完全绕开 cpr/curl 8.4 + mbedTLS 2.28
// backend, 直接用 mbedTLS 2.28 API 重建 HTTP/1.1 client.
//
// 当 v19.0.1 debug NRO 抓到的 startup.log 显示:
//   - case A: hardcoded IP stale -> 用 docs/v19.0.2-case-A.patch 修
//   - case B: cipher 协商被 server RST -> 用本文件 (mbedtls_direct)
//   - case D: cert verify 失败 -> 也走本文件 (我们直接用 mbedTLS x509 加载 ca-bundle.crt)
//
// 设计: 给 `HTTP::createSession` 提供一个新的 backend
// 替换 cpr/curl, aniswitch HTTP 公共 API 不变 (get/post/parseJson 等等),
// aniswitch 业务层零修改。
//
// 1-2 day ship. 估 500-700 行 C++.

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/error.h"

namespace aniswitch {
namespace mbedtls_direct {

// Cipher list 我们要强制使用 (按 Cloudflare 接受顺序):
//   1. TLS-ECDHE-RSA-WITH-AES-256-GCM-SHA384   0xC030
//   2. TLS-ECDHE-RSA-WITH-AES-128-GCM-SHA256   0xC02F
//   3. TLS-ECDHE-ECDSA-WITH-AES-256-GCM-SHA384 0xC02C
//   4. TLS-RSA-WITH-AES-256-GCM-SHA384         0x009D  (兜底)
constexpr int kCiphersuites[] = {
    MBEDTLS_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
    MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
    MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
    MBEDTLS_TLS_RSA_WITH_AES_256_GCM_SHA384,
    0
};

// signature_algorithms 显式 list (mbedTLS 2.28 API):
//   我们排除 SHA1 (符合 RFC 9155), 只用 SHA256 / SHA384 / SHA512
//   + ECDSA_SECP256R1_SHA256 (用于 ECDSA cert)
constexpr int kSigHashes[] = {
    MBEDTLS_MD_SHA512,
    MBEDTLS_MD_SHA384,
    MBEDTLS_MD_SHA256,
    MBEDTLS_MD_NONE
};

// 配置: hostname, ca-bundle 路径, libnx BSD socket fd
struct Config {
    std::string  hostname;        // SNI = hostname
    std::string  ca_bundle_path;   // Mozilla ca-bundle.crt
    bool         verify_peer;      // case D 修法: true + ca-bundle 加载
    int          socket_fd;        // libnx BSD socket fd, 已经 connect 完成
};

// 单次 HTTP/1.1 request
struct Request {
    std::string method;            // "GET" / "POST"
    std::string path;              // "/calendar"
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;              // empty for GET
    int timeout_ms;                 // total request timeout
};

// HTTP/1.1 response
struct Response {
    int status_code;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
    std::string error;              // mbedTLS error string if any
};

// 配置 + 握手 + send/recv 一次往返
// usage:
//   DirectTLS tls;
//   tls.configure(Config{...});
//   auto r = tls.execute(Request{...});
class DirectTLS {
public:
    DirectTLS();
    ~DirectTLS();

    // 不拷贝
    DirectTLS(const DirectTLS&) = delete;
    DirectTLS& operator=(const DirectTLS&) = delete;

    // 初始化: 加载 mbedtls_x509_crt ca-bundle, 设 cipher list + sig_hashes,
    // 设 SNI = hostname, 完成 TLS handshake.
    // 返回 0 on success, mbedtls error code on failure.
    int configure(const Config& cfg);

    // 发送 request, 等 response, 返回 Response.
    // 内部走: 拼 HTTP/1.1 wire format -> mbedtls_ssl_write ->
    //         mbedtls_ssl_read until 0-length content / chunked done ->
    //         解析 status line + headers + body
    Response execute(const Request& req);

private:
    // mbedTLS 状态
    mbedtls_ssl_context  ssl_;
    mbedtls_ssl_config   conf_;
    mbedtls_ctr_drbg_context ctr_drbg_;
    mbedtls_entropy_context entropy_;
    mbedtls_x509_crt     cacert_;
    int                  socket_fd_ = -1;
    std::string          hostname_;

    // 内部: read 全部 response 直到 EOF, parse status + headers + body
    Response read_full_response(int timeout_ms);
};

}  // namespace mbedtls_direct
}  // namespace aniswitch
