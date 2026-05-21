#pragma once
#include <stdint.h>

#define EVA_ENCODING_ERROR_ARGUMENT -1
#define EVA_ENCODING_ERROR_INPUT -2
#define EVA_ENCODING_ERROR_OUTPUT -3
#define EVA_ENCODING_ERROR_UNMAPPABLE -4

uint16_t translate_code(uint16_t code);
uint16_t modified_to_utf16(uint16_t code);

// Reversed from Binary.
// FUN_08884680
uint16_t sjis_to_utf16(uint16_t sjis);
// FUN_08884724
int binary_search(uint16_t sjis, int low, int high);

/*
 * Convert EVA's extended SJIS bytes to UTF-8.
 *
 * The converter uses the same SJIS -> UTF-16 tables as translate_code().
 * Bytes in the custom lead range 0xA6-0xDD are consumed as two-byte custom
 * character codes, so pass an explicit input length instead of assuming a
 * regular C string.
 *
 * On success, returns the UTF-8 byte count excluding the trailing '\0'.
 * The output buffer is always NUL-terminated on success, so output_size must
 * include room for that terminator.
 */
int eva_sjis_to_utf8(const uint8_t *input, int input_size, char *output, int output_size);

/*
 * Convert BMP UTF-8 to EVA's extended SJIS bytes.
 *
 * The reverse path prefers original game SJIS entries before custom entries,
 * matching the Python encoder's "Shift-JIS first, custom table second" policy.
 * Four-byte UTF-8 scalar values are rejected because the game mapping tables
 * only store UTF-16 BMP characters.
 *
 * On success, returns the EVA SJIS byte count excluding the trailing '\0'.
 * The output buffer is always NUL-terminated on success, so output_size must
 * include room for that terminator.
 */
int utf8_to_eva_sjis(const char *input, int input_size, uint8_t *output, int output_size);
