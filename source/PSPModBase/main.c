//
// PSP userspace module example for modding games
// A module that can be used as a basis to build mods for PSP games and apps
// 
// by xan1242 / Tenjoin 
// 
// This is based on PSP modules from Widescreen Fixes Pack by ThirteenAG (and others!)
// https://github.com/ThirteenAG/WidescreenFixesPack
// 
// NOTE: in order to load this on a CFW PSP, you will need to bootstrap it with a kernel module!
// It will fail to load if the module size is bigger than certain size.
// PPSSPP works fine otherwise.
// 
// NOTE 2: Since this is a userspace plugin, please be aware that for any kernel function,
// you MUST use bridging functions (such as kubridge or sctrl functions)
// You may NOT even *link* against or include any protected kernel functions, or else the module will NOT start!
// 


#include <pspsdk.h>
#include <pspuser.h>
#include <pspctrl.h>
#include <stdint.h>
#include <systemctrl.h>
#include <kubridge.h>
#include <stdio.h>
#include <string.h>

// We have to use paths relative to the source file (unless you edit the makefile)
#include "../../includes/psp/injector.h"
#include "../../includes/psp/inireader.h"

#include "encoding/transform.h"
#include "system/system.h"

// Define the name of the game's main module here
// The easiest way this can be found is by using PPSSPP's debugger
// Example: GTA Vice City Stories and Liberty City Stories both have the module name of "GTA3"
#define MODULE_NAME_INTERNAL "USER_MAIN"

// This is the name of this module that will be presented to the PSP OS.
// We also use it here as the base name for the ini and log files.
// Best practice is to rename this to match your project name
#define MODULE_NAME "PSPModBase"

#define MODULE_VERSION_MAJOR 1
#define MODULE_VERSION_MINOR 0

// Uncomment for logging
// We use a global definition like so to reduce final binary size (which is very important because PSP is memory constrained!)
// #define LOG

// We ignore Intellisense here to reduce squiggles in VS
#ifndef __INTELLISENSE__
PSP_MODULE_INFO(MODULE_NAME, 0, MODULE_VERSION_MAJOR, MODULE_VERSION_MINOR);
#endif

// Forward-declare initialization function
int MainInit();

int bPPSSPP = 0;
static STMOD_HANDLER previous;

#define INI_NAME MODULE_NAME ".ini"
char inipath[128] = "ms0:/seplugins/" INI_NAME;

#ifdef LOG
#define LOG_NAME MODULE_NAME ".log"
// Default initialized path
char logpath[128] = "ms0:/seplugins/" LOG_NAME;

//
// A basic printf logger that writes to a file.
//
int logPrintf(const char* text, ...) {
    va_list list;
    char string[512];

    va_start(list, text);
    vsprintf(string, text, list);
    va_end(list);

    SceUID fd = sceIoOpen(logpath, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
    if (fd >= 0) {
        sceIoWrite(fd, string, strlen(string));
        sceIoWrite(fd, "\n", 1);
        sceIoClose(fd);
    }

    return 0;
}
#endif

//
// CheckModules
// Executes only once on startup
// This works both on a real PSP and PPSSPP, but we limit this to PPSSPP because kuKernelFindModuleByName is better
//
static void CheckModules()
{
    SceUID modules[10];
    int count = 0;
    int bFoundMainModule = 0;
    int bFoundInternalModule = 0;
    if (sceKernelGetModuleIdList(modules, sizeof(modules), &count) >= 0)
    {
        int i;
        SceKernelModuleInfo info;
        for (i = 0; i < count; ++i)
        {
            info.size = sizeof(SceKernelModuleInfo);
            if (sceKernelQueryModuleInfo(modules[i], &info) < 0)
            {
                continue;
            }
            if (strcmp(info.name, MODULE_NAME_INTERNAL) == 0)
            {
#ifdef LOG
                logPrintf("Found module " MODULE_NAME_INTERNAL);
                logPrintf("text_addr: 0x%X\ntext_size: 0x%X", info.text_addr, info.text_size);
#endif
                injector.SetGameBaseAddress(info.text_addr, info.text_size);

                bFoundMainModule = 1;
            }
            else if (strcmp(info.name, MODULE_NAME) == 0)
            {
#ifdef LOG
                logPrintf("PRX module " MODULE_NAME);
                logPrintf("text_addr: 0x%X\ntext_size: 0x%X", info.text_addr, info.text_addr);
#endif
                injector.SetModuleBaseAddress(info.text_addr, info.text_size);

                bFoundInternalModule = 1;
            }
    }
}

    if (bFoundInternalModule)
    {
        if (bFoundMainModule)
        {
            MainInit();
        }
    }

    // Since we can't use OnModuleStart like on a PSP CFW, we have to scan for modules again
    // if we want to intercept another one. Read the note at the bottom of OnModuleStart for more info.

    return;
}

//
// CheckModulesPSP
// Executes only once on startup
// Works only on PSP CFW
//
void CheckModulesPSP()
{
    SceModule mod = { 0 };
    int kuErrCode = kuKernelFindModuleByName(MODULE_NAME_INTERNAL, &mod);
    if (kuErrCode != 0)
        return;

    SceModule this_module = { 0 };
    kuErrCode = kuKernelFindModuleByName(MODULE_NAME, &this_module);
    if (kuErrCode != 0)
        return;

    injector.SetGameBaseAddress(mod.text_addr, mod.text_size);
    injector.SetModuleBaseAddress(this_module.text_addr, this_module.text_addr);

    MainInit();
}

//
// OnModuleStart
// Executes any time a module is started
// This currently only works on PSP CFW, not on PPSSPP
//
// You can use this to hook any subsequently loaded modules.
//
int OnModuleStart(SceModule* mod) 
{
#ifdef LOG
    logPrintf("OnModuleStart: %s", mod->modname);
#endif

    // You can intercept other modules with new initializers and new base addresses.
    // There are some games with separate modules so you will cases where you have to switch around.
    // injector only works with one at a time, so keep that in mind and update the base addresses accordingly!

    // To search for a module, you may use the 'mod' argument here.
    // Example:
    // if (strcmp(mod->modname, "MyModuleName") == 0)
    // {
    //      Hook stuff here...
    // }
    //

    if (!previous)
        return 0;
    
    // This passes the call to the next hook that may or may not be there
    return previous(mod);
}

void SetDefaultPaths()
{
    if (bPPSSPP) 
    {
        strcpy(inipath, "ms0:/PSP/PLUGINS/" MODULE_NAME "/" INI_NAME);
#ifdef LOG
        strcpy(logpath, "ms0:/PSP/PLUGINS/" MODULE_NAME "/" LOG_NAME);
#endif
    }
    else 
    { 
        strcpy(inipath, "ms0:/seplugins/" INI_NAME);
#ifdef LOG
        strcpy(logpath, "ms0:/seplugins/" LOG_NAME);
#endif
    }
}

int module_start(SceSize argc, void* argp) 
{
    char* ptr_path;
    // If a kemulator interface exists, we know that we're in an emulator
    if (sceIoDevctl("kemulator:", 0x00000003, NULL, 0, NULL, 0) == 0) 
        bPPSSPP = 1;

    if (argc > 0) 
    { 
        // on real hardware we use module_start's argp path
        // location depending on where prx is loaded from
        strcpy(inipath, (char*)argp);
        ptr_path = strrchr(inipath, '/');
        if (ptr_path)
            strcpy(ptr_path + 1, INI_NAME);
        else
            SetDefaultPaths();
#ifdef LOG
        strcpy(logpath, (char*)argp);
        ptr_path = strrchr(logpath, '/');
        if (ptr_path)
            strcpy(ptr_path + 1, LOG_NAME);
        else
            SetDefaultPaths();
#endif
    }
    else 
    { 
        // no arguments found
        SetDefaultPaths();
    }

    if (bPPSSPP)
        CheckModules(); // scan the modules using normal/official syscalls (https://github.com/hrydgard/ppsspp/pull/13335#issuecomment-689026242)
    else // PSP
    {
        CheckModulesPSP();
        previous = sctrlHENSetStartModuleHandler(OnModuleStart);
    }

    return 0;
}

//
// MainInit
// Put your initialization code here
//
int MainInit() {
#ifdef LOG
    logPrintf(MODULE_NAME " MainInit");
#endif

    /*
    * Here you can use the injector to modify instructions or reroute the code to
    * another place (e.g. a function in this module)
    *
    * Keep in mind that the injector recalculates addresses based on the address
    * you set with SetGameBaseAddress and SetModuleBaseAddress!
    *
    *
    * Example: we can make a call to a function "MyFunction" like so:
    * injector.MakeCALL(0x189560, (uintptr_t)&MyFunction);
    *
    * Example 2: we can write a NOP at any instruction like so:
    * injector.MakeNOP(0x1477DC);
    *
    * For more examples of usage, check WidescreenFixesPack:
    * https://github.com/ThirteenAG/WidescreenFixesPack
    */

    // In this example we'll simply set up the ini reader, read the value from the ini and print it
    // The ini in question in the source here is: data/PSPModBase.ini

    // Set up the inireader path
    inireader.SetIniPath(inipath);

    // Extend SHIFT-JIS charset to support more characters (e.g. Chinese characters)
    int iniExtendCharset = inireader.ReadInteger("PATCHES", "EnableExtendCharset", 0);
#ifdef LOG
    logPrintf("extend charset is: %d\n", iniExtendCharset);
#endif

    if (iniExtendCharset)
    {
        // BIN_COND 修改：扩展边界到 0xa6
        uint32_t bin_instr = sltiu(v0, a0, 0xa6);
        injector.WriteInstr(0x08874260, bin_instr);

        // EVS_COND 修改：扩展边界到 0xa6
        uint32_t evs_instr = slti(v0, a0, 0xa6);
        injector.WriteInstr(0x08819d68, evs_instr);
    }

    // Patch the SHIFT-JIS to UTF-16 translation function to support the extended charset
    int iniTranslateCode = inireader.ReadInteger("PATCHES", "EnableCharTranslationHook", 0);
#ifdef LOG
    logPrintf("translate code is: %d", iniTranslateCode);
#endif

    if (iniTranslateCode)
    {
        // 字符翻译钩子
        injector.MakeJAL(0x088691b8, (uintptr_t)translate_code);
    }

    int iniEnableCustomFontPatch = inireader.ReadInteger("PATCHES", "EnableCustomFontPatch", 0);
#ifdef LOG
    logPrintf("custom font patch is: %d", iniEnableCustomFontPatch);
#endif

    if(iniEnableCustomFontPatch)
    {
        // 字体钩子
        injector.MakeJAL(0x08869040, (uintptr_t)sceFtttNewLib);
        injector.MakeJAL(0x088692f8, (uintptr_t)sceFtttOpen);
        injector.MakeJAL(0x08869310, (uintptr_t)sceFtttGetFontInfo);
    }

    // TODO: EnableStringPatches

    // TODO: EnableExternalTranslation

    // TODO: EnableSaveDataPatch
    int iniEnableSaveDataPatch = inireader.ReadInteger("PATCHES", "EnableSaveDataPatch", 0);
#ifdef LOG
    logPrintf("save data patch is: %d", iniEnableSaveDataPatch);
#endif

    if (iniEnableSaveDataPatch)
    {
        injector.MakeJAL(0x0880b4e4, (uintptr_t)sceUtilitySavedataInitStartPatched);
        injector.MakeJAL(0x0880b6f0, (uintptr_t)sceUtilitySavedataInitStartPatched);
        injector.MakeJAL(0x0880baf8, (uintptr_t)sceUtilitySavedataInitStartPatched);
        injector.MakeJAL(0x0880bce0, (uintptr_t)sceUtilitySavedataInitStartPatched);
        injector.MakeJAL(0x0880d97c, (uintptr_t)sceUtilitySavedataInitStartPatched);
        injector.MakeJAL(0x0880e1d4, (uintptr_t)sceUtilitySavedataInitStartPatched);
    }

    // TODO: EnableMessageDialogPatch
    int iniEnableMessageDialogPatch = inireader.ReadInteger("PATCHES", "EnableMessageDialogPatch", 0);
#ifdef LOG
    logPrintf("message dialog patch is: %d", iniEnableMessageDialogPatch);
#endif
    if (iniEnableMessageDialogPatch)
    {
        uint32_t li_instr = li(v1, 0x0b); // li v1, 0x0b or addiu $v1, $zero, 0x000B
        injector.WriteInstr(0x0880d024, li_instr);
        uint32_t sw_instr = sw(v1, s1, 4); // sw v1, 4(s1)
        injector.WriteInstr(0x0880d02c, sw_instr);
    }

    // TODO: EnablePulseAutowin
    int iniEnablePulseAutowin = inireader.ReadInteger("CHEATS", "EnablePulseAutowin", 0);
#ifdef LOG
    logPrintf("pulse autowin is: %d", iniEnablePulseAutowin);
#endif
    if (iniEnablePulseAutowin)
    {
        injector.WriteMemory32(0x0885b478, 0x00000000);
        injector.WriteMemory32(0x0885b5fc, 0x00000000);
        // injector.WriteMemory32(0x0885b5cc, 0x0a216d82);
        injector.WriteMemory32(0x0885b5c4, 0x00000000);
    }

    // TODO: ExternalTransFile

    // TODO: ExternalFontFile

    // TODO: EnableBattleDebugMenu
    int iniEnableBattleDebugMenu = inireader.ReadInteger("DEBUG", "EnableBattleDebugMenu", 0);
#ifdef LOG
    logPrintf("battle debug menu is: %d", iniEnableBattleDebugMenu);
#endif
    if (iniEnableBattleDebugMenu)
    {
        injector.WriteMemory32(0x08b57e04, 0x01);
    }

    // TODO: EnableDailyDebugMenu
    int iniEnableDailyDebugMenu = inireader.ReadInteger("DEBUG", "EnableDailyDebugMenu", 0);
#ifdef LOG
    logPrintf("daily debug menu is: %d", iniEnableDailyDebugMenu);
#endif
    if (iniEnableDailyDebugMenu)    {
        injector.WriteMemory32(0x089c97cc, 0x088984c0);
    }

    // not really necessary
    sceKernelDcacheWritebackAll();
    return 0;
}
