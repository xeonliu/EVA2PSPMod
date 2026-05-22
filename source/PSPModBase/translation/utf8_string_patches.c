/*
 * Fixed UTF-8 EBOOT string translations.
 *
 * These are the former plugin/src/patcher.c patch_sentence() strings. They
 * are separate from EBTRANS.BIN: that file stores EVA SJIS patch entries,
 * while these game and save-data UI strings are copied as UTF-8 bytes.
 */

#include <stdint.h>
#include <string.h>

#include "../../../includes/psp/injector.h"
#include "utf8_string_patches.h"

typedef struct Utf8StringPatch {
    uintptr_t address;
    const char *text;
} Utf8StringPatch;

static const Utf8StringPatch g_utf8StringPatches[] = {
    {0x089B4A94u, "碇真嗣"},
    {0x089B4AA4u, "惣流・明日香・兰格雷"},
    {0x089B4ACCu, "绫波丽"},
    {0x089B4ADCu, "葛城美里"},
    {0x089B4AECu, "碇源堂"},
    {0x089B4AFCu, "冬月耕造"},
    {0x089B4B10u, "赤木律子"},
    {0x089B4B20u, "伊吹摩耶"},
    {0x089B4B30u, "日向诚"},
    {0x089B4B40u, "青叶茂"},
    {0x089B4B50u, "加持良治"},
    {0x089B4B64u, "洞木光"},
    {0x089B4B74u, "铃原冬二"},
    {0x089B4B84u, "相田剑介"},
    {0x089B4B98u, "渚薰"},
    {0x089B4BA8u, "Pen Pen"},

    {0x089B4BB8u, "使徒、袭来"},
    {0x089B4BC8u, "但是、我爱这个世界"},
    {0x089B4BE8u, "丽、心的彼方"},
    {0x089B4C04u, "亲吻脆弱的地方"},
    {0x089B4C28u, "女人的战斗"},
    {0x089B4C38u, "人类补完计划"},
    {0x089B4C4Cu, "未完成的白日梦"},
    {0x089B4C64u, "女人如火"},
    {0x089B4C70u, "花样年华"},
    {0x089B4C80u, "暧昧的天空"},
    {0x089B4C90u, "Cobalt Sky"},
    {0x089B4CA8u, "VS．SEELE"},
    {0x089B4CBCu, "心中的一切"},
    {0x089B4CD8u, "从梦中醒来"},
    {0x089B4CF0u, "看见春天的人"},
    {0x089B4D04u, "折断的翅膀"},
    {0x089B4D14u, "人手难及"},
    {0x089B4D3Cu, "「芝村」平衡"},

    {0x089B4D64u, "零"},
    {0x089B4D68u, "一"},
    {0x089B4D6Cu, "二"},
    {0x089B4D70u, "三"},
    {0x089B4D74u, "四"},
    {0x089B4D78u, "五"},
    {0x089B4D7Cu, "六"},
    {0x089B4D80u, "七"},
    {0x089B4D84u, "八"},
    {0x089B4D88u, "九"},
    {0x089B4D8Cu, "十"},
    {0x089B4D90u, "第"},
    {0x089B4D94u, "话"},
    {0x089B4D98u, "「"},
    {0x089B4D9Cu, "」"},
    {0x089B4DA0u, "日目"},
    {0x089B4DA8u, "结束"},
    {0x089B4DB0u, "剧情通关文件"},
    {0x089B4DD4u, "开放剧情数"},
    {0x089B4DF0u, "完成剧情数"},

    {0x089B51C4u, "AM"},
    {0x089B51C8u, "PM"},
    {0x089B4E14u, "加载完成。"},
    {0x089B4E38u, "保存完成。"},
    {0x089B4E5Cu, "Memory Stick™空闲容量不足。\n\n"},
    {0x089B4EA8u, "本标题还需要\n"},
    {0x089B4ECCu, "游戏数据("},
    {0x089B4EE0u, "KB)和\n"},
    {0x089B4EE8u, "剧情通关数据("},
    {0x089B4F08u, "KB)的\n"},
    {0x089B4F10u, "空闲容量。\n\n"},
    {0x089B4F3Cu, "是否删除其他游戏数据？"},
    {0x089B4F74u, "是否继续游戏？"},
    {0x089B4F98u, "是否中止保存？"},
    {0x089B4FBCu, "未找到Memory Stick™。\n\n"},
    {0x089B4FF8u, "本标题需要保存游戏数据("},
    {0x089B5024u, "KB)的\n"},
    {0x089B502Cu, "空闲容量。\n\n"},
    {0x089B5070u, "中止保存，继续游戏吗？"},
    {0x089B50ACu, "无法访问Memory Stick™。\n\n"},
    {0x089B50F4u, "无法保存到Memory Stick™。\n\n是否删除其他游戏数据后再次保存？"},
    {0x089EA084u, "新世纪福音战士２　被创造的世界"},
    {0x089EA0C4u, "空闲存档槽"},
    {0x089EA0D8u, "Memory Stick™尚未完成加载。\n\n是否停止加载，继续游戏？"},
};

void Utf8StringPatches_Apply(void)
{
    unsigned int i;

    for (i = 0; i < sizeof(g_utf8StringPatches) / sizeof(g_utf8StringPatches[0]); ++i)
    {
        const Utf8StringPatch *patch = &g_utf8StringPatches[i];
        injector.WriteMemoryRaw(patch->address, (void *)patch->text, strlen(patch->text) + 1);
    }
}
