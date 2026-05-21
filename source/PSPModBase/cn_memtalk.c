/*
 * Chinese MemTalk sentence builder.
 *
 * The ActionRecord selection and the game's memory text window stay in the
 * original code. This hook replaces MemTalk_ShowMemorySentence at 0x0890F080
 * so detail sentences can render Chinese event templates with {A}/{B} slots.
 *
 * Translation data lives in translation/cn_memtalk_templates.c and is
 * generated from docs/memtalk_template_translation_workbook.tsv as UTF-8.
 * The selected template is converted to EVA SJIS before it reaches the game.
 */

#include <pspuser.h>
#include <stdint.h>

#include "../../includes/psp/injector.h"
#include "cn_memtalk.h"
#include "encoding/transform.h"

#define CN_MEMTALK_ADDR_SHOW_TOKENIZED_TEXT 0x0882FD7Cu
#define CN_MEMTALK_ADDR_GET_CURRENT_PLAYER_BIT 0x08828580u
#define CN_MEMTALK_ADDR_RAND_N 0x08871778u
#define CN_MEMTALK_ADDR_GET_NAME_BY_BIT 0x088395D8u
#define CN_MEMTALK_ADDR_FORMAT_CHARACTER_MASK 0x0890F7B4u
#define CN_MEMTALK_ADDR_FORMAT_TIME_PHRASE 0x0890F8B8u
#define CN_MEMTALK_ADDR_FORMAT_LOCATION_PHRASE 0x0890F9F0u

#define CN_MEMTALK_HOOK_SHOW_MEMORY_SENTENCE 0x0890F080u
#define CN_MEMTALK_LOW24_MASK 0x00FFFFFFu

enum {
    CN_MEMTALK_SENTENCE_CAP = 512,
    CN_MEMTALK_EVENT_CAP = 384,
    CN_MEMTALK_MASK_CAP = 96,
    CN_MEMTALK_TEMPLATE_PIECE_CAP = 192
};

typedef struct CnMemTalkActionRecord {
    uint32_t timestamp;
    uint32_t maskA;
    uint32_t maskB;
    uint8_t valid;
    uint8_t locationId;
    uint16_t templateId;
    uint8_t recordType;
    uint8_t unk11;
    uint16_t sortKey;
} CnMemTalkActionRecord;

typedef void *(*CnMemTalkShowTokenizedTextFn)(int group, int speakerBit, int targetBit, int thirdBit, char *text);
typedef int (*CnMemTalkGetCurrentPlayerBitFn)(void);
typedef int (*CnMemTalkRandNFn)(int n);
typedef const char *(*CnMemTalkGetNameByBitFn)(int bit);
typedef int (*CnMemTalkFormatCharacterMaskFn)(uint32_t mask, int styleBit, char *out);
typedef const char *(*CnMemTalkFormatTimePhraseFn)(const CnMemTalkActionRecord *rec);
typedef const char *(*CnMemTalkFormatLocationPhraseFn)(const CnMemTalkActionRecord *rec, int styleBit);

static uint8_t CnMemTalk_GetSpeakerBit(const void *ctx)
{
    return *((const uint8_t *)ctx + 0x51);
}

static uint8_t CnMemTalk_GetTargetBit(const void *ctx)
{
    return *((const uint8_t *)ctx + 0x52);
}

static int CnMemTalk_StrStartsWith(const uint8_t *text, const char *ascii)
{
    while (*ascii)
    {
        if (*text++ != (uint8_t)*ascii++)
        {
            return 0;
        }
    }
    return 1;
}

static int CnMemTalk_AppendByte(char *out, int outCap, int pos, uint8_t byte)
{
    if (!out || outCap <= 0 || pos + 1 >= outCap)
    {
        return pos;
    }

    out[pos++] = (char)byte;
    out[pos] = '\0';
    return pos;
}

static int CnMemTalk_AppendText(char *out, int outCap, int pos, const char *text)
{
    const uint8_t *p = (const uint8_t *)text;

    if (!text)
    {
        return pos;
    }

    while (*p)
    {
        pos = CnMemTalk_AppendByte(out, outCap, pos, *p++);
    }
    return pos;
}

static int CnMemTalk_StrLen(const char *text)
{
    int size = 0;
    if (!text)
    {
        return 0;
    }
    while (text[size])
    {
        ++size;
    }
    return size;
}

static int CnMemTalk_RenderEvaTemplatePiece(
    char *out,
    int outCap,
    int pos,
    const char *piece,
    const char *maskAText,
    const char *maskBText)
{
    const uint8_t *p = (const uint8_t *)piece;

    if (!piece)
    {
        return pos;
    }

    while (*p)
    {
        if (CnMemTalk_StrStartsWith(p, "{A}"))
        {
            pos = CnMemTalk_AppendText(out, outCap, pos, maskAText);
            p += 3;
            continue;
        }
        if (CnMemTalk_StrStartsWith(p, "{B}"))
        {
            pos = CnMemTalk_AppendText(out, outCap, pos, maskBText);
            p += 3;
            continue;
        }

        /*
         * Keep EVA SJIS bytes paired while scanning ASCII placeholders. The
         * custom Chinese lead range and normal SJIS leads are both >= 0x80.
         */
        if (*p >= 0x80)
        {
            pos = CnMemTalk_AppendByte(out, outCap, pos, *p++);
            if (*p)
            {
                pos = CnMemTalk_AppendByte(out, outCap, pos, *p++);
            }
            continue;
        }

        pos = CnMemTalk_AppendByte(out, outCap, pos, *p++);
    }

    return pos;
}

static int CnMemTalk_RenderTemplatePiece(
    char *out,
    int outCap,
    int pos,
    const char *utf8Piece,
    const char *maskAText,
    const char *maskBText)
{
    uint8_t evaSjisPiece[CN_MEMTALK_TEMPLATE_PIECE_CAP];
    int pieceLen;

    if (!utf8Piece || !utf8Piece[0])
    {
        return pos;
    }

    pieceLen = utf8_to_eva_sjis(
        utf8Piece,
        CnMemTalk_StrLen(utf8Piece),
        evaSjisPiece,
        sizeof(evaSjisPiece));
    if (pieceLen < 0)
    {
        return pos;
    }

    return CnMemTalk_RenderEvaTemplatePiece(
        out,
        outCap,
        pos,
        (const char *)evaSjisPiece,
        maskAText,
        maskBText);
}

static int CnMemTalk_MaskContainsStyle(uint32_t mask, uint8_t styleBit)
{
    if (styleBit >= 32)
    {
        return 0;
    }
    return (mask & (1u << styleBit)) != 0;
}

static int CnMemTalk_BuildEvent(
    const CnMemTalkActionRecord *rec,
    uint8_t styleBit,
    char *out,
    int outCap)
{
    CnMemTalkFormatCharacterMaskFn FormatCharacterMask =
        (CnMemTalkFormatCharacterMaskFn)CN_MEMTALK_ADDR_FORMAT_CHARACTER_MASK;
    const CnMemTalkTemplatePair *templatePair;
    char maskAText[CN_MEMTALK_MASK_CAP];
    char maskBText[CN_MEMTALK_MASK_CAP];
    int pos = 0;

    if (!out || outCap <= 0)
    {
        return 0;
    }
    out[0] = '\0';

    if (!rec || rec->templateId >= CN_MEMTALK_TEMPLATE_COUNT)
    {
        return 0;
    }

    templatePair = &g_cnMemTalkTemplates[rec->templateId];
    if (!templatePair->prefix)
    {
        return 0;
    }

    maskAText[0] = '\0';
    maskBText[0] = '\0';
    FormatCharacterMask(rec->maskA, styleBit, maskAText);
    FormatCharacterMask(rec->maskB, styleBit, maskBText);

    /*
     * Auto templates omit {A}. Prefix a non-speaker actor before the Chinese
     * event; manual templates already place {A} where the translation needs it.
     */
    if (templatePair->subjectPolicy == CN_MEMTALK_SUBJECT_AUTO &&
        !CnMemTalk_MaskContainsStyle(rec->maskA, styleBit) &&
        maskAText[0])
    {
        pos = CnMemTalk_AppendText(out, outCap, pos, maskAText);
    }

    pos = CnMemTalk_RenderTemplatePiece(out, outCap, pos, templatePair->prefix, maskAText, maskBText);
    pos = CnMemTalk_RenderTemplatePiece(out, outCap, pos, templatePair->suffix, maskAText, maskBText);
    return pos;
}

static int CnMemTalk_AppendTalkTarget(char *out, int outCap, int pos, uint8_t targetBit)
{
    if (targetBit == 0)
    {
        return pos;
    }

    pos = CnMemTalk_AppendText(out, outCap, pos, "\x8C\xFC");
    return CnMemTalk_AppendText(out, outCap, pos, "$b");
}

static int CnMemTalk_BuildSimpleSentence(
    char *out,
    int outCap,
    uint8_t targetBit,
    const char *verbSjis)
{
    int pos = 0;

    if (!out || outCap <= 0)
    {
        return 0;
    }
    out[0] = '\0';

    pos = CnMemTalk_AppendText(out, outCap, pos, "$a");
    pos = CnMemTalk_AppendTalkTarget(out, outCap, pos, targetBit);
    pos = CnMemTalk_AppendText(out, outCap, pos, verbSjis);
    pos = CnMemTalk_AppendText(out, outCap, pos, "\xAC\x83\x8B\x8E\x93\x49\x8E\x96\x8F\xEE\x81\x42");
    return pos;
}

static int CnMemTalk_BuildDetailSentence(
    char *out,
    int outCap,
    const CnMemTalkActionRecord *rec,
    uint8_t styleBit,
    uint8_t targetBit,
    const char *verbSjis)
{
    CnMemTalkFormatTimePhraseFn FormatTimePhrase =
        (CnMemTalkFormatTimePhraseFn)CN_MEMTALK_ADDR_FORMAT_TIME_PHRASE;
    CnMemTalkFormatLocationPhraseFn FormatLocationPhrase =
        (CnMemTalkFormatLocationPhraseFn)CN_MEMTALK_ADDR_FORMAT_LOCATION_PHRASE;
    char eventText[CN_MEMTALK_EVENT_CAP];
    const char *timePhrase;
    const char *placePhrase;
    int pos = 0;

    if (!out || outCap <= 0)
    {
        return 0;
    }
    out[0] = '\0';

    if (!CnMemTalk_BuildEvent(rec, styleBit, eventText, sizeof(eventText)))
    {
        return CnMemTalk_BuildSimpleSentence(out, outCap, targetBit, verbSjis);
    }

    timePhrase = FormatTimePhrase(rec);
    placePhrase = FormatLocationPhrase(rec, styleBit);

    pos = CnMemTalk_AppendText(out, outCap, pos, "$a");
    pos = CnMemTalk_AppendTalkTarget(out, outCap, pos, targetBit);
    pos = CnMemTalk_AppendText(out, outCap, pos, verbSjis);
    pos = CnMemTalk_AppendText(out, outCap, pos, timePhrase);
    pos = CnMemTalk_AppendText(out, outCap, pos, placePhrase);
    pos = CnMemTalk_AppendText(out, outCap, pos, eventText);
    pos = CnMemTalk_AppendText(out, outCap, pos, "\x81\x42");
    return pos;
}

static void *CnMemTalk_ShowTailIfNeeded(uint8_t speakerBit, uint8_t targetBit)
{
    CnMemTalkRandNFn RandN = (CnMemTalkRandNFn)CN_MEMTALK_ADDR_RAND_N;
    CnMemTalkGetNameByBitFn GetNameByBit = (CnMemTalkGetNameByBitFn)CN_MEMTALK_ADDR_GET_NAME_BY_BIT;
    CnMemTalkShowTokenizedTextFn ShowTokenizedText =
        (CnMemTalkShowTokenizedTextFn)CN_MEMTALK_ADDR_SHOW_TOKENIZED_TEXT;
    const char *targetName;
    const char *tailText;
    char buffer[CN_MEMTALK_SENTENCE_CAP];
    int pos = 0;
    int r;

    if (speakerBit != 16 || targetBit == 0)
    {
        return 0;
    }

    r = RandN(5);
    if (r >= 0 && r < 2)
    {
        tailText = "\xAC\xFB\x90\xA5\x95\x73\x96\xBE\x94\x92\x81\x42";
    }
    else if (r == 2)
    {
        tailText = "\x96\x76\x94\x5C\x99\xDF\x90\xB4\x81\x42";
    }
    else
    {
        tailText = "\x96\x76\x94\x5C\x97\x9D\x89\xF0\x81\x42";
    }

    targetName = GetNameByBit(targetBit);
    buffer[0] = '\0';
    pos = CnMemTalk_AppendText(buffer, sizeof(buffer), pos, "\x92\x41\x90\xA5");
    pos = CnMemTalk_AppendText(buffer, sizeof(buffer), pos, targetName);
    CnMemTalk_AppendText(buffer, sizeof(buffer), pos, tailText);
    return ShowTokenizedText(0, speakerBit, targetBit, 0, buffer);
}

void *CnMemTalk_ShowMemorySentence(void *ctx, const void *recVoid, const char *verbSjis)
{
    const CnMemTalkActionRecord *rec = (const CnMemTalkActionRecord *)recVoid;
    CnMemTalkGetCurrentPlayerBitFn GetCurrentPlayerBit =
        (CnMemTalkGetCurrentPlayerBitFn)CN_MEMTALK_ADDR_GET_CURRENT_PLAYER_BIT;
    CnMemTalkShowTokenizedTextFn ShowTokenizedText =
        (CnMemTalkShowTokenizedTextFn)CN_MEMTALK_ADDR_SHOW_TOKENIZED_TEXT;
    uint8_t speakerBit;
    uint8_t targetBit;
    uint32_t overlap;
    int currentPlayerBit;
    char buffer[CN_MEMTALK_SENTENCE_CAP];
    void *result;
    void *tailResult;

    if (!ctx)
    {
        return 0;
    }

    speakerBit = CnMemTalk_GetSpeakerBit(ctx);
    targetBit = CnMemTalk_GetTargetBit(ctx);
    currentPlayerBit = GetCurrentPlayerBit();

    if (speakerBit != (uint8_t)currentPlayerBit && targetBit != (uint8_t)currentPlayerBit)
    {
        return 0;
    }

    overlap = rec ? (rec->maskA & rec->maskB & CN_MEMTALK_LOW24_MASK) : 1u;
    if (!rec || overlap)
    {
        CnMemTalk_BuildSimpleSentence(buffer, sizeof(buffer), targetBit, verbSjis);
    }
    else
    {
        CnMemTalk_BuildDetailSentence(buffer, sizeof(buffer), rec, speakerBit, targetBit, verbSjis);
    }

    result = ShowTokenizedText(0, speakerBit, targetBit, 0, buffer);
    tailResult = CnMemTalk_ShowTailIfNeeded(speakerBit, targetBit);
    return tailResult ? tailResult : result;
}

void CnMemTalk_InstallHook(void)
{
    injector.MakeJMPwNOP(CN_MEMTALK_HOOK_SHOW_MEMORY_SENTENCE, (uintptr_t)&CnMemTalk_ShowMemorySentence);
    sceKernelDcacheWritebackAll();
    sceKernelIcacheInvalidateAll();
}
