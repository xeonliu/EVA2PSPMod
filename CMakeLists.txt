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
# PSPModBaseBoot – kernel-mode bootstrapper
# --------------------------------------------------------------------------
add_prx_module(PSPModBaseBoot
    source/PSPModBaseBoot/main.c
    source/PSPModBaseBoot/exports.c
)

target_compile_options(PSPModBaseBoot PRIVATE ${COMMON_CFLAGS})
target_link_libraries(PSPModBaseBoot PRIVATE 
    pspsystemctrl_kernel 
    m
    pspdebug 
    pspdisplay_driver 
    pspctrl_driver 
    pspmodinfo 
    pspsdk 
    pspkernel
)
target_link_options(PSPModBaseBoot PRIVATE -nostdlib)

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
target_link_libraries(PSPModBase PRIVATE pspsystemctrl_user pspkubridge m)
