/* Stub abort() to break the _exit dependency from libc.
 * On PSP a PRX has no C runtime entry point, so abort() is unreachable
 * in normal flow. If it somehow fires, exit the game gracefully. */
#include <pspkernel.h>

__attribute__((weak)) void abort(void)
{
    sceKernelExitGame();
    while (1) {} /* satisfy [[noreturn]] expectation */
}
