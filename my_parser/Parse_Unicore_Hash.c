/**
 * @file Parse_Unicore_Hash.c
 * @brief 和芯星通Unicore Hash协议解析器 - 功能实现
 * @version 1.0
 * @date 2024-12
 */

#include "Parse_Unicore_Hash.h"
#include "Message_Parser.h"
#include <string.h>

#define UNICORE_HASH_BUFFER_OVERHEAD (1 + 1 + 1)

//----------------------------------------
// 状态机函数 (前向声明)
//----------------------------------------
static void sempUnicoreHashValidateChecksum(SEMP_PARSE_STATE *parse);
static bool sempUnicoreHashLineFeed(SEMP_PARSE_STATE *parse, uint8_t data);
static bool sempUnicoreHashCarriageReturn(SEMP_PARSE_STATE *parse, uint8_t data);
static bool sempUnicoreHashLineTermination(SEMP_PARSE_STATE *parse, uint8_t data);
static bool sempUnicoreHashChecksumByte(SEMP_PARSE_STATE *parse, uint8_t data);
static bool sempUnicoreHashFindAsterisk(SEMP_PARSE_STATE *parse, uint8_t data);
static bool sempUnicoreHashFindFirstComma(SEMP_PARSE_STATE *parse, uint8_t data);


static void sempUnicoreHashValidateCrc(SEMP_PARSE_STATE *parse)
{
    SEMP_SCRATCH_PAD *scratchPad = (SEMP_SCRATCH_PAD *)parse->scratchPad;
    uint32_t crc = 0;
    uint32_t crcRx = 0;
    const uint8_t *data = &parse->buffer[1];

    do {
        crc = semp_crc32Table[(crc ^ *data) & 0xFF] ^ (crc >> 8);
    } while (*++data != '*');

    data++; // Skip '*'
    for(int i = 0; i < 8; i++) {
        crcRx = (crcRx << 4) | semp_util_asciiToNibble(*data++);
    }

    if (crc != crcRx) {
        sempPrintf(parse->printDebug, "SEMP: %s Unicore hash (#) %s, bad CRC", parse->parserName, scratchPad->unicoreHash.sentenceName);
        return;
    }

    if ((uint32_t)(parse->msg_length + UNICORE_HASH_BUFFER_OVERHEAD) > parse->buffer_length) {
        sempPrintf(parse->printDebug, "SEMP %s: Unicore hash (#) sentence too long", parse->parserName);
        parse->state = sempFirstByte;
        return;
    }

    parse->buffer[parse->msg_length++] = '\r';
    parse->buffer[parse->msg_length++] = '\n';
    parse->buffer[parse->msg_length] = 0;

    parse->eomCallback(parse, parse->parser_type);
}

static void sempUnicoreHashValidateChecksum(SEMP_PARSE_STATE *parse)
{
    SEMP_SCRATCH_PAD *scratchPad = (SEMP_SCRATCH_PAD *)parse->scratchPad;

    if (scratchPad->unicoreHash.checksumBytes > 2) {
        sempUnicoreHashValidateCrc(parse);
        return;
    }

    uint32_t checksum = (semp_util_asciiToNibble(parse->buffer[parse->msg_length - 2]) << 4) | semp_util_asciiToNibble(parse->buffer[parse->msg_length - 1]);

    if ((checksum == parse->crc) || (parse->badCrc && (!parse->badCrc(parse)))) {
        parse->buffer[parse->msg_length++] = '\r';
        parse->buffer[parse->msg_length++] = '\n';
        parse->buffer[parse->msg_length] = 0;
        parse->eomCallback(parse, parse->parser_type);
    } else {
        sempPrintf(parse->printDebug, "SEMP: %s Unicore hash (#) %s, bad checksum", parse->parserName, scratchPad->unicoreHash.sentenceName);
    }
}

static bool sempUnicoreHashLineFeed(SEMP_PARSE_STATE *parse, uint8_t data)
{
    parse->msg_length--;
    if (data == '\n') {
        sempUnicoreHashValidateChecksum(parse);
        parse->state = sempFirstByte;
        return true;
    }
    sempUnicoreHashValidateChecksum(parse);
    return sempFirstByte(parse, data);
}

static bool sempUnicoreHashCarriageReturn(SEMP_PARSE_STATE *parse, uint8_t data)
{
    parse->msg_length--;
    if (data == '\r') {
        sempUnicoreHashValidateChecksum(parse);
        parse->state = sempFirstByte;
        return true;
    }
    sempUnicoreHashValidateChecksum(parse);
    return sempFirstByte(parse, data);
}

static bool sempUnicoreHashLineTermination(SEMP_PARSE_STATE *parse, uint8_t data)
{
    parse->msg_length--;
    if (data == '\r') {
        parse->state = sempUnicoreHashLineFeed;
        return true;
    } else if (data == '\n') {
        parse->state = sempUnicoreHashCarriageReturn;
        return true;
    }
    sempUnicoreHashValidateChecksum(parse);
    return sempFirstByte(parse, data);
}

static bool sempUnicoreHashChecksumByte(SEMP_PARSE_STATE *parse, uint8_t data)
{
    SEMP_SCRATCH_PAD *scratchPad = (SEMP_SCRATCH_PAD *)parse->scratchPad;
    scratchPad->unicoreHash.bytesRemaining--;

    if (semp_util_asciiToNibble(parse->buffer[parse->msg_length - 1]) < 0) {
        sempPrintf(parse->printDebug, "SEMP %s: Unicore hash (#) invalid checksum character", parse->parserName);
        return sempFirstByte(parse, data);
    }

    if (!scratchPad->unicoreHash.bytesRemaining)
        parse->state = sempUnicoreHashLineTermination;
    return true;
}

static bool sempUnicoreHashFindAsterisk(SEMP_PARSE_STATE *parse, uint8_t data)
{
    SEMP_SCRATCH_PAD *scratchPad = (SEMP_SCRATCH_PAD *)parse->scratchPad;
    if (data == '*') {
        scratchPad->unicoreHash.bytesRemaining = scratchPad->unicoreHash.checksumBytes;
        parse->state = sempUnicoreHashChecksumByte;
    } else {
        parse->crc ^= data;
        if ((uint32_t)(parse->msg_length + UNICORE_HASH_BUFFER_OVERHEAD) > parse->buffer_length) {
            sempPrintf(parse->printDebug, "SEMP %s: Unicore hash (#) sentence too long", parse->parserName);
            return sempFirstByte(parse, data);
        }
    }
    return true;
}

static bool sempUnicoreHashFindFirstComma(SEMP_PARSE_STATE *parse, uint8_t data)
{
    SEMP_SCRATCH_PAD *scratchPad = (SEMP_SCRATCH_PAD *)parse->scratchPad;
    parse->crc ^= data;

    if ((data != ',') || (scratchPad->unicoreHash.sentenceNameLength == 0)) {
        uint8_t upper = data & ~0x20;
        if (((upper < 'A') || (upper > 'Z')) && ((data < '0') || (data > '9'))) {
            sempPrintf(parse->printDebug, "SEMP %s: Unicore hash (#) invalid sentence name character", parse->parserName);
            return sempFirstByte(parse, data);
        }

        if (scratchPad->unicoreHash.sentenceNameLength == (sizeof(scratchPad->unicoreHash.sentenceName) - 1)) {
            sempPrintf(parse->printDebug, "SEMP %s: Unicore hash (#) sentence name too long", parse->parserName);
            return sempFirstByte(parse, data);
        }

        scratchPad->unicoreHash.sentenceName[scratchPad->unicoreHash.sentenceNameLength++] = data;
    } else {
        scratchPad->unicoreHash.sentenceName[scratchPad->unicoreHash.sentenceNameLength] = 0;
        
        scratchPad->unicoreHash.checksumBytes = (strstr((const char *)scratchPad->unicoreHash.sentenceName, "MODE") != NULL) ? 2 : 8;

        parse->state = sempUnicoreHashFindAsterisk;
    }
    return true;
}

bool sempUnicoreHashPreamble(SEMP_PARSE_STATE *parse, uint8_t data)
{
    SEMP_SCRATCH_PAD *scratchPad = (SEMP_SCRATCH_PAD *)parse->scratchPad;
    if (data != '#')
        return false;
    
    scratchPad->unicoreHash.sentenceNameLength = 0;
    parse->state = sempUnicoreHashFindFirstComma;
    return true;
} 