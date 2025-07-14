/**
 * @file Parse_UBLOX.c
 * @brief u-blox UBX协议解析器 - 功能实现
 * @version 1.0
 * @date 2024-12
 */

#include "Parse_UBLOX.h"

//----------------------------------------
// u-blox 解析状态机函数 (前向声明)
//----------------------------------------

static bool sempUbloxCkB(SEMP_PARSE_STATE *parse, uint8_t data);
static bool sempUbloxCkA(SEMP_PARSE_STATE *parse, uint8_t data);
static bool sempUbloxPayload(SEMP_PARSE_STATE *parse, uint8_t data);
static bool sempUbloxLength2(SEMP_PARSE_STATE *parse, uint8_t data);
static bool sempUbloxLength1(SEMP_PARSE_STATE *parse, uint8_t data);
static bool sempUbloxId(SEMP_PARSE_STATE *parse, uint8_t data);
static bool sempUbloxClass(SEMP_PARSE_STATE *parse, uint8_t data);
static bool sempUbloxSync2(SEMP_PARSE_STATE *parse, uint8_t data);


// 读取CK_B字节并验证校验和
static bool sempUbloxCkB(SEMP_PARSE_STATE *parse, uint8_t data)
{
    SEMP_SCRATCH_PAD *scratchPad = (SEMP_SCRATCH_PAD *)parse->scratchPad;

    bool badChecksum = (parse->buffer[parse->msg_length - 2] != scratchPad->ublox.ck_a) || (parse->buffer[parse->msg_length - 1] != scratchPad->ublox.ck_b);

    if (!badChecksum || (parse->badCrc && !parse->badCrc(parse)))
    {
        parse->eomCallback(parse, parse->parser_type);
    }
    else
    {
        sempPrintf(parse->printDebug,
                   "SEMP %s: UBLOX bad checksum received 0x%02x%02x computed 0x%02x%02x",
                   parse->parserName,
                   parse->buffer[parse->msg_length - 2], parse->buffer[parse->msg_length - 1],
                   scratchPad->ublox.ck_a, scratchPad->ublox.ck_b);
    }

    parse->msg_length = 0;
    parse->state = sempFirstByte;
    return false;
}

// 读取CK_A字节
static bool sempUbloxCkA(SEMP_PARSE_STATE *parse, uint8_t data)
{
    parse->state = sempUbloxCkB;
    return true;
}

// 读取负载
static bool sempUbloxPayload(SEMP_PARSE_STATE *parse, uint8_t data)
{
    SEMP_SCRATCH_PAD *scratchPad = (SEMP_SCRATCH_PAD *)parse->scratchPad;

    if (scratchPad->ublox.bytesRemaining--)
    {
        scratchPad->ublox.ck_a += data;
        scratchPad->ublox.ck_b += scratchPad->ublox.ck_a;
        return true;
    }
    return sempUbloxCkA(parse, data);
}

// 读取长度字节2
static bool sempUbloxLength2(SEMP_PARSE_STATE *parse, uint8_t data)
{
    SEMP_SCRATCH_PAD *scratchPad = (SEMP_SCRATCH_PAD *)parse->scratchPad;
    
    scratchPad->ublox.ck_a += data;
    scratchPad->ublox.ck_b += scratchPad->ublox.ck_a;

    scratchPad->ublox.bytesRemaining |= ((uint16_t)data) << 8;
    parse->state = sempUbloxPayload;
    return true;
}

// 读取长度字节1
static bool sempUbloxLength1(SEMP_PARSE_STATE *parse, uint8_t data)
{
    SEMP_SCRATCH_PAD *scratchPad = (SEMP_SCRATCH_PAD *)parse->scratchPad;

    scratchPad->ublox.ck_a += data;
    scratchPad->ublox.ck_b += scratchPad->ublox.ck_a;

    scratchPad->ublox.bytesRemaining = data;
    parse->state = sempUbloxLength2;
    return true;
}

// 读取ID
static bool sempUbloxId(SEMP_PARSE_STATE *parse, uint8_t data)
{
    SEMP_SCRATCH_PAD *scratchPad = (SEMP_SCRATCH_PAD *)parse->scratchPad;

    scratchPad->ublox.ck_a += data;
    scratchPad->ublox.ck_b += scratchPad->ublox.ck_a;

    scratchPad->ublox.message |= data;
    parse->state = sempUbloxLength1;
    return true;
}

// 读取类别
static bool sempUbloxClass(SEMP_PARSE_STATE *parse, uint8_t data)
{
    SEMP_SCRATCH_PAD *scratchPad = (SEMP_SCRATCH_PAD *)parse->scratchPad;

    scratchPad->ublox.ck_a = data;
    scratchPad->ublox.ck_b = data;

    scratchPad->ublox.message = ((uint16_t)data) << 8;
    parse->state = sempUbloxId;
    return true;
}

// 读取同步字符2
static bool sempUbloxSync2(SEMP_PARSE_STATE *parse, uint8_t data)
{
    if (data != 0x62)
    {
        sempPrintf(parse->printDebug, "SEMP %s: UBLOX invalid second sync byte", parse->parserName);
        return sempFirstByte(parse, data);
    }
    parse->state = sempUbloxClass;
    return true;
}

// 检查前导符
bool sempUbloxPreamble(SEMP_PARSE_STATE *parse, uint8_t data)
{
    if (data != 0xb5)
    {
        return false;
    }
    parse->state = sempUbloxSync2;
    return true;
} 