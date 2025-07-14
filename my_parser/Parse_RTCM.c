/**
 * @file Parse_RTCM.c
 * @brief RTCM协议解析器 - 功能实现
 * @version 1.0
 * @date 2024-12
 */

#include "Parse_RTCM.h"
#include "../lib/semp_crc24q.h" // 包含CRC24Q查找表

//----------------------------------------
// RTCM CRC计算
//----------------------------------------
static uint32_t sempRtcmComputeCrc24q(SEMP_PARSE_STATE *parse, uint8_t data)
{
    uint32_t crc = parse->crc;
    crc = ((parse)->crc << 8) ^ semp_crc24qTable[data ^ (((parse)->crc >> 16) & 0xff)];
    return crc & 0x00ffffff;
}

//----------------------------------------
// RTCM 解析状态机函数
//----------------------------------------
static bool sempRtcmReadCrc(SEMP_PARSE_STATE *parse, uint8_t data);
static bool sempRtcmReadData(SEMP_PARSE_STATE *parse, uint8_t data);
static bool sempRtcmReadMessage2(SEMP_PARSE_STATE *parse, uint8_t data);
static bool sempRtcmReadMessage1(SEMP_PARSE_STATE *parse, uint8_t data);
static bool sempRtcmReadLength2(SEMP_PARSE_STATE *parse, uint8_t data);
static bool sempRtcmReadLength1(SEMP_PARSE_STATE *parse, uint8_t data);


// 读取CRC
static bool sempRtcmReadCrc(SEMP_PARSE_STATE *parse, uint8_t data)
{
    SEMP_SCRATCH_PAD *scratchPad = (SEMP_SCRATCH_PAD *)parse->scratchPad;
    scratchPad->rtcm.bytesRemaining--;

    if (scratchPad->rtcm.bytesRemaining > 0)
        return true;

    if ((parse->crc == 0) || (parse->badCrc && (!parse->badCrc(parse))))
    {
        parse->eomCallback(parse, parse->parser_type);
    }
    else
    {
        sempPrintf(parse->printDebug,
                   "SEMP: %s RTCM %d, 0x%04x (%d) bytes, bad CRC, computed: %06x",
                   parse->parserName,
                   scratchPad->rtcm.message,
                   parse->msg_length, parse->msg_length,
                   scratchPad->rtcm.crc);
    }

    parse->state = sempFirstByte;
    return false;
}

// 读取数据
static bool sempRtcmReadData(SEMP_PARSE_STATE *parse, uint8_t data)
{
    SEMP_SCRATCH_PAD *scratchPad = (SEMP_SCRATCH_PAD *)parse->scratchPad;
    scratchPad->rtcm.bytesRemaining--;

    if (scratchPad->rtcm.bytesRemaining <= 0)
    {
        scratchPad->rtcm.crc = parse->crc;
        scratchPad->rtcm.bytesRemaining = 3;
        parse->state = sempRtcmReadCrc;
    }
    return true;
}

// 读取消息ID低4位
static bool sempRtcmReadMessage2(SEMP_PARSE_STATE *parse, uint8_t data)
{
    SEMP_SCRATCH_PAD *scratchPad = (SEMP_SCRATCH_PAD *)parse->scratchPad;
    scratchPad->rtcm.message |= data >> 4;
    scratchPad->rtcm.bytesRemaining--;
    parse->state = sempRtcmReadData;
    return true;
}

// 读取消息ID高8位
static bool sempRtcmReadMessage1(SEMP_PARSE_STATE *parse, uint8_t data)
{
    SEMP_SCRATCH_PAD *scratchPad = (SEMP_SCRATCH_PAD *)parse->scratchPad;
    scratchPad->rtcm.message = data << 4;
    scratchPad->rtcm.bytesRemaining--;
    parse->state = sempRtcmReadMessage2;
    return true;
}

// 读取长度低8位
static bool sempRtcmReadLength2(SEMP_PARSE_STATE *parse, uint8_t data)
{
    SEMP_SCRATCH_PAD *scratchPad = (SEMP_SCRATCH_PAD *)parse->scratchPad;
    scratchPad->rtcm.bytesRemaining |= data;
    parse->state = sempRtcmReadMessage1;
    return true;
}

// 读取长度高2位
static bool sempRtcmReadLength1(SEMP_PARSE_STATE *parse, uint8_t data)
{
    SEMP_SCRATCH_PAD *scratchPad = (SEMP_SCRATCH_PAD *)parse->scratchPad;
    if (data & (~3))
    {
        return sempFirstByte(parse, data);
    }
    scratchPad->rtcm.bytesRemaining = data << 8;
    parse->state = sempRtcmReadLength2;
    return true;
}

// 检查前导符
bool sempRtcmPreamble(SEMP_PARSE_STATE *parse, uint8_t data)
{
    if (data == 0xd3)
    {
        parse->computeCrc = sempRtcmComputeCrc24q;
        parse->crc = parse->computeCrc(parse, data);
        parse->state = sempRtcmReadLength1;
        return true;
    }
    return false;
} 