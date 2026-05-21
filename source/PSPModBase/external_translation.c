#include <pspuser.h>
#include <stdint.h>

#include "../../includes/psp/injector.h"
#include "external_translation.h"

enum {
    EXTERNAL_TRANSLATION_ENTRY_BUFFER_SIZE = 1024,
    EXTERNAL_TRANSLATION_ERROR_ARGUMENT = -1,
    EXTERNAL_TRANSLATION_ERROR_TRUNCATED = -2,
    EXTERNAL_TRANSLATION_ERROR_ENTRY_SIZE = -3
};

/*
 * EBTRANS.BIN is intentionally a raw patch list:
 *
 *   u32 entryCount
 *   repeat entryCount times:
 *     u32 ramAddress
 *     u32 byteCount
 *     u8  bytes[1024]
 *
 * The bytes are already EVA SJIS. The PSP side must write byteCount bytes
 * verbatim; converting them again would corrupt embedded custom codes.
 */
static int ExternalTranslation_ReadExact(SceUID fd, void *out, int size)
{
    uint8_t *p = (uint8_t *)out;
    int readSize = 0;

    while (readSize < size)
    {
        int result = sceIoRead(fd, p + readSize, size - readSize);
        if (result < 0)
        {
            return result;
        }
        if (result == 0)
        {
            return EXTERNAL_TRANSLATION_ERROR_TRUNCATED;
        }
        readSize += result;
    }

    return readSize;
}

int ExternalTranslation_Apply(const char *filename)
{
    static uint8_t entryBuffer[EXTERNAL_TRANSLATION_ENTRY_BUFFER_SIZE];
    uint32_t entryCount;
    uint32_t entryIndex;
    SceUID fd;
    int result;

    if (!filename || !filename[0])
    {
        return EXTERNAL_TRANSLATION_ERROR_ARGUMENT;
    }

    fd = sceIoOpen(filename, PSP_O_RDONLY, 0777);
    if (fd < 0)
    {
        return fd;
    }

    result = ExternalTranslation_ReadExact(fd, &entryCount, sizeof(entryCount));
    if (result < 0)
    {
        goto done;
    }

    for (entryIndex = 0; entryIndex < entryCount; ++entryIndex)
    {
        uint32_t ramAddress;
        uint32_t byteCount;

        result = ExternalTranslation_ReadExact(fd, &ramAddress, sizeof(ramAddress));
        if (result < 0)
        {
            goto done;
        }

        result = ExternalTranslation_ReadExact(fd, &byteCount, sizeof(byteCount));
        if (result < 0)
        {
            goto done;
        }

        result = ExternalTranslation_ReadExact(fd, entryBuffer, sizeof(entryBuffer));
        if (result < 0)
        {
            goto done;
        }

        if (byteCount > sizeof(entryBuffer))
        {
            result = EXTERNAL_TRANSLATION_ERROR_ENTRY_SIZE;
            goto done;
        }

        if (byteCount)
        {
            injector.WriteMemoryRaw((uintptr_t)ramAddress, entryBuffer, byteCount);
        }
    }

    sceKernelDcacheWritebackAll();
    result = (int)entryCount;

done:
    {
        int closeResult = sceIoClose(fd);
        if (result >= 0 && closeResult < 0)
        {
            return closeResult;
        }
    }
    return result;
}
