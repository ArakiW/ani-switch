// SPDX-License-Identifier: AGPL-3.0
// Quick diagnostic for SHA-256 of "abc"
#include "sha256.h"
#include <cstdio>
int main() {
    using namespace test_sha256;
    unsigned char out[DIGEST_SIZE];
    Context ctx;
    init(ctx);
    update(ctx, reinterpret_cast<const uint8_t*>("abc"), 3);
    final_(ctx, out);
    std::printf("digest: ");
    for (auto b : out) std::printf("%02x", b);
    std::printf("\n");
    // Expected: ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
    std::printf("expect: ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\n");
    return 0;
}
