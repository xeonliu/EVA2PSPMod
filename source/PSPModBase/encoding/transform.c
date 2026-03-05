/**
Modify the SJIS Table to store more Chinese Characters
First Byte
0x00-0x7F: ASCII
0x81-0x9F: Hiragana, Katakana, Greek, Cyrillic, etc.
0xA1-0xDF: Half-width Katakana
0xE0-0xFC: Kanji

第二字节的范围是 0x40 - 0x7E 或 0x80 - 0xFC

Use 0xA6-0xDD to store more Chinese Characters
0xA600-0xDDFF.
*/
#include <pspkernel.h>
#include <stdint.h>

#include "transform.h"

// #define UTF16_TABLE_ADDRESS 0x08a2fb60
// UTF16 Encoding
extern unsigned char UTF16_bin[14436]; // UTF16 Table

// #define DAT_08a3325c_ADDRESS 0x08a3325c
// lower bound of SJIS Encoding (u16) + Offset (u16)
// 0x20 0x00 0x00 0x00
// 0xa1 0x00 0x00 0x5f
// 0x40 0x81 0x9e 0x00
extern unsigned char SJIS_bin[360]; // SJIS Table

// GB2312 Input Space
// First Byte: 0xA1-0xF7
// Second Byte: 0xA1-0xFE
//
// Output Space: 0xA600-0xDDFF
// index: See gb2312_to_custom_map
extern unsigned char GB2312_CUSTOM_BIN[]; // UTF-16 Table for Custom Encoding

#ifdef LOG
extern int logPrintf(const char* text, ...);
#endif

/**
 * Convert UTF-16 code point to UTF-8 and store it in a buffer
 * Handles BMP (Basic Multilingual Plane) characters up to 0xFFFF
 * 
 * UTF-16 to UTF-8 conversion rules:
 * - 0x0000-0x007F: 1 byte  (0xxxxxxx)
 * - 0x0080-0x07FF: 2 bytes (110xxxxx 10xxxxxx)
 * - 0x0800-0xFFFF: 3 bytes (1110xxxx 10xxxxxx 10xxxxxx)
 */
void utf16_to_utf8(uint16_t utf16_code, char *utf8_buf, int buf_size)
{
    int utf8_len = 0;

    if (utf16_code < 0x80)
    {
        // 1-byte UTF-8
        if (buf_size < 2) return; // Ensure space for null terminator
        utf8_buf[0] = (unsigned char)utf16_code;
        utf8_len = 1;
    }
    else if (utf16_code < 0x800)
    {
        // 2-byte UTF-8
        if (buf_size < 3) return;
        utf8_buf[0] = (unsigned char)(0xC0 | (utf16_code >> 6));
        utf8_buf[1] = (unsigned char)(0x80 | (utf16_code & 0x3F));
        utf8_len = 2;
    }
    else
    {
        // 3-byte UTF-8
        if (buf_size < 4) return;
        utf8_buf[0] = (unsigned char)(0xE0 | (utf16_code >> 12));
        utf8_buf[1] = (unsigned char)(0x80 | ((utf16_code >> 6) & 0x3F));
        utf8_buf[2] = (unsigned char)(0x80 | (utf16_code & 0x3F));
        utf8_len = 3;
    }

    // Null-terminate the UTF-8 string
    utf8_buf[utf8_len] = '\0';
}

uint16_t translate_code(u16 code)
{
    if (code >= 0xA600 && code <= 0xDDFF)
    {
        return modified_to_utf16(code);
    }
    return sjis_to_utf16(code);
}

/**
Use 0xA6-0xDD to store GB2312 Chinese Characters
*/
uint16_t modified_to_utf16(u16 code)
{
#ifdef LOG
    logPrintf("Modified to UTF16: %x", code);
    if (code > 0xc332)
    {
        logPrintf("Out of Range: %x", code);
    }
#endif
    return ((u16 *)GB2312_CUSTOM_BIN)[code - 0xA600];
}

// FUN_08884680
uint16_t sjis_to_utf16(u16 sjis)
{

    u16 *DAT_08a3325c = (u16 *)(SJIS_bin);
    u16 *UTF16_TABLE = (u16 *)(UTF16_bin);
#ifdef LOG
    logPrintf("SHIFT-JIS: %x", sjis);
#endif
    int low = 0;
    int high = 0x5a;

    int index = binary_search(sjis, low, high);

    if (index == -1)
    {
        // Return '?' if not found
        return 0x003f;
    }

    // lower bound of SJIS Encoding (u16) + Offset to UTF16 Table (u16)
    uint16_t prefix = DAT_08a3325c[index << 1] & 0xFFFF;
    uint16_t offset = DAT_08a3325c[(index << 1) + 1] & 0xFFFF;

    int table_offset = sjis - prefix + offset;

    uint16_t utf16_code = UTF16_TABLE[table_offset];

    // Print UTF-16 and UTF-8 result for debugging
    char utf8_buf[4];
    utf16_to_utf8(utf16_code, utf8_buf, sizeof(utf8_buf));

#ifdef LOG
    logPrintf("UTF-16 Code: 0x%04X", utf16_code);
    logPrintf("UTF-8 Result: %s", utf8_buf);
#endif

    return utf16_code;
}

// FUN_08884724
// binary_search function
int binary_search(uint16_t target, int low, int high)
{

    u16 *DAT_08a3325c = (u16 *)(SJIS_bin);
    // u16 *UTF16_TABLE = (u16 *)(UTF16_bin);
    low = low & 0xFFFF;
    high = high & 0xFFFF;

    while (low <= high)
    {
        int mid = ((low + high) >> 1);

        // 0x44 0x29 0x8b 0x8f
        uint16_t mid_val = DAT_08a3325c[mid << 1] & 0xFFFF;
        uint16_t next_val = DAT_08a3325c[(mid << 1) + 2] & 0xFFFF;

        if (target >= mid_val && (mid == high || target < next_val))
        {
#ifdef LOG
            logPrintf("Found: %x", mid);
#endif
            return mid;
        }
        else if (mid_val < target)
        {
            low = (mid + 1);
        }
        else
        {
            high = (mid - 1);
        }
    }
#ifdef LOG
    logPrintf("Not Found: %x", target);
#endif

    return -1; // 如果未找到目标值，则返回 -1
}

