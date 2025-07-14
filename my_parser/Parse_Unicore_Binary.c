/**
 * @file Parse_Unicore_Binary.c
 * @brief 和芯星通Unicore二进制协议解析器 - 功能实现
 * @version 1.0
 * @date 2024-12
 */

#include "Parse_Unicore_Binary.h"
#include "Message_Parser.h" // 包含主解析器头文件以获取CRC函数等

//----------------------------------------
// Unicore 二进制协议CRC计算
//----------------------------------------
static uint32_t sempUnicoreBinaryComputeCrc(SEMP_PARSE_STATE *parse, uint8_t data)
{
    // 复用主解析器中的CRC32计算
    // return semp_computeCrc32(parse, data);
}

//----------------------------------------
// 状态机函数 (前向声明)
//----------------------------------------
static bool sempUnicoreBinaryReadCrc(SEMP_PARSE_STATE *parse, uint8_t data);
static bool sempUnicoreBinaryReadData(SEMP_PARSE_STATE *parse, uint8_t data);
static bool sempUnicoreBinaryReadHeader(SEMP_PARSE_STATE *parse, uint8_t data);
static bool sempUnicoreBinarySync3(SEMP_PARSE_STATE *parse, uint8_t data);
static bool sempUnicoreBinarySync2(SEMP_PARSE_STATE *parse, uint8_t data);

// 读取CRC
static bool sempUnicoreBinaryReadCrc(SEMP_PARSE_STATE *parse, uint8_t data)
{
    SEMP_SCRATCH_PAD *scratchPad = (SEMP_SCRATCH_PAD *)parse->scratchPad;

    if (--scratchPad->unicoreBinary.bytesRemaining)
        return true;

    if ((!parse->crc) || (parse->badCrc && (!parse->badCrc(parse))))
    {
        parse->eomCallback(parse, parse->parser_type);
    }
    else
    {
        sempPrintf(parse->printDebug, "SEMP: %s Unicore, bad CRC", parse->parserName);
    }
    
    parse->state = sempFirstByte;
    return false;
}

// 读取消息数据
static bool sempUnicoreBinaryReadData(SEMP_PARSE_STATE *parse, uint8_t data)
{
    SEMP_SCRATCH_PAD *scratchPad = (SEMP_SCRATCH_PAD *)parse->scratchPad;

    if (!--scratchPad->unicoreBinary.bytesRemaining)
    {
        scratchPad->unicoreBinary.bytesRemaining = 4;
        scratchPad->unicoreBinary.crc = parse->crc;
        parse->state = sempUnicoreBinaryReadCrc;
    }
    return true;
}

// 读取消息头
static bool sempUnicoreBinaryReadHeader(SEMP_PARSE_STATE *parse, uint8_t data)
{
    SEMP_SCRATCH_PAD *scratchPad = (SEMP_SCRATCH_PAD *)parse->scratchPad;

    if (parse->msg_length >= sizeof(SEMP_UNICORE_HEADER))
    {
        SEMP_UNICORE_HEADER *header = (SEMP_UNICORE_HEADER *)parse->buffer;
        scratchPad->unicoreBinary.bytesRemaining = header->messageLength;
        parse->state = sempUnicoreBinaryReadData;
    }
    return true;
}

// 读取同步字节3
static bool sempUnicoreBinarySync3(SEMP_PARSE_STATE *parse, uint8_t data)
{
    if (data != 0xB5)
        return sempFirstByte(parse, data);
    
    parse->state = sempUnicoreBinaryReadHeader;
    return true;
}

// 读取同步字节2
static bool sempUnicoreBinarySync2(SEMP_PARSE_STATE *parse, uint8_t data)
{
    if (data != 0x44)
        return sempFirstByte(parse, data);

    parse->state = sempUnicoreBinarySync3;
    return true;
}

// 检查前导符
bool sempUnicoreBinaryPreamble(SEMP_PARSE_STATE *parse, uint8_t data)
{
    if (data != 0xAA)
        return false;

    parse->crc = 0;
    parse->computeCrc = sempUnicoreBinaryComputeCrc;
    parse->crc = parse->computeCrc(parse, data);
    parse->state = sempUnicoreBinarySync2;
    return true;
} 