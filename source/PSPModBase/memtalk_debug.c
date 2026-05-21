/*
 * MemTalk debug replacement.
 *
 * This file intentionally reproduces the original MemTalk_ShowMemorySentence
 * logic instead of translating it. The goal is to run inside the original game,
 * show the same sentence as the original function, and write each intermediate
 * generation stage to a log file for later Chinese sentence design.
 *
 * Hook target:
 *   0x0890F080  MemTalk_ShowMemorySentence / sub_890F080
 *
 * Log file:
 *   ms0:/PSP/memtalk_debug.log
 */

#include <pspuser.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../includes/psp/injector.h"
#include "encoding/transform.h"
#include "memtalk_debug.h"

#define MEMTALK_LOG_PATH "ms0:/PSP/memtalk_debug.log"

#define MEMTALK_TEMPLATE_COUNT 0x6D6u
#define MEMTALK_LOW24_MASK 0x00FFFFFFu
#define MEMTALK_EXPANDED_ORIG_LIMIT 0x46u

#define ADDR_GET_CURRENT_PLAYER_BIT 0x08828580u
#define ADDR_SHOW_TOKENIZED_TEXT 0x0882FD7Cu
#define ADDR_RAND_N 0x08871778u
#define ADDR_GET_NAME_BY_BIT 0x088395D8u
#define ADDR_EXPAND_ACTION_TEMPLATE 0x0890FA58u
#define ADDR_FORMAT_CHARACTER_MASK 0x0890F7B4u
#define ADDR_FORMAT_TIME_PHRASE 0x0890F8B8u
#define ADDR_FORMAT_LOCATION_PHRASE 0x0890F9F0u

#define ADDR_SIMPLE_SKELETON 0x089D52DCu
#define ADDR_TAIL_0 0x089D52F4u
#define ADDR_TAIL_1 0x089D5318u
#define ADDR_TAIL_2 0x089D533Cu
#define ADDR_TOKEN_BNI 0x089D535Cu
#define ADDR_STR_GA 0x089D5364u
#define ADDR_DETAIL_SKELETON 0x089D5368u
#define ADDR_EMPTY_STRING 0x089D0128u

typedef struct MemTalkActionRecord {
    uint32_t timestamp;   /* +0x00 */
    uint32_t maskA;       /* +0x04, template-layer $a */
    uint32_t maskB;       /* +0x08, template-layer $b */
    uint8_t valid;        /* +0x0C */
    uint8_t locationId;   /* +0x0D */
    uint16_t templateId;  /* +0x0E */
    uint8_t recordType;   /* +0x10 */
    uint8_t unk11;        /* +0x11 */
    uint16_t sortKey;     /* +0x12 */
} MemTalkActionRecord;

typedef void *(*ShowTokenizedTextFn)(int group, int speakerBit, int targetBit, int thirdBit, char *text);
typedef int (*GetCurrentPlayerBitFn)(void);
typedef int (*RandNFn)(int n);
typedef const char *(*GetNameByBitFn)(int bit);
typedef int (*ExpandActionTemplateFn)(const MemTalkActionRecord *rec, char *outBuf, unsigned int outBufSize, char delimiter, int styleBit);
typedef int (*FormatCharacterMaskFn)(uint32_t mask, int styleBit, char *out);
typedef const char *(*FormatTimePhraseFn)(const MemTalkActionRecord *rec);
typedef const char *(*FormatLocationPhraseFn)(const MemTalkActionRecord *rec, int styleBit);

static intptr_t g_gameBaseDelta = 0;
static uintptr_t g_gameTextAddr = 0;
static int g_logReady = 0;
static uint32_t g_callCount = 0;

static uintptr_t GameAddr(uintptr_t stdAddr) {
    /*
     * These constants are IDA absolute addresses. USER_MAIN reports text_addr
     * as 0x08804040, while IDA imagebase is 0x08804000 and .text also starts at
     * 0x08804040. Adding text_addr-imagebase would shift every function/string
     * by 0x40 and jump into the middle of code or strings.
     */
    return stdAddr;
}

static const char *GameStr(uintptr_t stdAddr) {
    return (const char *)GameAddr(stdAddr);
}

static void MemTalkDebug_Log(const char *fmt, ...) {
    char buf[1024];
    va_list args;
    SceUID fd;
    int n;

    va_start(args, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (n < 0) return;
    if (n >= (int)sizeof(buf)) n = (int)sizeof(buf) - 1;

    fd = sceIoOpen(MEMTALK_LOG_PATH, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
    if (fd >= 0) {
        sceIoWrite(fd, buf, (SceSize)n);
        sceIoWrite(fd, "\n", 1);
        sceIoClose(fd);
    }
}

static void MemTalkDebug_ResetLog(void) {
    SceUID fd = sceIoOpen(MEMTALK_LOG_PATH, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd >= 0) {
        sceIoClose(fd);
    }
    g_logReady = 1;
}

static int MemTalkDebug_StrLenBounded(const char *s, int maxLen) {
    int n = 0;
    if (!s) return 0;
    while (n < maxLen && s[n]) n++;
    return n;
}

static void MemTalkDebug_LogHexBytes(const char *label, const char *s, int maxBytes) {
    char line[768];
    int pos = 0;
    int i;
    int n;

    if (!s) {
        MemTalkDebug_Log("%s: <null>", label);
        return;
    }

    n = MemTalkDebug_StrLenBounded(s, maxBytes);
    pos += snprintf(line + pos, sizeof(line) - (size_t)pos, "%s ptr=%08X len<=%d hex=", label, (unsigned int)(uintptr_t)s, n);
    for (i = 0; i < n && pos < (int)sizeof(line) - 4; i++) {
        pos += snprintf(line + pos, sizeof(line) - (size_t)pos, "%02X", (unsigned char)s[i]);
        if (i + 1 < n && pos < (int)sizeof(line) - 2) {
            line[pos++] = ' ';
            line[pos] = '\0';
        }
    }
    if (s[n]) {
        snprintf(line + pos, sizeof(line) - (size_t)pos, " ...");
    }
    MemTalkDebug_Log("%s", line);
}

static void MemTalkDebug_LogUtf8Bytes(const char *label, const char *s, int maxBytes) {
    char utf8[1024];
    int n;
    int utf8Len;

    if (!s) return;

    n = MemTalkDebug_StrLenBounded(s, maxBytes);
    utf8Len = eva_sjis_to_utf8((const uint8_t *)s, n, utf8, sizeof(utf8));
    if (utf8Len < 0) {
        MemTalkDebug_Log("%s.utf8=<eva_sjis_to_utf8 error %d>", label, utf8Len);
        return;
    }

    MemTalkDebug_Log("%s.utf8=\"%s\"%s", label, utf8, s[n] ? " ..." : "");
}

static void MemTalkDebug_LogTextBytes(const char *label, const char *s, int maxBytes) {
    MemTalkDebug_LogHexBytes(label, s, maxBytes);
    MemTalkDebug_LogUtf8Bytes(label, s, maxBytes);
}

static void MemTalkDebug_LogRecord(const MemTalkActionRecord *rec) {
    if (!rec) {
        MemTalkDebug_Log("rec: <null>");
        return;
    }

    MemTalkDebug_Log(
        "rec=%08X timestamp=%u maskA=%08X maskB=%08X valid=%u locationId=%u templateId=%u recordType=%u unk11=%u sortKey=%u",
        (unsigned int)(uintptr_t)rec,
        (unsigned int)rec->timestamp,
        (unsigned int)rec->maskA,
        (unsigned int)rec->maskB,
        (unsigned int)rec->valid,
        (unsigned int)rec->locationId,
        (unsigned int)rec->templateId,
        (unsigned int)rec->recordType,
        (unsigned int)rec->unk11,
        (unsigned int)rec->sortKey);
}

static uint8_t MemTalkDebug_GetSpeakerBit(const void *ctx) {
    return *((const uint8_t *)ctx + 0x51);
}

static uint8_t MemTalkDebug_GetTargetBit(const void *ctx) {
    return *((const uint8_t *)ctx + 0x52);
}

void MemTalkDebug_SetGameBase(uintptr_t gameBase) {
    g_gameTextAddr = gameBase;
    g_gameBaseDelta = 0;
}

void MemTalkDebug_InstallHook(void) {
    MemTalkDebug_ResetLog();
    MemTalkDebug_Log("MemTalkDebug install: gameTextAddr=%08X addrMode=ida_absolute gameBaseDelta=%08X hook=0890F080 replacement=%08X",
        (unsigned int)g_gameTextAddr,
        (unsigned int)g_gameBaseDelta,
        (unsigned int)(uintptr_t)&MemTalkDebug_ShowMemorySentence);

    injector.MakeJMPwNOP(0x0890F080, (uintptr_t)&MemTalkDebug_ShowMemorySentence);
    sceKernelDcacheWritebackAll();
    sceKernelIcacheInvalidateAll();
}

void *MemTalkDebug_ShowMemorySentence(void *ctx, const void *recVoid, const char *verbSjis) {
    const MemTalkActionRecord *rec = (const MemTalkActionRecord *)recVoid;
    GetCurrentPlayerBitFn GetCurrentPlayerBit = (GetCurrentPlayerBitFn)GameAddr(ADDR_GET_CURRENT_PLAYER_BIT);
    ShowTokenizedTextFn ShowTokenizedText = (ShowTokenizedTextFn)GameAddr(ADDR_SHOW_TOKENIZED_TEXT);
    RandNFn RandN = (RandNFn)GameAddr(ADDR_RAND_N);
    GetNameByBitFn GetNameByBit = (GetNameByBitFn)GameAddr(ADDR_GET_NAME_BY_BIT);
    ExpandActionTemplateFn ExpandActionTemplate = (ExpandActionTemplateFn)GameAddr(ADDR_EXPAND_ACTION_TEMPLATE);
    FormatCharacterMaskFn FormatCharacterMask = (FormatCharacterMaskFn)GameAddr(ADDR_FORMAT_CHARACTER_MASK);
    FormatTimePhraseFn FormatTimePhrase = (FormatTimePhraseFn)GameAddr(ADDR_FORMAT_TIME_PHRASE);
    FormatLocationPhraseFn FormatLocationPhrase = (FormatLocationPhraseFn)GameAddr(ADDR_FORMAT_LOCATION_PHRASE);

    uint8_t speakerBit;
    uint8_t targetBit;
    int currentPlayerBit;
    uint32_t overlap;
    void *result = 0;
    char buffer[256];

    if (!g_logReady) {
        MemTalkDebug_ResetLog();
    }

    if (!ctx) {
        MemTalkDebug_Log("[MemTalk #%u] ctx is null", (unsigned int)++g_callCount);
        return 0;
    }

    speakerBit = MemTalkDebug_GetSpeakerBit(ctx);
    targetBit = MemTalkDebug_GetTargetBit(ctx);
    currentPlayerBit = GetCurrentPlayerBit();

    MemTalkDebug_Log("");
    MemTalkDebug_Log("========== MemTalk #%u ==========", (unsigned int)++g_callCount);
    MemTalkDebug_Log("ctx=%08X speakerBit=%u targetBit=%u currentPlayerBit=%d verbPtr=%08X",
        (unsigned int)(uintptr_t)ctx,
        (unsigned int)speakerBit,
        (unsigned int)targetBit,
        currentPlayerBit,
        (unsigned int)(uintptr_t)verbSjis);
    MemTalkDebug_LogTextBytes("verbSjis", verbSjis, 96);
    MemTalkDebug_LogRecord(rec);

    /*
     * Original visibility rule:
     * only show when current player is speaker or target.
     */
    if (speakerBit != (uint8_t)currentPlayerBit && targetBit != (uint8_t)currentPlayerBit) {
        MemTalkDebug_Log("skip: current player is neither speaker nor target");
        return 0;
    }

    overlap = rec ? (rec->maskA & rec->maskB & MEMTALK_LOW24_MASK) : 1u;
    MemTalkDebug_Log("branch: rec=%s low24Overlap=%08X => %s",
        rec ? "yes" : "no",
        (unsigned int)overlap,
        (!rec || overlap) ? "simple" : "detail");

    if (!rec || overlap) {
        const char *fmt = GameStr(ADDR_SIMPLE_SKELETON);
        MemTalkDebug_LogTextBytes("simple.fmt", fmt, 128);
        sprintf(buffer, fmt, verbSjis ? verbSjis : "");
        MemTalkDebug_LogTextBytes("simple.buffer.beforeTokenEngine", buffer, 220);
    } else {
        char expanded[80];
        char maskAText[16];
        char maskBText[16];
        const char *timePhrase;
        const char *placePhrase;
        const char *targetPrefix;
        const char *aOpt;
        const char *gaOpt;
        const char *detailFmt;
        int needA;

        if (rec->templateId >= MEMTALK_TEMPLATE_COUNT) {
            MemTalkDebug_Log("warning: templateId out of range: %u", (unsigned int)rec->templateId);
        }

        ExpandActionTemplate(rec, expanded, MEMTALK_EXPANDED_ORIG_LIMIT, '\n', speakerBit);
        FormatCharacterMask(rec->maskA, speakerBit, maskAText);
        FormatCharacterMask(rec->maskB, speakerBit, maskBText);
        timePhrase = FormatTimePhrase(rec);
        placePhrase = FormatLocationPhrase(rec, speakerBit);
        needA = (rec->maskA & (1u << speakerBit)) == 0;
        targetPrefix = targetBit ? GameStr(ADDR_TOKEN_BNI) : GameStr(ADDR_EMPTY_STRING);
        aOpt = needA ? maskAText : GameStr(ADDR_EMPTY_STRING);
        gaOpt = needA ? GameStr(ADDR_STR_GA) : GameStr(ADDR_EMPTY_STRING);
        detailFmt = GameStr(ADDR_DETAIL_SKELETON);

        MemTalkDebug_Log("detail.needA=%d", needA);
        MemTalkDebug_LogTextBytes("detail.expanded(prefix/suffix with delimiter)", expanded, sizeof(expanded));
        MemTalkDebug_LogTextBytes("detail.maskAText", maskAText, sizeof(maskAText));
        MemTalkDebug_LogTextBytes("detail.maskBText", maskBText, sizeof(maskBText));
        MemTalkDebug_LogTextBytes("detail.timePhrase", timePhrase, 96);
        MemTalkDebug_LogTextBytes("detail.placePhrase", placePhrase, 96);
        MemTalkDebug_LogTextBytes("detail.targetPrefix", targetPrefix, 32);
        MemTalkDebug_LogTextBytes("detail.aOpt", aOpt, 32);
        MemTalkDebug_LogTextBytes("detail.gaOpt", gaOpt, 16);
        MemTalkDebug_LogTextBytes("detail.fmt", detailFmt, 180);

        sprintf(
            buffer,
            detailFmt,
            targetPrefix,
            timePhrase,
            verbSjis ? verbSjis : "",
            placePhrase,
            aOpt,
            gaOpt,
            maskBText,
            expanded);

        MemTalkDebug_LogTextBytes("detail.buffer.beforeTokenEngine", buffer, 240);
    }

    result = ShowTokenizedText(0, speakerBit, targetBit, 0, buffer);
    MemTalkDebug_Log("display.main.done result=%08X", (unsigned int)(uintptr_t)result);

    /*
     * Preserve original speakerBit==16 tail sentence logic.
     */
    if (speakerBit == 16 && targetBit != 0) {
        int r = RandN(5);
        const char *targetName;
        const char *tailFmt;

        MemTalkDebug_Log("tail.rand=%d", r);

        if (r >= 0 && r < 2) {
            tailFmt = GameStr(ADDR_TAIL_2);
        } else if (r == 2) {
            tailFmt = GameStr(ADDR_TAIL_1);
        } else {
            tailFmt = GameStr(ADDR_TAIL_0);
        }

        targetName = GetNameByBit(targetBit);
        MemTalkDebug_LogTextBytes("tail.targetName", targetName, 48);
        MemTalkDebug_LogTextBytes("tail.fmt", tailFmt, 96);
        sprintf(buffer, tailFmt, targetName);
        MemTalkDebug_LogTextBytes("tail.buffer.beforeTokenEngine", buffer, 160);
        result = ShowTokenizedText(0, speakerBit, targetBit, 0, buffer);
        MemTalkDebug_Log("display.tail.done result=%08X", (unsigned int)(uintptr_t)result);
    }

    return result;
}
