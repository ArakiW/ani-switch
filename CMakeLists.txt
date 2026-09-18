cmake_minimum_required(VERSION 3.15)

# ---- Path setup (must run BEFORE toolchain.cmake which expects BOREALIS_LIBRARY) -

set(ANISWITCH_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(THIRD_PARTY_DIR ${ANISWITCH_ROOT}/third_party)
set(BOREALIS_LIBRARY ${THIRD_PARTY_DIR}/borealis/library)

# ---- Build options (from borealis commonOption.cmake) ------------------------

include(${BOREALIS_LIBRARY}/cmake/commonOption.cmake)

# ---- Toolchain setup (MUST be included BEFORE project() so cmake picks up ----
#      the cross compiler from the toolchain file. wiliwili does it this way;
#      doing it the other way around makes cmake auto-detect /usr/bin/cc as
#      the compiler before the toolchain macro has a chance to find
#      aarch64-none-elf-gcc, and every C++ file then compiles against the host
#      glibc stdlib, which fails on glibc-specific macros like __wur, __THROW.

include(${BOREALIS_LIBRARY}/cmake/toolchain.cmake)

# ---- Project info (now safe to call — toolchain has set CMAKE_C/CXX_COMPILER) -

project(aniswitch
    VERSION 0.1.0
    DESCRIPTION "Bangumi client for Nintendo Switch (based on wiliwili)"
    LANGUAGES C CXX
)

# ---- aniswitch-specific options ----------------------------------------------

option(ANISWITCH_BUILD_TESTS      "Build unit tests"                            ON)
option(ANISWITCH_BUILD_SWITCH_NRO "Build Switch NRO (requires devkitpro)"       OFF)
option(ANISWITCH_ENABLE_LTO       "Enable link-time optimization"               OFF)
option(ANISWITCH_WARN_AS_ERROR    "Treat warnings as errors"                    OFF)
option(ANISWITCH_SWITCH_DEBUG     "Enable Switch startup diagnostics"            OFF)
# Enable cpr in the production build (the test target deliberately
# does NOT set this so the cpr-using block in image_loader.cpp is
# excluded and the test stays linkable without libcurl).
if(NOT DEFINED ANISWITCH_HAS_CPR)
    set(ANISWITCH_HAS_CPR 1)
endif()
add_compile_definitions(ANISWITCH_HAS_CPR=${ANISWITCH_HAS_CPR})

if(PLATFORM_SWITCH AND ANISWITCH_SWITCH_DEBUG)
    # Keep DEBUG scoped to an explicitly requested diagnostic Switch build.
    add_compile_definitions(ANISWITCH_SWITCH_DEBUG DEBUG)
endif()

# Fix the newlib include path leak. wiliwili's toolchain.cmake sets
# CMAKE_C_FLAGS="-I${DEVKITPRO}/libnx/include -I${DEVKITPRO}/portlibs/switch/include"
# which makes libnx headers (sys/socket.h, netinet/in.h, switch/kernel/mutex.h)
# come first. They then try to #include <sys/_types.h> / <sys/lock.h>, which
# do NOT exist in libnx — they only live in the newlib sysroot at
# ${DEVKITPRO}/devkitA64/aarch64-none-elf/include/.
#
# Note: --sysroot was tried and REVERTED. Setting --sysroot on the cross
# compiler makes it look for the C++ stdlib inside the sysroot at
# <sysroot>/include/c++/... but aarch64-none-elf-gcc's C++ stdlib actually
# lives at ${DEVKITPRO}/devkitA64/lib/gcc/aarch64-none-elf/<ver>/include/c++/,
# and the toolchain's hardcoded paths get clobbered. The result: GCC falls
# back to the host's glibc C++ headers (/usr/include/c++/12/...) and fails
# to find features.h. The fix is to ONLY add the newlib include path with
# -I (or -isystem). The cross compiler still uses its own C++ stdlib.
if(PLATFORM_SWITCH)
    set(SWITCH_NEWLIB_INC ${DEVKITPRO}/devkitA64/aarch64-none-elf/include)
    if(EXISTS ${SWITCH_NEWLIB_INC}/sys/_types.h)
        add_compile_options(-I${SWITCH_NEWLIB_INC})
        message(STATUS "Switch newlib inc: ${SWITCH_NEWLIB_INC}")
    else()
        message(WARNING "Switch newlib headers not found at ${SWITCH_NEWLIB_INC}")
    endif()
endif()

# ---- Global compile settings -------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Compiler flags
if(MSVC)
    add_compile_options(/W4 /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
    add_compile_options(
        -ffunction-sections
        -fdata-sections
        -Wno-unused-variable
        -Wno-deprecated-declarations
        -Wno-template-body
        -DFMT_HEADER_ONLY=0
        -DFMT_USE_CHRONO=0
        -DFONS_HASH_LUT_SIZE=4096
    )
endif()

if(ANISWITCH_WARN_AS_ERROR)
    if(MSVC)
        add_compile_options(/WX)
    else()
        add_compile_options(-Werror)
    endif()
endif()

# Switch toolchain: skip sanitizer TryCompile checks (they take 4+ min each
# on aarch64-none-elf-gcc because the link step is slow). The checks are
# not used by anything on Switch anyway.
if(PLATFORM_SWITCH)
    set(ADDRESS_SANITIZER_AVAILABLE OFF CACHE BOOL "" FORCE)
    set(THREAD_SANITIZER_AVAILABLE OFF CACHE BOOL "" FORCE)
    set(LEAK_SANITIZER_AVAILABLE   OFF CACHE BOOL "" FORCE)
    set(UNDEFINED_BEHAVIOUR_SANITIZER_AVAILABLE OFF CACHE BOOL "" FORCE)
    set(ALL_SANITIZERS_AVAILABLE    OFF CACHE BOOL "" FORCE)
endif()

# ---- CPR configuration (must come BEFORE borealis, since both want tinyxml2) -

if(NOT DEFINED USE_SYSTEM_CPR)
    set(USE_SYSTEM_CPR OFF)
endif()
if(NOT DEFINED USE_SYSTEM_CURL)
    set(USE_SYSTEM_CURL OFF)
endif()

if(USE_SYSTEM_CPR)
    find_package(cpr CONFIG REQUIRED)
else()
    set(BUILD_SHARED_LIBS OFF)
    set(CPR_ENABLE_SSL ON)
    if(PLATFORM_SWITCH OR PLATFORM_PS4 OR PLATFORM_PSV)
        # v19.0.1: vendored curl-8.4.0 source (see
        # `third_party/curl-8.4.0/`) is a fresh FetchContent-less
        # tree — there is no `_deps/curl-build/CMakeCache.txt`
        # left over from a previous build to cache-stamp the
        # `try_run()` results that `CMake/OtherTests.cmake` invokes
        # for HAVE_H_ERRNO_ASSIGNABLE etc.  In a clean cold build
        # those try_run() calls would error out with
        # "try_run() invoked in cross-compiling mode", killing
        # the configure before we even reach the curl build
        # target.  Pre-set the cache variables the curl feature
        # check uses so try_run() short-circuits to the cached
        # result.  newlib ships <netdb.h> with `h_errno` as an
        # int, so HAVE_H_ERRNO_ASSIGNABLE is effectively TRUE on
        # the Switch.
        set(HAVE_H_ERRNO_ASSIGNABLE_EXITCODE "0" CACHE STRING "" FORCE)
        set(HAVE_H_ERRNO_ASSIGNABLE TRUE CACHE BOOL "" FORCE)
        set(HAVE_H_ERRNO TRUE CACHE BOOL "" FORCE)
        # v18.0: drop the system-curl shortcut.  devkitpro's
        # bundled libcurl is 7.69.1 (verified 2026-09-09) and
        # mbedTLS is 2.28.10 — that combination has no cipher
        # suite in common with Cloudflare's TLS 1.3 edge, so
        # every HTTPS request just hung in SSL_connect.  The
        # cpr 1.10.5 vendored curl is 8.4.0 and supports
        # mbedTLS 2.28+ via the mbedTLS backend, which gives
        # us ChaCha20-Poly1305 / AES-256-GCM / X25519 and lets
        # the handshake complete.  The cost is a 1-2 min
        # longer build (it now compiles curl) and a slightly
        # bigger NRO.
        set(CPR_USE_SYSTEM_CURL OFF)
        set(CPR_FORCE_MBEDTLS_BACKEND ON)
    elseif(PLATFORM_DESKTOP)
        if(WIN32)
            set(CPR_FORCE_WINSSL_BACKEND ON)
        endif()
    elseif(PLATFORM_IOS)
        set(CPR_USE_SYSTEM_CURL OFF)
        set(CPR_SKIP_CA_BUNDLE_SEARCH ON)
    elseif(PLATFORM_SWITCH OR PLATFORM_PS4 OR PLATFORM_PSV)
        # v18.0: drop the system-curl shortcut.  devkitpro's
        # bundled libcurl is 7.69.1 (verified 2026-09-09) and
        # mbedTLS is 2.28.10 — that combination has no cipher
        # suite in common with Cloudflare's TLS 1.3 edge, so
        # every HTTPS request just hung in SSL_connect.  The
        # cpr 1.10.5 vendored curl is 8.4.0 and supports
        # mbedTLS 2.28+ via the mbedTLS backend, which gives
        # us ChaCha20-Poly1305 / AES-256-GCM / X25519 and lets
        # the handshake complete.  The cost is a 1-2 min
        # longer build (it now compiles curl) and a slightly
        # bigger NRO.
        set(CPR_USE_SYSTEM_CURL OFF)
        set(CPR_FORCE_MBEDTLS_BACKEND ON)
    endif()
    add_subdirectory(${THIRD_PARTY_DIR}/cpr EXCLUDE_FROM_ALL)
    if(PLATFORM_SWITCH)
        # newlib needs _GNU_SOURCE to expose fd_set, sigh, etc. that cpr's
        # <curl/curl.h> -> <curl/multi.h> uses without including
        # <sys/select.h> themselves. This is an upstream curl/newlib
        # interaction; glibc auto-exposes them but newlib is strict.
        target_compile_definitions(cpr PRIVATE _GNU_SOURCE)
    endif()
endif()

# ---- OpenCC ----------------------------------------------------------------

if(NOT DEFINED USE_LIBROMFS)
    set(USE_LIBROMFS OFF)
endif()
if(NOT DEFINED DISABLE_OPENCC)
    set(DISABLE_OPENCC OFF)
endif()

if(NOT USE_LIBROMFS AND NOT DISABLE_OPENCC AND NOT PLATFORM_IOS)
    set(BRLS_USE_OPENCC ON)
    if(USE_SYSTEM_OPENCC)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(opencc REQUIRED IMPORTED_TARGET opencc)
        add_library(libopencc ALIAS PkgConfig::opencc)
        set(BRLS_OPENCC_INCLUDE ${opencc_INCLUDE_DIRS})
    else()
        set(BUILD_SHARED_LIBS OFF)
        set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
        add_subdirectory(${THIRD_PARTY_DIR}/OpenCC EXCLUDE_FROM_ALL)
        target_compile_options(libopencc PRIVATE -w)
        set(BRLS_OPENCC_INCLUDE ${THIRD_PARTY_DIR}/OpenCC/src)
    endif()
else()
    set(BRLS_USE_OPENCC OFF)
endif()

# ---- QR-Code-generator -----------------------------------------------------

if(USE_SYSTEM_QRCODEGEN)
    find_package(qrcodegencpp REQUIRED GLOBAL)
    add_library(qrcode ALIAS qrcodegencpp::qrcodegencpp)
else()
    add_library(qrcode STATIC ${THIRD_PARTY_DIR}/QR-Code-generator/cpp/qrcodegen.cpp)
    target_include_directories(qrcode PUBLIC ${THIRD_PARTY_DIR}/QR-Code-generator/cpp)
    set_property(TARGET qrcode PROPERTY CXX_STANDARD 17)
endif()

# ---- pystring --------------------------------------------------------------

if(USE_SYSTEM_PYSTRING)
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(pystring IMPORTED_TARGET pystring)
    endif()
    if(pystring_FOUND)
        add_library(pystring ALIAS PkgConfig::pystring)
    else()
        find_path(PYSTRING_INCLUDE_DIR pystring.h PATH_SUFFIXES pystring REQUIRED)
        find_library(PYSTRING_LIBRARY NAMES pystring REQUIRED)
        list(APPEND APP_PLATFORM_INCLUDE ${PYSTRING_INCLUDE_DIR})
        list(APPEND APP_PLATFORM_LIB ${PYSTRING_LIBRARY})
        set(APP_PLATFORM_LIB ${APP_PLATFORM_LIB} PARENT_SCOPE)
        set(APP_PLATFORM_INCLUDE ${APP_PLATFORM_INCLUDE} PARENT_SCOPE)
    endif()
else()
    add_library(pystring STATIC ${THIRD_PARTY_DIR}/pystring/pystring.cpp)
    target_include_directories(pystring PUBLIC ${THIRD_PARTY_DIR}/pystring)
    set_property(TARGET pystring PROPERTY CXX_STANDARD 17)
endif()

# ---- lunasvg ---------------------------------------------------------------

if(USE_SYSTEM_LUNASVG)
    find_package(lunasvg REQUIRED GLOBAL)
    add_library(lunasvg ALIAS lunasvg::lunasvg)
else()
    set(BUILD_SHARED_LIBS OFF)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    add_subdirectory(${THIRD_PARTY_DIR}/lunasvg EXCLUDE_FROM_ALL)
    # Lunasvg defines file-local constants with the same names in multiple
    # translation units.  CMake unity builds merge those units and make the
    # otherwise valid definitions collide, which is especially visible in the
    # Switch/deko3d build.
    set_target_properties(lunasvg PROPERTIES UNITY_BUILD OFF)
endif()

# ---- mongoose --------------------------------------------------------------

if(USE_SYSTEM_MONGOOSE)
    find_library(MONGOOSE_LIBRARY NAMES mongoose REQUIRED)
    add_library(mongoose UNKNOWN IMPORTED)
    set_property(TARGET mongoose PROPERTY IMPORTED_LOCATION ${MONGOOSE_LIBRARY})
else()
    set(MG_ENABLE_IPV6 ON)
    # On Switch the devkitpro newlib needs _POSIX_C_SOURCE=200809L to
    # expose SIGPIPE/SIG_IGN in <signal.h>. MG_* arch flags are added
    # to ensure proper feature detection.
    if(PLATFORM_SWITCH)
        set(MG_ARCH "${CMAKE_SYSTEM_PROCESSOR}" CACHE STRING "")
        add_compile_definitions(_POSIX_C_SOURCE=200809L _DEFAULT_SOURCE)
    endif()
    add_subdirectory(${THIRD_PARTY_DIR}/mongoose EXCLUDE_FROM_ALL)
endif()

# ---- MemoryModule ----------------------------------------------------------

if(MPV_BUNDLE_DLL)
    add_library(MemoryModule STATIC ${THIRD_PARTY_DIR}/MemoryModule/MemoryModule.c)
    target_include_directories(MemoryModule PUBLIC ${THIRD_PARTY_DIR}/MemoryModule)
endif()

# ---- Resource embedding configuration ----------------------------------------
# If libromfs is explicitly requested, configure it before Borealis so all
# resource consumers use the same backend. Switch defaults to native libnx
# RomFS (mounted by switch_wrapper.c), so it intentionally does not enable
# libromfs and avoids requiring a target-architecture generator executable.
if(USE_LIBROMFS AND EXISTS ${ANISWITCH_ROOT}/resources)
    add_libromfs(aniswitch ${ANISWITCH_ROOT}/resources)
endif()

# ---- borealis (defines its own dependencies, including tinyxml2) ---------

add_subdirectory(${BOREALIS_LIBRARY})

# libromfs (vendored inside borealis) is needed for the `romfs/romfs.hpp`
# header that view.hpp pulls in. borealis's CMakeLists doesn't propagate
# this include path out, so we add it explicitly.
include_directories(${BOREALIS_LIBRARY}/lib/extern/libromfs/lib/include)

# ---- libpdr (UPnP/DLNA MediaRenderer) — DISABLED -----------------------------
#
# libpdr is a Switch DLNA/UPnP renderer that wiliwili uses to expose itself
# as a media renderer on the local network. We don't need it for ani-switch
# (Bangumi is a client, not a renderer) and removing it avoids two problems:
#   1. soap.cpp / ssdp.cpp #include <mongoose.h> from C++ and mongoose.h
#      tries to #include <sys/epoll.h> when __linux__ is defined — Switch
#      has no epoll and the C++ wrapper hits a hard error.
#   2. We're not using pdr anywhere (verified: zero references in src/).
# Source is kept in third_party/libpdr/ for future reference / re-enable.
#
# Uncomment the block below to re-enable.
#
# file(GLOB_RECURSE PDR_SRC CONFIGURE_DEPENDS
#     ${THIRD_PARTY_DIR}/libpdr/src/*.cpp)
# add_library(pdr STATIC ${PDR_SRC})
# target_include_directories(pdr PUBLIC
#     ${THIRD_PARTY_DIR}/libpdr/include
#     ${THIRD_PARTY_DIR}/mongoose
#     ${BOREALIS_LIBRARY}/lib/extern/tinyxml2)
# target_compile_features(pdr PUBLIC cxx_std_17)

# ---- Source files ------------------------------------------------------------

# v18.0: see platform/switch/curl_basename_shim.c — vendored
# curl 8.4.0 calls POSIX basename(3) from lib/mime.c and newlib
# doesn't ship it.  We add the shim directly to the executable
# sources so the symbol is visible to the linker even when
# libcurl's libcurl.a is re-scanned.
set(ANISWITCH_SOURCES
    # v18.0: basename shim for vendored curl 8.4.0
    ${ANISWITCH_ROOT}/platform/switch/curl_basename_shim.c
    # entry
    src/main.cpp
    # utils
    src/utils/config_helper.cpp
    src/utils/dns_helper.cpp
    src/utils/event_helper.cpp
    src/utils/gesture_helper.cpp
    src/utils/activity_helper.cpp
    src/utils/register_helper.cpp
    src/utils/string_helper.cpp
    src/utils/number_helper.cpp
    src/utils/thread_helper.cpp
    src/utils/crash_helper.cpp
    src/utils/perf_switch.cpp  # v22.1 Switch perf/telemetry helper
    src/utils/home_led.cpp       # v22.1 HOME LED status indicator
    src/utils/shortcut_helper.cpp
    src/utils/image_helper.cpp
    src/utils/vibration_helper.cpp
    src/utils/home_led.cpp
    src/utils/version_helper.cpp
    src/utils/sqlite_store.cpp
    # net
    src/net/http.cpp
    src/net/mbedtls_direct.cpp  # v19.0.2 case B' skeleton
    src/net/json_helper.cpp
    src/net/md5.cpp
    src/net/uuid.cpp
    src/net/bgm_client.cpp
    src/net/bgm_auth.cpp
    src/net/ani_client.cpp  # v18.5
    src/net/dandanplay_client.cpp
    src/net/dandanplay_auth.cpp
    src/net/myani_client.cpp
    src/net/danmaku_parser.cpp
    # core
    src/core/source_manager.cpp
    src/core/http_source.cpp
    src/core/cache_source.cpp
    src/core/local_video_scanner.cpp  # v17.4
    src/core/episode_resolver.cpp
    src/core/web_selector_data.cpp
    src/core/web_selector_cache.cpp
    src/core/web_selector_provider.cpp
    src/utils/image_loader.cpp
    src/utils/update_checker.cpp
    src/ui/theme.cpp
    src/ui/status_view.cpp
    src/ui/token_refresh_task.cpp
    src/core/collection.cpp
    src/core/history.cpp
    # player
    src/player/mpv_core.cpp
    src/player/seamless_hls.cpp
    src/player/danmaku_core.cpp
    src/player/danmaku_renderer.cpp
    src/player/subtitle_core.cpp
    src/player/video_view.cpp
    # TsVitch player graft (GPL-3.0, compatible with AGPL-3.0)
    src/player/tsvitch_video_view.cpp
    src/player/tsvitch_video_profile.cpp
    src/player/tsvitch_video_progress_slider.cpp
    src/player/tsvitch_svg_image.cpp
    src/player/tsvitch_hint_label.cpp
    # ui - activities
    src/ui/activity/main_activity.cpp
    src/ui/activity/subject_activity.cpp
    src/ui/activity/episode_list_activity.cpp
    src/ui/activity/player_activity.cpp
    src/ui/activity/source_picker_activity.cpp
    src/ui/activity/search_activity.cpp
    src/ui/activity/my_collection_activity.cpp
    src/ui/activity/history_activity.cpp
    src/ui/activity/setting_activity.cpp
    src/ui/activity/settings_activity.cpp  # v18.6
    src/ui/activity/hint_activity.cpp
    src/ui/activity/onboarding_activity.cpp  # v17.3
    src/ui/activity/local_video_activity.cpp  # v17.4
    src/ui/activity/bangumi_sync_activity.cpp  # v18.4
    src/ui/activity/email_login_activity.cpp   # v18.5
    src/ui/activity/theme_preview_activity.cpp  # v18.7
    src/ui/activity/log_activity.cpp             # v18.8
    src/ui/activity/login_activity.cpp
    src/ui/activity/person_activity.cpp
    src/ui/nav_stress.cpp
    src/ui/presenter/person_presenter.cpp
    src/ui/fragment/home_recommend.cpp
    src/ui/fragment/home_bangumi.cpp
    src/ui/fragment/home_rank.cpp
    src/ui/fragment/episode_list.cpp
    src/ui/fragment/subject_info.cpp
    src/ui/fragment/comment_list.cpp
    src/ui/fragment/season_evaluate.cpp
    src/ui/fragment/player_fragments.cpp
    src/ui/fragment/subject_cell.cpp
    # ui - presenters
    src/ui/presenter/presenter.cpp
    src/ui/presenter/home_presenter.cpp
    src/ui/presenter/subject_presenter.cpp
    src/ui/presenter/episode_presenter.cpp
    src/ui/presenter/search_presenter.cpp
    src/ui/presenter/collection_presenter.cpp
    src/ui/presenter/history_presenter.cpp
    src/ui/presenter/token_refresh_presenter.cpp  # v18.4
    # activity factory
    src/ui/activity_factory.cpp
)

# ---- Platform-specific link libraries ----------------------------------------

if(PLATFORM_SWITCH)
    add_subdirectory(platform/switch)
elseif(PLATFORM_DESKTOP)
    if(NOT APPLE)
        find_package(PkgConfig REQUIRED)
    endif()
    pkg_search_module(MPV REQUIRED IMPORTED_TARGET mpv)
    find_package(Threads REQUIRED)
    find_package(OpenGL REQUIRED)
    find_package(SDL2   QUIET)
    find_package(PNG    QUIET)
    find_package(JPEG   QUIET)
    find_package(WEBP   QUIET)
endif()

add_subdirectory(${THIRD_PARTY_DIR}/sqlite)   # only for tests/CMakeLists.txt sqlite_devoptab

# ---- Target executable -------------------------------------------------------

add_executable(aniswitch ${ANISWITCH_SOURCES})

target_include_directories(aniswitch PRIVATE
    ${ANISWITCH_ROOT}/src
    ${THIRD_PARTY_DIR}
)

# Common dependencies
target_link_libraries(aniswitch PRIVATE
    borealis
    cpr
    lunasvg
    qrcode
    pystring
    mongoose
    ${CMAKE_THREAD_LIBS_INIT}
)

if(BRLS_USE_OPENCC)
    target_link_libraries(aniswitch PRIVATE libopencc)
endif()

# Platform-specific link options
if(PLATFORM_SWITCH)
    target_link_libraries(aniswitch PRIVATE aniswitch_switch)
    target_link_libraries(aniswitch PRIVATE mbedtls mbedcrypto)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SWITCH_MPV REQUIRED mpv)
    target_link_directories(aniswitch PRIVATE ${SWITCH_MPV_STATIC_LIBRARY_DIRS})
    target_link_libraries(aniswitch PRIVATE ${SWITCH_MPV_STATIC_LIBRARIES})
    target_link_options(aniswitch PRIVATE
        -Wl,--defsym=__nx_heap_size=0x10000000   # 256MB heap (default is 16MB)
    )
elseif(PLATFORM_DESKTOP)
    target_link_libraries(aniswitch PRIVATE PkgConfig::MPV)
    if(APPLE)
        target_link_libraries(aniswitch PRIVATE
            "-framework CoreMedia -framework CoreText -framework VideoToolbox"
            ${CMAKE_DL_LIBS}
        )
    elseif(WIN32)
        target_link_libraries(aniswitch PRIVATE
            winmm ws2_32 iphlpapi dnsapi
            ${CMAKE_DL_LIBS}
        )
        if(MINGW)
            set_target_properties(aniswitch PROPERTIES
                LINK_FLAGS "-static-libgcc -static-libstdc++ -static"
            )
        endif()
    else()
        target_link_libraries(aniswitch PRIVATE dl)
    endif()
endif()

# ---- Resource embedding ------------------------------------------------------

# Resources are either read from the native filesystem (desktop/Switch RomFS)
# or configured through the explicit USE_LIBROMFS block above. Do not call
# add_libromfs here: Borealis has already been configured at this point.

# ---- Tests -------------------------------------------------------------------

if(ANISWITCH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---- Install (desktop only) --------------------------------------------------

if(PLATFORM_DESKTOP AND UNIX AND NOT APPLE)
    install(TARGETS aniswitch RUNTIME DESTINATION bin)
    install(DIRECTORY resources/ DESTINATION share/aniswitch)
endif()

# ---- Summary -----------------------------------------------------------------

if(PLATFORM_SWITCH)
    set(_PLATFORM_MSG "Switch")
elseif(PLATFORM_DESKTOP)
    set(_PLATFORM_MSG "Desktop")
else()
    set(_PLATFORM_MSG "Unknown")
endif()

message(STATUS "")
message(STATUS "============================================")
message(STATUS "  ani-switch ${PROJECT_VERSION}")
message(STATUS "============================================")
message(STATUS "  Platform:        ${_PLATFORM_MSG}")
message(STATUS "  Build type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "  Tests:           ${ANISWITCH_BUILD_TESTS}")
message(STATUS "  Switch NRO:      ${ANISWITCH_BUILD_SWITCH_NRO}")
message(STATUS "  deko3d:          ${USE_DEKO3D}")
message(STATUS "============================================")
message(STATUS "")
