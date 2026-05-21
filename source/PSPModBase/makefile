TARGET = ../../build/PSPModBase
# include more objs here depending on what you use (e.g. "../../includes/psp/mips.o")
OBJS = main.o exports.o abort_stub.o memtalk_debug.o\
 encoding/gb2312.o encoding/utf16.o encoding/sjis.o encoding/transform.o\
 system/psputil.o system/scefttt.o system/sceLibFont.o\
 ../../includes/psp/mips.o ../../includes/psp/injector.o ../../includes/psp/rini.o ../../includes/psp/inireader.o

CFLAGS = -O2 -Os -G0 -Wall -fshort-wchar -fno-pic -mno-check-zero-division -fpack-struct=16
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS) -DF_sceLibFont_0000 -DF_sceLibFont_0003 -DF_sceLibFont_0014 -DF_sceLibFont_0017


BUILD_PRX = 1
PRX_EXPORTS = exports.exp

LIBS = -lpspsystemctrl_user -lpspkubridge -lm

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build_prx.mak
