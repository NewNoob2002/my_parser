/**
 * @file Parse_BT.c
 * @brief 自定义二进制协议(BT)解析器 - 功能实现
 * @version 1.0
 * @date 2024-12
 */

#include "Message_Parser.h"

//----------------------------------------
// CRC计算 (复用主解析器)
//----------------------------------------
static uint32_t sempCustomComputeCrc(SEMP_PARSE_STATE *parse, uint8_t data)
{
    uint32_t crc;

    crc = parse->crc;
    crc = semp_crc32Table[(crc ^ data) & 0xff] ^ (crc >> 8);
    return crc;
}

static bool sempCustomReadCrc(SEMP_PARSE_STATE *parse, uint8_t data)
{
    SEMP_SCRATCH_PAD *scratchPad = (SEMP_SCRATCH_PAD *)parse->scratchPad;

    // Determine if the entire message was read
    if (--scratchPad->custom.bytesRemaining)
        // Need more bytes
        return true;

    // uint32_t crcRead = parse->crc;
    
    // uint32_t crcComputed = scratchPad->bluetooth.crc;
    // Call the end-of-message routine with this message
    if ((!parse->crc) || (parse->badCrc && (!parse->badCrc(parse))))
        parse->eomCallback(parse, parse->parser_type); // Pass parser array index
    else
    {
        sempPrintf(parse->printDebug,
                   "SEMP: %s Unicore, bad CRC, "
                   "received %02x %02x %02x %02x, computed: %02x %02x %02x %02x",
                   parse->parserName,
                   parse->buffer[parse->msg_length - 4],
                   parse->buffer[parse->msg_length - 3],
                   parse->buffer[parse->msg_length - 2],
                   parse->buffer[parse->msg_length - 1],
                   scratchPad->custom.crc & 0xff,
                   (scratchPad->custom.crc >> 8) & 0xff,
                   (scratchPad->custom.crc >> 16) & 0xff,
                   (scratchPad->custom.crc >> 24) & 0xff);
    }
    parse->state = sempFirstByte;
    return false;
}

static bool sempCustomReadData(SEMP_PARSE_STATE *parse, uint8_t data)
{
    SEMP_SCRATCH_PAD *scratchPad = (SEMP_SCRATCH_PAD *)parse->scratchPad;

    // Determine if the entire message was read
    if (!--scratchPad->custom.bytesRemaining)
    {
        // The message data is complete, read the CRC
        scratchPad->custom.bytesRemaining = 4;
        parse->crc ^= 0xFFFFFFFF;
        scratchPad->custom.crc = parse->crc;
        parse->state = sempCustomReadCrc;
    }
    return true;
}

static bool sempCustomReadHeader(SEMP_PARSE_STATE *parse, uint8_t data)
{
    SEMP_SCRATCH_PAD *scratchPad = (SEMP_SCRATCH_PAD *)parse->scratchPad;
    if (parse->msg_length >= sizeof(SEMP_CUSTOM_HEADER))
    {
        SEMP_CUSTOM_HEADER *header = (SEMP_CUSTOM_HEADER *)parse->buffer;
        scratchPad->custom.bytesRemaining = header->messageLength;
        parse->state = sempCustomReadData;
    }
    return true;
}

static bool sempCustomSync3(SEMP_PARSE_STATE *parse, uint8_t data)
{
    if (data != 0x18)
        return sempFirstByte(parse, data);
    
    parse->state = sempCustomReadHeader;
    return true;
}

static bool sempCustomSync2(SEMP_PARSE_STATE *parse, uint8_t data)
{
    if (data != 0x44)
        // Invalid sync byte, start searching for a preamble byte
        return sempFirstByte(parse, data);

    // Look for the last sync byte
    parse->state = sempCustomSync3;
    return true;
}

bool sempCustomPreamble(SEMP_PARSE_STATE *parse, uint8_t data)
{
    if (data != 0xAA)
        return false;

    // Look for the second sync byte
    parse->crc = 0xFFFFFFFF;
    parse->computeCrc = sempCustomComputeCrc;
    parse->crc = parse->computeCrc(parse, data);
    parse->state = sempCustomSync2;
    return true;
} 