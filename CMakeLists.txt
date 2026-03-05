cmake_minimum_required(VERSION 3.10)
project(PSPModBase C)

include($ENV{PSPDEV}/psp/share/AddPrxModule.cmake)

# Common compiler flags matching the original makefiles
set(COMMON_CFLAGS
    -O2 -Os -G0 -Wall
    -fshort-wchar -fno-pic
    -mno-check-zero-division
)

# --------------------------------------------------------------------------
# Encoding library (encoding/transform + tables)
# --------------------------------------------------------------------------
add_library(encoding STATIC
    source/PSPModBase/encoding/transform.c
    source/PSPModBase/encoding/gb2312.c
    source/PSPModBase/encoding/sjis.c
    source/PSPModBase/encoding/utf16.c
)

target_compile_options(encoding PRIVATE ${COMMON_CFLAGS})
target_include_directories(encoding PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/source/PSPModBase)

# --------------------------------------------------------------------------
# PSPModBaseBoot – kernel-mode bootstrapper
# --------------------------------------------------------------------------
add_prx_module(PSPModBaseBoot
    source/PSPModBaseBoot/main.c
    source/PSPModBaseBoot/exports.c
)

target_compile_options(PSPModBaseBoot PRIVATE ${COMMON_CFLAGS})
target_link_libraries(PSPModBaseBoot PRIVATE pspsystemctrl_kernel m)

# --------------------------------------------------------------------------
# PSPModBase – user-mode plugin
# --------------------------------------------------------------------------
add_prx_module(PSPModBase
    source/PSPModBase/main.c
    source/PSPModBase/abort_stub.c
    includes/psp/mips.c
    includes/psp/injector.c
    includes/psp/rini.c
    includes/psp/inireader.c
    source/PSPModBase/exports.c
)

target_compile_options(PSPModBase PRIVATE ${COMMON_CFLAGS} -fpack-struct=16)
target_link_libraries(PSPModBase PRIVATE
    encoding
    pspsystemctrl_user
    pspkubridge
    m
)
