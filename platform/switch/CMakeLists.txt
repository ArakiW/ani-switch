# platform/switch/CMakeLists.txt
#
# Switch build helpers for ani-switch.
# Provides the `aniswitch_switch` target that pulls in libnx, deko3d, and the
# averne ffmpeg/mpv Switch packages.

# ---- Find switch tools -------------------------------------------------------

set(NX_NACPTOOL_EXE    ${DEVKITPRO}/tools/bin/nacptool    CACHE FILEPATH "nacptool")
set(NX_ELF2NRO_EXE     ${DEVKITPRO}/tools/bin/elf2nro     CACHE FILEPATH "elf2nro")
set(NX_UAM_EXE         ${DEVKITPRO}/tools/bin/uam         CACHE FILEPATH "uam")

# ---- Build the switch wrapper object library --------------------------------
#
# The wrapper (userAppInit / userAppExit) must be the first thing compiled so
# that constructors of static objects see the initialized socket layer.

set(SWITCH_SOURCES
    ${ANISWITCH_ROOT}/platform/switch/switch_wrapper.c
    # v18.0: POSIX basename() shim.  Vendored curl 8.4.0
    # (mbedTLS backend, used in the v18.0 P0 fix) calls
    # basename(3) from lib/mime.c and newlib doesn't ship it.
    # Without this shim, the Switch link fails with
    # "undefined reference to `basename'".
    ${ANISWITCH_ROOT}/platform/switch/curl_basename_shim.c
)

add_library(aniswitch_switch STATIC ${SWITCH_SOURCES})

# Re-add the libnx include paths that the global toolchain stripped
target_include_directories(aniswitch_switch PRIVATE
    ${DEVKITPRO}/libnx/include
    ${DEVKITPRO}/portlibs/switch/include
)

target_link_libraries(aniswitch_switch PRIVATE
    nx
)

# mbedTLS lives in the devkitpro portlibs; we use it for PKCE SHA-256
# and PKCE random-byte generation in bgm_auth (replaces OpenSSL, which
# devkitpro's switch portlibs don't ship).
find_library(MBEDTLS_LIB mbedtls HINTS ${DEVKITPRO}/portlibs/switch/lib)
find_library(MBEDCRYPTO_LIB mbedcrypto HINTS ${DEVKITPRO}/portlibs/switch/lib)

# ---- deko3d shader generation ------------------------------------------------

if(USE_DEKO3D)
    set(ANISWITCH_DKSH_DIR ${CMAKE_BINARY_DIR}/resources/shaders)
    file(MAKE_DIRECTORY ${ANISWITCH_DKSH_DIR})

    set(SWITCH_SHADER_BASE ${BOREALIS_LIBRARY}/lib/extern/nanovg/deko3d/shaders)

    add_custom_command(OUTPUT ${ANISWITCH_DKSH_DIR}/fill_vsh.dksh
        COMMAND ${NX_UAM_EXE} -s vert
                -o ${ANISWITCH_DKSH_DIR}/fill_vsh.dksh
                ${SWITCH_SHADER_BASE}/fill_vsh.glsl
        DEPENDS ${SWITCH_SHADER_BASE}/fill_vsh.glsl
        COMMENT "Compiling fill_vsh.glsl -> fill_vsh.dksh"
    )
    add_custom_command(OUTPUT ${ANISWITCH_DKSH_DIR}/fill_aa_fsh.dksh
        COMMAND ${NX_UAM_EXE} -s frag
                -o ${ANISWITCH_DKSH_DIR}/fill_aa_fsh.dksh
                ${SWITCH_SHADER_BASE}/fill_aa_fsh.glsl
        DEPENDS ${SWITCH_SHADER_BASE}/fill_aa_fsh.glsl
        COMMENT "Compiling fill_aa_fsh.glsl -> fill_aa_fsh.dksh"
    )
    add_custom_command(OUTPUT ${ANISWITCH_DKSH_DIR}/fill_fsh.dksh
        COMMAND ${NX_UAM_EXE} -s frag
                -o ${ANISWITCH_DKSH_DIR}/fill_fsh.dksh
                ${SWITCH_SHADER_BASE}/fill_fsh.glsl
        DEPENDS ${SWITCH_SHADER_BASE}/fill_fsh.glsl
        COMMENT "Compiling fill_fsh.glsl -> fill_fsh.dksh"
    )

    add_custom_target(aniswitch_dksh
        DEPENDS
            ${ANISWITCH_DKSH_DIR}/fill_vsh.dksh
            ${ANISWITCH_DKSH_DIR}/fill_aa_fsh.dksh
            ${ANISWITCH_DKSH_DIR}/fill_fsh.dksh
    )
endif()

# ---- Generate NRO from ELF ---------------------------------------------------

set(ANISWITCH_PROJECT_AUTHOR "Mavis")
set(ANISWITCH_VERSION        "0.1.0")
set(ANISWITCH_ICON           ${ANISWITCH_ROOT}/resources/icon/icon.jpg)
set(ANISWITCH_NACP           ${CMAKE_CURRENT_BINARY_DIR}/aniswitch.nacp)
set(ANISWITCH_NRO            ${CMAKE_BINARY_DIR}/aniswitch.nro)
set(ANISWITCH_ELF2NRO_ICON_ARGS)
if(EXISTS ${ANISWITCH_ICON})
    list(APPEND ANISWITCH_ELF2NRO_ICON_ARGS --icon=${ANISWITCH_ICON})
else()
    message(STATUS "NRO icon not found; using hbmenu default icon")
endif()

add_custom_target(aniswitch.nro
    DEPENDS aniswitch
    COMMAND ${NX_NACPTOOL_EXE} --create
            aniswitch
            ${ANISWITCH_PROJECT_AUTHOR}
            ${ANISWITCH_VERSION}
            ${ANISWITCH_NACP}
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${ANISWITCH_ROOT}/resources
            ${CMAKE_BINARY_DIR}/resources
    COMMAND ${CMAKE_COMMAND} -E remove -f
            ${CMAKE_BINARY_DIR}/resources/font/*.ttf
    # v16.10.11: ca-bundle.crt is shipped in release/ next to
    # the NRO for the user to drop at sdmc:/switch/aniswitch/.
    # libcurl opens it via stdio fopen, so a RomFS copy would
    # just waste 189KB inside the NRO (borealis' romfs: prefix
    # isn't visible to libcurl).  Strip it before elf2nro.
    COMMAND ${CMAKE_COMMAND} -E remove -f
            ${CMAKE_BINARY_DIR}/resources/ca-bundle.crt
    COMMAND ${NX_ELF2NRO_EXE}
            ${CMAKE_BINARY_DIR}/aniswitch.elf
            ${ANISWITCH_NRO}
            ${ANISWITCH_ELF2NRO_ICON_ARGS}
            --nacp=${ANISWITCH_NACP}
            --romfsdir=${CMAKE_BINARY_DIR}/resources
    COMMENT "Building aniswitch.nro"
    VERBATIM
)

if(USE_DEKO3D)
    add_dependencies(aniswitch.nro aniswitch_dksh)
endif()
