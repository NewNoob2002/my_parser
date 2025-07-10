/**
 * @file Parse_NMEA.c
 * @brief NMEA GPS协议解析器
 * @details 解析标准NMEA 0183 GPS协议语句，支持校验和验证
 * @version 1.0
 * @date 2024-12
 */

#include "Message_Parser.h"
#include <string.h>

//----------------------------------------
// 协议格式说明
//----------------------------------------
//
//  NMEA Sentence Format:
//  +----------+---------+--------+---------+----------+----------+----------+
//  | Preamble |  Name   | Comma  |  Data   | Asterisk | Checksum |   CRLF   |
//  |  8 bits  | n bytes | 8 bits | n bytes |  8 bits  | 2 bytes  | 2 bytes  |
//  |     $    |         |    ,   |         |    *     |    XX    |   \r\n   |
//  +----------+---------+--------+---------+----------+----------+----------+
//               |                            |
//               |<-------- Checksum -------->|
//
//  Examples:
//  $GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n
//  $GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\r\n

//----------------------------------------
// 常量定义
//----------------------------------------

// 为语句末尾的字符预留空间: *, XX, \r, \n, \0
#define NMEA_BUFFER_OVERHEAD    (1 + 2 + 2 + 1)

//----------------------------------------
// 内部状态函数前向声明
//----------------------------------------

static bool mpNmeaFindFirstComma(MP_PARSE_STATE *parse, uint8_t data);
static bool mpNmeaFindAsterisk(MP_PARSE_STATE *parse, uint8_t data);
static bool mpNmeaChecksumByte1(MP_PARSE_STATE *parse, uint8_t data);
static bool mpNmeaChecksumByte2(MP_PARSE_STATE *parse, uint8_t data);
static bool mpNmeaLineTermination(MP_PARSE_STATE *parse, uint8_t data);

//----------------------------------------
// NMEA协议解析状态机
//----------------------------------------

/**
 * @brief 验证NMEA校验和并完成消息处理
 * @param parse 解析器状态指针
 */
static void mpNmeaValidateChecksum(MP_PARSE_STATE *parse)
{
    int checksum;
    MP_SCRATCH_PAD *scratchPad = &parse->scratchPad;

    // 转换校验和字符为二进制值
    checksum = msgp_util_asciiToNibble(parse->buffer[parse->length - 2]) << 4;
    checksum |= msgp_util_asciiToNibble(parse->buffer[parse->length - 1]);

    // 验证校验和
    bool checksum_ok = (checksum == (int)(parse->crc & 0xFF));

    if (checksum_ok) {
        // 校验和正确，添加CRLF
        parse->buffer[parse->length++] = '\r';
        parse->buffer[parse->length++] = '\n';
        parse->buffer[parse->length] = 0; // 零终止符

        // 处理NMEA语句
        if (parse->eomCallback) {
            parse->eomCallback(parse, parse->protocolIndex);
        }
    } else {
        // 校验和错误
        if (parse->badCrc) {
            parse->badCrc(parse);
        }
    }
}

/**
 * @brief 处理行终止符 (\r\n)
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return false=消息处理完成
 */
static bool mpNmeaLineTermination(MP_PARSE_STATE *parse, uint8_t data)
{
    // 不添加当前字符到长度计数中
    parse->length -= 1;

    // 处理行终止符
    mpNmeaValidateChecksum(parse);
    
    // 无论成功或失败，NMEA消息到此结束，重置状态机寻找下一个包
    parse->state = NULL; //
    return false; // 消息处理完成
}

/**
 * @brief 读取第二个校验和字节
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return true=继续解析，false=状态错误
 */
static bool mpNmeaChecksumByte2(MP_PARSE_STATE *parse, uint8_t data)
{
    // 验证第二个校验和字符是否为有效的十六进制字符
    if (msgp_util_asciiToNibble(parse->buffer[parse->length - 1]) >= 0) {
        parse->state = mpNmeaLineTermination;
        return true;
    }

    // 无效的校验和字符
    msgp_util_safePrintf(parse->printDebug,
        "MP: %s NMEA无效的第二个校验和字符: 0x%02X",
        parse->parserName ? parse->parserName : "Unknown", data);
    parse->state = NULL;
    return false;
}

/**
 * @brief 读取第一个校验和字节
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return true=继续解析，false=状态错误
 */
static bool mpNmeaChecksumByte1(MP_PARSE_STATE *parse, uint8_t data)
{
    // 验证第一个校验和字符是否为有效的十六进制字符
    if (msgp_util_asciiToNibble(parse->buffer[parse->length - 1]) >= 0) {
        parse->state = mpNmeaChecksumByte2;
        return true;
    }

    // 无效的校验和字符
    msgp_util_safePrintf(parse->printDebug,
        "MP: %s NMEA无效的第一个校验和字符: 0x%02X",
        parse->parserName ? parse->parserName : "Unknown", data);
    parse->state = NULL;
    return false;
}

/**
 * @brief 查找星号（*）分隔符
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return true=继续解析，false=缓冲区溢出
 */
static bool mpNmeaFindAsterisk(MP_PARSE_STATE *parse, uint8_t data)
{
    if (data == '*') {
        // 找到星号，开始读取校验和
        parse->state = mpNmeaChecksumByte1;
    } else {
        // 包含在校验和计算中的数据字节
        parse->crc ^= data;

        // 验证缓冲区空间是否足够
        if ((uint32_t)(parse->length + NMEA_BUFFER_OVERHEAD) > parse->bufferLength) {
            // 语句过长
            msgp_util_safePrintf(parse->printDebug,
                "MP: %s NMEA语句过长, 增加缓冲区大小 > %d",
                parse->parserName ? parse->parserName : "Unknown",
                parse->bufferLength);
            parse->state = NULL;
            return false;
        }
    }
    return true;
}

/**
 * @brief 查找第一个逗号并读取语句名称
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return true=继续解析，false=格式错误
 */
static bool mpNmeaFindFirstComma(MP_PARSE_STATE *parse, uint8_t data)
{
    MP_SCRATCH_PAD *scratchPad = &parse->scratchPad;

    // 包含在校验和计算中的数据字节
    parse->crc ^= data;

    if (data == ',') {
        // 找到第一个逗号，语句名称结束
        // 将语句名称复制到暂存区，并添加零终止符
        if (scratchPad->nmea.info.sentenceNameLength < sizeof(scratchPad->nmea.info.sentenceName)) {
            scratchPad->nmea.info.sentenceName[scratchPad->nmea.info.sentenceNameLength] = '\0';
        } else {
            // 防止溢出
            scratchPad->nmea.info.sentenceName[sizeof(scratchPad->nmea.info.sentenceName) - 1] = '\0';
        }
        
        parse->state = mpNmeaFindAsterisk;
    } else {
        // 继续读取语句名称
        if (scratchPad->nmea.info.sentenceNameLength < sizeof(scratchPad->nmea.info.sentenceName) -1) {
             scratchPad->nmea.info.sentenceName[scratchPad->nmea.info.sentenceNameLength++] = data;
        } else {
            msgp_util_safePrintf(parse->printDebug, "MP: NMEA语句名称过长");
            parse->state = NULL;
            return false;
        }
    }
    return true;
}

//----------------------------------------
// 公共接口函数
//----------------------------------------

/**
 * @brief NMEA协议前导字节检测
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return true=检测到NMEA协议前导，false=不是NMEA协议
 */
bool msgp_nmea_preamble(MP_PARSE_STATE *parse, uint8_t data)
{
    if (data != '$') return false;

    parse->length = 0;
    parse->buffer[parse->length++] = data;
    parse->scratchPad.nmea.info.sentenceNameLength = 0;
    parse->crc = 0;
    parse->computeCrc = NULL;
    parse->state = mpNmeaFindFirstComma;
    
    msgp_util_safePrintf(parse->printDebug, 
        "MP: 检测到NMEA协议前导字符 '$'");
    
    return true;
}

//----------------------------------------
// NMEA协议辅助函数
//----------------------------------------

/**
 * @brief 获取NMEA语句名称
 * @param parse 解析器状态指针
 * @return NMEA语句名称字符串
 */
const char* msgp_nmea_getSentenceName(const MP_PARSE_STATE *parse)
{
    if (!parse) return "Unknown";
    
    const MP_SCRATCH_PAD *scratchPad = &parse->scratchPad;
    return (const char *)scratchPad->nmea.info.sentenceName;
}

/**
 * @brief 解析NMEA语句获取字段数据
 * @param parse 解析器状态指针
 * @param fields 输出字段数组
 * @param maxFields 最大字段数量
 * @return 实际解析的字段数量
 */
int msgp_nmea_parseFields(const MP_PARSE_STATE *parse, char fields[][32], int maxFields)
{
    if (!parse || !fields || maxFields == 0) return 0;
    
    // 我们需要一个以null结尾的字符串
    char sentence[MP_MINIMUM_BUFFER_LENGTH];
    uint16_t len = (parse->length < sizeof(sentence) -1) ? parse->length : sizeof(sentence) - 1;
    memcpy(sentence, parse->buffer, len);
    sentence[len] = '\0';

    int fieldCount = 0;
    char *p = sentence;
    
    // 跳过 '$'
    if (*p == '$') p++;
    
    fields[fieldCount][0] = '\0';
    int field_char_count = 0;
    
    while(*p && *p != '*' && fieldCount < maxFields) {
        if (*p == ',') {
            fields[fieldCount][field_char_count] = '\0';
            fieldCount++;
            if(fieldCount < maxFields) {
                fields[fieldCount][0] = '\0';
                field_char_count = 0;
            }
        } else {
            if (field_char_count < 31) {
                fields[fieldCount][field_char_count++] = *p;
            }
        }
        p++;
    }
    
    if (field_char_count > 0) {
        fields[fieldCount][field_char_count] = '\0';
        fieldCount++;
    }
    
    return fieldCount;
}

/**
 * @brief 识别NMEA语句类型
 * @param sentenceName NMEA语句名称
 * @return 语句类型描述字符串
 */
const char* msgp_nmea_getSentenceType(const char *sentenceName)
{
    if (!sentenceName) return "未知";
    
    // 常见的NMEA语句类型
    if (strncmp(sentenceName, "GPGGA", 5) == 0) {
        return "Global Positioning System Fix Data";
    } else if (strncmp(sentenceName, "GPRMC", 5) == 0) {
        return "Recommended Minimum Course";
    } else if (strncmp(sentenceName, "GPGSV", 5) == 0) {
        return "GPS Satellites in View";
    } else if (strncmp(sentenceName, "GPGSA", 5) == 0) {
        return "GPS DOP and Active Satellites";
    } else if (strncmp(sentenceName, "GPVTG", 5) == 0) {
        return "Track Made Good and Ground Speed";
    } else if (strncmp(sentenceName, "GPGLL", 5) == 0) {
        return "Geographic Position - Latitude/Longitude";
    } else if (strncmp(sentenceName, "GPZDA", 5) == 0) {
        return "Date & Time";
    } else if (strncmp(sentenceName, "GNGGA", 5) == 0) {
        return "GNSS Fix Data";
    } else if (strncmp(sentenceName, "GNRMC", 5) == 0) {
        return "GNSS Recommended Minimum Course";
    }
    
    return "Unknown NMEA Sentence";
} 