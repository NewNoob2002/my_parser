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
    checksum = mpAsciiToNibble(parse->buffer[parse->length - 2]) << 4;
    checksum |= mpAsciiToNibble(parse->buffer[parse->length - 1]);

    // 验证校验和
    if ((checksum == (int)(parse->crc & 0xFF)) || 
        (parse->badCrc && (!parse->badCrc(parse)))) {
        
        // 校验和正确，添加CRLF
        parse->buffer[parse->length++] = '\r';
        parse->buffer[parse->length++] = '\n';
        parse->buffer[parse->length] = 0; // 零终止符

        // 处理NMEA语句
        parse->messagesProcessed[MP_PROTOCOL_NMEA]++;
        if (parse->eomCallback) {
            parse->eomCallback(parse, MP_PROTOCOL_NMEA);
        }
        
        mpSafePrintf(parse->printDebug,
            "MP: NMEA语句解析成功: %s (%d字节)",
            scratchPad->nmea.info.sentenceName, parse->length);
    } else {
        // 校验和错误
        parse->crcErrors[MP_PROTOCOL_NMEA]++;
        mpSafePrintf(parse->printDebug,
            "MP: %s NMEA %s, 校验和错误, 接收: %c%c, 计算: %02X",
            parse->parserName ? parse->parserName : "Unknown",
            scratchPad->nmea.info.sentenceName,
            parse->buffer[parse->length - 2], parse->buffer[parse->length - 1],
            (uint8_t)(parse->crc & 0xFF));
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
    if (data == '\r' || data == '\n') {
        mpNmeaValidateChecksum(parse);
        return false; // 消息处理完成
    }

    // 无效的行终止符，仍然验证并处理消息
    mpNmeaValidateChecksum(parse);
    return false;
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
    if (mpAsciiToNibble(parse->buffer[parse->length - 1]) >= 0) {
        parse->state = mpNmeaLineTermination;
        return true;
    }

    // 无效的校验和字符
    mpSafePrintf(parse->printDebug,
        "MP: %s NMEA无效的第二个校验和字符: 0x%02X",
        parse->parserName ? parse->parserName : "Unknown", data);
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
    if (mpAsciiToNibble(parse->buffer[parse->length - 1]) >= 0) {
        parse->state = mpNmeaChecksumByte2;
        return true;
    }

    // 无效的校验和字符
    mpSafePrintf(parse->printDebug,
        "MP: %s NMEA无效的第一个校验和字符: 0x%02X",
        parse->parserName ? parse->parserName : "Unknown", data);
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
            mpSafePrintf(parse->printDebug,
                "MP: %s NMEA语句过长, 增加缓冲区大小 > %d",
                parse->parserName ? parse->parserName : "Unknown",
                parse->bufferLength);
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
    parse->crc ^= data;
    
    if ((data != ',') || (scratchPad->nmea.info.sentenceNameLength == 0)) {
        // 验证语句名称字符的有效性
        uint8_t upper = data & ~0x20; // 转换为大写
        if (((upper < 'A') || (upper > 'Z')) && ((data < '0') || (data > '9'))) {
            mpSafePrintf(parse->printDebug,
                "MP: %s NMEA无效语句名字符: 0x%02X ('%c')",
                parse->parserName ? parse->parserName : "Unknown", data, 
                (data >= 32 && data <= 126) ? data : '?');
            return false;
        }

        // 检查语句名称长度
        if (scratchPad->nmea.info.sentenceNameLength >= (MP_MAX_SENTENCE_NAME - 1)) {
            mpSafePrintf(parse->printDebug,
                "MP: %s NMEA语句名过长 > %d 字符",
                parse->parserName ? parse->parserName : "Unknown", 
                MP_MAX_SENTENCE_NAME - 1);
            return false;
        }

        // 保存语句名称字符
        scratchPad->nmea.info.sentenceName[scratchPad->nmea.info.sentenceNameLength++] = data;
    } else {
        // 找到第一个逗号，语句名称读取完成
        scratchPad->nmea.info.sentenceName[scratchPad->nmea.info.sentenceNameLength] = 0;
        parse->state = mpNmeaFindAsterisk;
        
        mpSafePrintf(parse->printDebug,
            "MP: NMEA语句名: %s", scratchPad->nmea.info.sentenceName);
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
bool mpNmeaPreamble(MP_PARSE_STATE *parse, uint8_t data)
{
    // 检测NMEA前导字符 ($)
    if (data != '$') {
        return false;
    }
    
    // 初始化NMEA解析状态
    MP_SCRATCH_PAD *scratchPad = &parse->scratchPad;
    scratchPad->nmea.info.sentenceNameLength = 0;
    parse->crc = 0;              // NMEA校验和从0开始
    parse->computeCrc = NULL;    // NMEA使用简单异或校验
    parse->state = mpNmeaFindFirstComma;
    
    mpSafePrintf(parse->printDebug, 
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
const char* mpNmeaGetSentenceName(const MP_PARSE_STATE *parse)
{
    if (!parse) return "Unknown";
    
    const MP_SCRATCH_PAD *scratchPad = &parse->scratchPad;
    return (const char *)scratchPad->nmea.info.sentenceName;
}

/**
 * @brief 解析NMEA语句获取字段数据
 * @param sentence NMEA语句字符串
 * @param fields 输出字段数组
 * @param maxFields 最大字段数量
 * @return 实际解析的字段数量
 */
int mpNmeaParseFields(const char *sentence, char fields[][32], int maxFields)
{
    if (!sentence || !fields || maxFields <= 0) {
        return 0;
    }
    
    int fieldCount = 0;
    int fieldPos = 0;
    int sentencePos = 0;
    
    // 跳过前导 '$' 和语句名称直到第一个逗号
    while (sentence[sentencePos] && sentence[sentencePos] != ',') {
        sentencePos++;
    }
    if (sentence[sentencePos] == ',') {
        sentencePos++; // 跳过逗号
    }
    
    // 解析字段
    while (sentence[sentencePos] && sentence[sentencePos] != '*' && fieldCount < maxFields) {
        if (sentence[sentencePos] == ',') {
            // 字段结束
            fields[fieldCount][fieldPos] = '\0';
            fieldCount++;
            fieldPos = 0;
        } else {
            // 添加字符到当前字段
            if (fieldPos < 31) { // 预留一个字符给 '\0'
                fields[fieldCount][fieldPos++] = sentence[sentencePos];
            }
        }
        sentencePos++;
    }
    
    // 处理最后一个字段
    if (fieldPos > 0 && fieldCount < maxFields) {
        fields[fieldCount][fieldPos] = '\0';
        fieldCount++;
    }
    
    return fieldCount;
}

/**
 * @brief 验证NMEA语句格式
 * @param sentence NMEA语句字符串
 * @return true=格式正确，false=格式错误
 */
bool mpNmeaValidateSentence(const char *sentence)
{
    if (!sentence || sentence[0] != '$') {
        return false;
    }
    
    int len = strlen(sentence);
    if (len < 8) { // 最小长度: $XXXXX*XX
        return false;
    }
    
    // 查找星号位置
    int asteriskPos = -1;
    for (int i = 1; i < len; i++) {
        if (sentence[i] == '*') {
            asteriskPos = i;
            break;
        }
    }
    
    if (asteriskPos == -1 || asteriskPos >= (len - 2)) {
        return false; // 没有找到星号或校验和不完整
    }
    
    // 验证校验和字符是否为十六进制
    if (mpAsciiToNibble(sentence[asteriskPos + 1]) < 0 || 
        mpAsciiToNibble(sentence[asteriskPos + 2]) < 0) {
        return false;
    }
    
    // 计算并验证校验和
    uint8_t calculatedChecksum = 0;
    for (int i = 1; i < asteriskPos; i++) {
        calculatedChecksum ^= sentence[i];
    }
    
    int receivedChecksum = (mpAsciiToNibble(sentence[asteriskPos + 1]) << 4) | 
                          mpAsciiToNibble(sentence[asteriskPos + 2]);
    
    return (calculatedChecksum == receivedChecksum);
}

/**
 * @brief 识别NMEA语句类型
 * @param sentenceName NMEA语句名称
 * @return 语句类型描述字符串
 */
const char* mpNmeaGetSentenceType(const char *sentenceName)
{
    if (!sentenceName) return "Unknown";
    
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