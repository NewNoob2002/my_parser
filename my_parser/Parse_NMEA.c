/**
 * @file Parse_NMEA.c
 * @brief NMEA协议解析器 - 功能实现
 * @details
 * @version 1.0
 * @date 2024-12
 */

#include "Parse_NMEA.h"

// 为校验和、回车、换行和字符串结束符预留空间
#define NMEA_BUFFER_OVERHEAD (1 + 2 + 2 + 1)

//----------------------------------------
// NMEA 解析状态机函数
//----------------------------------------

// 前向声明内部状态函数
static bool sempNmeaLineFeed(SEMP_PARSE_STATE *parse, uint8_t data);
static bool sempNmeaCarriageReturn(SEMP_PARSE_STATE *parse, uint8_t data);
static bool sempNmeaLineTermination(SEMP_PARSE_STATE *parse, uint8_t data);
static bool sempNmeaChecksumByte2(SEMP_PARSE_STATE *parse, uint8_t data);
static bool sempNmeaChecksumByte1(SEMP_PARSE_STATE *parse, uint8_t data);
static bool sempNmeaFindAsterisk(SEMP_PARSE_STATE *parse, uint8_t data);
static bool sempNmeaFindFirstComma(SEMP_PARSE_STATE *parse, uint8_t data);

// 验证校验和
static void sempNmeaValidateChecksum(SEMP_PARSE_STATE *parse)
{
    int checksum;
    SEMP_SCRATCH_PAD *scratchPad = (SEMP_SCRATCH_PAD *)parse->scratchPad;

    // 将校验和字符转换为二进制
    checksum = semp_util_asciiToNibble(parse->buffer[parse->msg_length - 2]) << 4;
    checksum |= semp_util_asciiToNibble(parse->buffer[parse->msg_length - 1]);

    // 验证校验和
    if ((checksum == parse->crc) || (parse->badCrc && (!parse->badCrc(parse))))
    {
        // 添加回车和换行符
        parse->buffer[parse->msg_length++] = '\r';
        parse->buffer[parse->msg_length++] = '\n';

        // 添加字符串结束符
        parse->buffer[parse->msg_length] = 0;

        // 调用EOM回调
        parse->eomCallback(parse, parse->parser_type);
    }
    else
    {
        // 打印校验和错误信息
        sempPrintf(parse->printDebug,
                   "SEMP: %s NMEA %s, 0x%04x (%d) bytes, bad checksum, received 0x%c%c, computed: 0x%02x",
                   parse->parserName,
                   scratchPad->nmea.sentenceName,
                   parse->msg_length, parse->msg_length,
                   parse->buffer[parse->msg_length - 2],
                   parse->buffer[parse->msg_length - 1],
                   parse->crc);
    }
}

// 读取换行符
static bool sempNmeaLineFeed(SEMP_PARSE_STATE *parse, uint8_t data)
{
    parse->msg_length -= 1; // 不将当前字符计入长度

    if (data == '\n')
    {
        sempNmeaValidateChecksum(parse);
        parse->state = sempFirstByte;
        return true;
    }

    sempNmeaValidateChecksum(parse);
    return sempFirstByte(parse, data);
}

// 读取回车符
static bool sempNmeaCarriageReturn(SEMP_PARSE_STATE *parse, uint8_t data)
{
    parse->msg_length -= 1;

    if (data == '\r')
    {
        sempNmeaValidateChecksum(parse);
        parse->state = sempFirstByte;
        return true;
    }
    
    sempNmeaValidateChecksum(parse);
    return sempFirstByte(parse, data);
}

// 读取行终止符
static bool sempNmeaLineTermination(SEMP_PARSE_STATE *parse, uint8_t data)
{
    parse->msg_length -= 1;

    if (data == '\r')
    {
        parse->state = sempNmeaLineFeed;
        return true;
    }
    else if (data == '\n')
    {
        parse->state = sempNmeaCarriageReturn;
        return true;
    }

    sempNmeaValidateChecksum(parse);
    return sempFirstByte(parse, data);
}

// 读取第二个校验和字节
static bool sempNmeaChecksumByte2(SEMP_PARSE_STATE *parse, uint8_t data)
{
    if (semp_util_asciiToNibble(parse->buffer[parse->msg_length - 1]) >= 0)
    {
        parse->state = sempNmeaLineTermination;
        return true;
    }

    sempPrintf(parse->printDebug, "SEMP %s: NMEA invalid second checksum character", parse->parserName);
    return sempFirstByte(parse, data);
}

// 读取第一个校验和字节
static bool sempNmeaChecksumByte1(SEMP_PARSE_STATE *parse, uint8_t data)
{
    if (semp_util_asciiToNibble(parse->buffer[parse->msg_length - 1]) >= 0)
    {
        parse->state = sempNmeaChecksumByte2;
        return true;
    }
    
    sempPrintf(parse->printDebug, "SEMP %s: NMEA invalid first checksum character", parse->parserName);
    return sempFirstByte(parse, data);
}

// 寻找星号'*'
static bool sempNmeaFindAsterisk(SEMP_PARSE_STATE *parse, uint8_t data)
{
    if (data == '*')
    {
        parse->state = sempNmeaChecksumByte1;
    }
    else
    {
        parse->crc ^= data; // 包含在校验和计算中
        if ((uint32_t)(parse->msg_length + NMEA_BUFFER_OVERHEAD) > parse->buffer_length)
        {
            sempPrintf(parse->printDebug, "SEMP %s: NMEA sentence too long, increase buffer size > %d", parse->parserName, parse->buffer_length);
            return sempFirstByte(parse, data);
        }
    }
    return true;
}

// 寻找第一个逗号','
static bool sempNmeaFindFirstComma(SEMP_PARSE_STATE *parse, uint8_t data)
{
    SEMP_SCRATCH_PAD *scratchPad = (SEMP_SCRATCH_PAD *)parse->scratchPad;
    parse->crc ^= data;

    if ((data != ',') || (scratchPad->nmea.sentenceNameLength == 0))
    {
        uint8_t upper = data & ~0x20;
        if (((upper < 'A') || (upper > 'Z')) && ((data < '0') || (data > '9')))
        {
            sempPrintf(parse->printDebug, "SEMP %s: NMEA invalid sentence name character 0x%02x", parse->parserName, data);
            return sempFirstByte(parse, data);
        }

        if (scratchPad->nmea.sentenceNameLength == (sizeof(scratchPad->nmea.sentenceName) - 1))
        {
            sempPrintf(parse->printDebug, "SEMP %s: NMEA sentence name > %ld characters", parse->parserName, sizeof(scratchPad->nmea.sentenceName) - 1);
            return sempFirstByte(parse, data);
        }
        
        scratchPad->nmea.sentenceName[scratchPad->nmea.sentenceNameLength++] = data;
    }
    else
    {
        scratchPad->nmea.sentenceName[scratchPad->nmea.sentenceNameLength] = 0; // C string terminator
        parse->state = sempNmeaFindAsterisk;
    }
    return true;
}

// 检查前导符
bool sempNmeaPreamble(SEMP_PARSE_STATE *parse, uint8_t data)
{
    SEMP_SCRATCH_PAD *scratchPad = (SEMP_SCRATCH_PAD *)parse->scratchPad;
    if (data != '$' && data != '!')
    {
        return false;
    }
    
    if(scratchPad)
    {
        scratchPad->nmea.sentenceNameLength = 0;
    }

    parse->state = sempNmeaFindFirstComma;
    return true;
} 