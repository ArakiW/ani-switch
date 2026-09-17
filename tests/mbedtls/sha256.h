// SPDX-License-Identifier: CC0-1.0
//
// Test-only shim: when a test target adds tests/ to its include path
// before any system include path, this header shadows the devkitpro
// <mbedtls/sha256.h>.  It exposes the same C API surface as the real
// mbedTLS so dandanplay_auth.cpp links cleanly against the local
// portable SHA-256 (tests/sha256.h).  Production NRO builds on Switch
// use the real mbedTLS at link time and never see this shim.
#pragma once
#include "../sha256.h"
