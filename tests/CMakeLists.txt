cmake_minimum_required(VERSION 3.15)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(aniswitch_tests LANGUAGES C CXX)
    get_filename_component(ANISWITCH_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
    set(THIRD_PARTY_DIR "${ANISWITCH_ROOT}/third_party")
    set(BOREALIS_LIBRARY "${THIRD_PARTY_DIR}/borealis/library")
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    enable_testing()
endif()

if(MINGW)
    add_link_options(-static-libgcc -static-libstdc++ -static)
endif()

# Parse tests intentionally compile only the code under test. Keeping them free
# of cpr/curl makes the native test suite usable without a networking backend.
add_executable(test_bangumi_parse
    test_bangumi_parse.cpp
)
target_include_directories(test_bangumi_parse PRIVATE
    ${ANISWITCH_ROOT}/src
    ${THIRD_PARTY_DIR}
    ${BOREALIS_LIBRARY}/include/borealis/extern
)
add_test(NAME bangumi_parse COMMAND test_bangumi_parse)

add_executable(test_dandanplay_parse
    test_dandanplay_parse.cpp
    ${ANISWITCH_ROOT}/src/net/danmaku_parser.cpp
    ${BOREALIS_LIBRARY}/lib/extern/tinyxml2/tinyxml2.cpp
)
target_include_directories(test_dandanplay_parse PRIVATE
    ${BOREALIS_LIBRARY}/include/borealis/extern/tinyxml2
    ${ANISWITCH_ROOT}/src
    ${THIRD_PARTY_DIR}
    ${BOREALIS_LIBRARY}/include
    ${BOREALIS_LIBRARY}/include/borealis/extern
    ${BOREALIS_LIBRARY}/lib/extern/fmt/include
)
add_test(NAME dandanplay_parse COMMAND test_dandanplay_parse)

# dandanplay v2 request signing — exercises the SHA-256 + Base64 + URL
# path extraction logic without a network. The test links the
# production dandanplay_auth.cpp against a local SHA-256 shim
# (tests/sha256.h + tests/mbedtls/sha256.h) so the actual production
# code path is verified. On Switch, dandanplay_auth.cpp is linked
# against the real devkitpro mbedTLS at NRO build time.
add_executable(test_dandanplay_auth
    test_dandanplay_auth.cpp
    sha256.cpp
    ${ANISWITCH_ROOT}/src/net/dandanplay_auth.cpp
)
# tests/ goes first so the shim mbedtls/sha256.h shadows any system one.
target_include_directories(test_dandanplay_auth BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_include_directories(test_dandanplay_auth PRIVATE
    ${ANISWITCH_ROOT}/src
    ${BOREALIS_LIBRARY}/lib/extern/fmt/include
)
add_test(NAME dandanplay_auth COMMAND test_dandanplay_auth)

# myani REST protocol — exercises the JSON shape + the toParsed()
# conversion helper.  No network.  Only the plain-data
# net/myani_types.hpp is included (not the cpr-pulling
# net/myani_client.hpp) so the test stays linkable on every host.
add_executable(test_myani_parse test_myani_parse.cpp)
target_include_directories(test_myani_parse PRIVATE
    ${ANISWITCH_ROOT}/src
    ${BOREALIS_LIBRARY}/include/borealis/extern
)
add_test(NAME myani_parse COMMAND test_myani_parse)

# ProgramConfig::getConfigDir() test was removed: config_helper.cpp
# transitively pulls in cpr/curl/curl.h on every host which would
# require us to ship cURL headers in the test target.  The path
# fix is small enough to be reviewable directly; see commit history.
# (Residual entries below are kept as a marker for future work.)

option(ANISWITCH_BUILD_SQLITE_TEST "Build persistent store regression" ON)
if(ANISWITCH_BUILD_SQLITE_TEST)
    add_executable(test_sqlite_store
        test_sqlite_store.cpp
        ${ANISWITCH_ROOT}/src/utils/sqlite_store.cpp
    )
    target_include_directories(test_sqlite_store PRIVATE
        ${ANISWITCH_ROOT}/src
        ${THIRD_PARTY_DIR}
        ${BOREALIS_LIBRARY}/include
        ${BOREALIS_LIBRARY}/include/borealis/extern
        ${BOREALIS_LIBRARY}/lib/extern/fmt/include
    )
    add_test(NAME sqlite_store COMMAND test_sqlite_store)
endif()

add_executable(test_misc_utils
    test_misc_utils.cpp
    ${ANISWITCH_ROOT}/src/utils/string_helper.cpp
    ${ANISWITCH_ROOT}/src/utils/number_helper.cpp
    ${ANISWITCH_ROOT}/src/net/md5.cpp
)
target_include_directories(test_misc_utils PRIVATE
    ${ANISWITCH_ROOT}/src
    ${THIRD_PARTY_DIR}
    ${BOREALIS_LIBRARY}/lib/extern/fmt/include
)
target_compile_definitions(test_misc_utils PRIVATE FMT_HEADER_ONLY=1)
add_test(NAME misc_utils COMMAND test_misc_utils)

add_executable(test_web_selector_data test_web_selector_data.cpp
    ${ANISWITCH_ROOT}/src/core/web_selector_data.cpp
)
target_include_directories(test_web_selector_data PRIVATE
    ${ANISWITCH_ROOT}/src
    ${THIRD_PARTY_DIR}
    ${BOREALIS_LIBRARY}/include
    ${BOREALIS_LIBRARY}/include/borealis/extern
)
add_test(NAME web_selector_data COMMAND test_web_selector_data)

# WebSelector HTML cache (search + detail).  Links only the free-function
# translation unit so we don't need cpr / borealis to run the test on a
# vanilla Linux/Windows host.
add_executable(test_web_selector_cache test_web_selector_cache.cpp
    ${ANISWITCH_ROOT}/src/core/web_selector_cache.cpp
)
target_include_directories(test_web_selector_cache PRIVATE
    ${ANISWITCH_ROOT}/src
)
add_test(NAME web_selector_cache COMMAND test_web_selector_cache)

# ImageLoader — covers the pure-function bits (URL hashing +
# cachePathFor + load("") contract).  The network fetch path
# is exercised in the live Switch smoke test, not here, because
# it would pull cpr into the test target and break the "no
# networking" rule of these parse tests.
add_executable(test_image_loader test_image_loader.cpp
    ${ANISWITCH_ROOT}/src/utils/image_loader.cpp
)
target_include_directories(test_image_loader PRIVATE
    ${ANISWITCH_ROOT}/src
    ${THIRD_PARTY_DIR}
    ${BOREALIS_LIBRARY}/include
    ${BOREALIS_LIBRARY}/include/borealis/extern
    ${BOREALIS_LIBRARY}/lib/extern/fmt/include
)
# Use fmt as header-only so we don't need to build a fmt static
# library alongside the test (the main binary uses a compiled
# fmt; the test gets the inlined versions).  brls::Logger still
# works in header-only mode for the few call sites we exercise.
target_compile_definitions(test_image_loader PRIVATE FMT_HEADER_ONLY=1)
# We deliberately do NOT link ssl/crypto or cpr here — the test
# only exercises the pure-function bits (URL hashing +
# cachePathFor + load("") contract).  The live download path
# needs cpr and is covered by the live Switch smoke test instead.
# Note: ANISWITCH_HAS_CPR is left undefined for this target so
# the cpr block in image_loader.cpp is excluded by `#ifdef`.
add_test(NAME image_loader COMMAND test_image_loader)

# UpdateChecker — pure version comparison, no network.  Just
# link the impl file (no cpr) and test the compareVersions
# helper against a representative grid of semver-shaped inputs.
add_executable(test_update_checker test_update_checker.cpp
    ${ANISWITCH_ROOT}/src/utils/update_checker.cpp
)
target_include_directories(test_update_checker PRIVATE
    ${ANISWITCH_ROOT}/src
    ${THIRD_PARTY_DIR}
    ${BOREALIS_LIBRARY}/include
    ${BOREALIS_LIBRARY}/include/borealis/extern
    ${BOREALIS_LIBRARY}/lib/extern/fmt/include
)
target_compile_definitions(test_update_checker PRIVATE
    FMT_HEADER_ONLY=1
    ANISWITCH_HAS_CPR=0
)
add_test(NAME update_checker COMMAND test_update_checker)

find_package(Threads REQUIRED)
add_executable(test_runtime_utils
    test_runtime_utils.cpp
    ${ANISWITCH_ROOT}/src/utils/thread_helper.cpp
    ${ANISWITCH_ROOT}/src/utils/dns_helper.cpp
)
add_executable(test_presenter_lifetime test_presenter_lifetime.cpp)
foreach(target test_runtime_utils test_presenter_lifetime)
    target_include_directories(${target} PRIVATE
        ${ANISWITCH_ROOT}/src
        ${BOREALIS_LIBRARY}/include
        ${BOREALIS_LIBRARY}/lib/extern/fmt/include
    )
    target_compile_definitions(${target} PRIVATE FMT_HEADER_ONLY=1)
    target_link_libraries(${target} PRIVATE Threads::Threads)
endforeach()
if(WIN32)
    target_link_libraries(test_runtime_utils PRIVATE ws2_32)
endif()
add_test(NAME runtime_utils COMMAND test_runtime_utils)
add_test(NAME presenter_lifetime COMMAND test_presenter_lifetime)
set_tests_properties(runtime_utils presenter_lifetime PROPERTIES TIMEOUT 15)

add_executable(test_sources test_sources.cpp ${ANISWITCH_ROOT}/src/core/source_manager.cpp)
target_include_directories(test_sources PRIVATE ${ANISWITCH_ROOT}/src)
target_link_libraries(test_sources PRIVATE Threads::Threads)
add_test(NAME sources COMMAND test_sources)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_library(sqlite_devoptab STATIC ${THIRD_PARTY_DIR}/sqlite/sqlite3.c)
    target_compile_definitions(sqlite_devoptab PRIVATE SQLITE_SWITCH_DEVOPTAB SQLITE_OMIT_LOAD_EXTENSION SQLITE_OMIT_WAL)
    target_compile_options(sqlite_devoptab PRIVATE -O0)
    target_link_libraries(sqlite_devoptab PUBLIC Threads::Threads m)
    add_executable(test_sqlite_devoptab test_sqlite_devoptab.cpp)
    target_include_directories(test_sqlite_devoptab PRIVATE ${THIRD_PARTY_DIR}/sqlite)
    target_link_libraries(test_sqlite_devoptab PRIVATE sqlite_devoptab)
    add_test(NAME sqlite_devoptab COMMAND test_sqlite_devoptab)
    set_tests_properties(sqlite_devoptab PROPERTIES TIMEOUT 30)
endif()

# v16.10.12: live end-to-end HTTP test.  This is the gate we
# promised the user — we no longer ship a new NRO version
# without first confirming the same cpr session shape on the
# host gets a real 200 from api.bgm.tv.  It exercises the
# SslOptions (TLS 1.2 + ca_info + verify off) and the URL
# rewrite (api.bgm.tv -> 104.26.8.23) + Host header
# (UpdateHeader merge) that the NRO uses.  If this test
# passes, the Switch is extremely likely to also pass —
# the only Switch-specific piece left is the mbedTLS
# backend, and even that we already forced to TLS 1.2.
# Skipped (returns 0) if the network is offline.
add_executable(test_http_e2e
    test_http_e2e.cpp
)
target_include_directories(test_http_e2e PRIVATE
    ${ANISWITCH_ROOT}/src
    ${THIRD_PARTY_DIR}
)
target_link_libraries(test_http_e2e PRIVATE cpr)
if(WIN32)
    target_link_libraries(test_http_e2e PRIVATE ws2_32)
endif()
add_test(NAME http_e2e COMMAND test_http_e2e)
set_tests_properties(http_e2e PROPERTIES TIMEOUT 30)
