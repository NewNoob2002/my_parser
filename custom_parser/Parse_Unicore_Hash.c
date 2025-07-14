/**
 * @file Parse_Unicore_Hash.c
 * @brief 中海达Hash协议解析器
 * @details 解析中海达GNSS模块的ASCII Hash协议，支持校验和验证
 * @version 1.0
 * @date 2024-12
 */

#include "Message_Parser.h"
#include <string.h>
#include <stdio.h>

//----------------------------------------
// 协议格式说明
//----------------------------------------
//
//  Unicore Hash Protocol Format:
//  +----------+----------+----------+----------+----------+----------+----------+
//  | Preamble | Command  | Comma    | Port     | Comma    | Data     | Asterisk |
//  | 8 bits   | n bytes  | 8 bits   | n bytes  | 8 bits   | n bytes  | 8 bits   |
//  |    #     |          |    ,     |          |    ,     |          |    *     |
//  +----------+----------+----------+----------+----------+----------+----------+
//  | Checksum | CRLF     |
//  | 2 bytes  | 2 bytes  |
//  |    XX    |   \r\n   |
//  +----------+----------+
//             |                                             |
//             |<-------- Checksum Calculation ------------>|
//
//  Examples:
//  #BESTPOSA,COM1,0,55.0,FINESTEERING,1419,340033.000,02000040,d821,15564;...
//  #LOGLISTA,COM1,0,55.0,FINESTEERING,1419,340033.000,02000040,d821,15564;...

//----------------------------------------
// 常量定义
//----------------------------------------

// 为命令末尾的字符预留空间: *, XX, \r, \n, \0
#define UNICORE_HASH_BUFFER_OVERHEAD    (1 + 2 + 2 + 1)

//----------------------------------------
// 内部状态函数前向声明
//----------------------------------------

static bool mpUnicoreHashFindFirstComma(MP_PARSE_STATE *parse, uint8_t data);
static bool mpUnicoreHashFindSemicolon(MP_PARSE_STATE *parse, uint8_t data);
static bool mpUnicoreHashChecksumByte1(MP_PARSE_STATE *parse, uint8_t data);
static bool mpUnicoreHashChecksumByte2(MP_PARSE_STATE *parse, uint8_t data);
static bool mpUnicoreHashLineTermination(MP_PARSE_STATE *parse, uint8_t data);

//----------------------------------------
// Unicore Hash协议解析状态机
//----------------------------------------

/**
 * @brief 验证校验和并完成消息处理
 * @param parse 解析器状态指针
 */
static void mpUnicoreHashValidateChecksum(MP_PARSE_STATE *parse)
{
    int checksum;
    
    // 转换校验和字符为二进制值
    checksum = msgp_util_asciiToNibble(parse->buffer[parse->length - 2]) << 4;
    checksum |= msgp_util_asciiToNibble(parse->buffer[parse->length - 1]);

    // 验证校验和
    bool checksum_ok = (checksum == (int)(parse->crc & 0xFF));
    if (checksum_ok) {
        if (parse->eomCallback) {
            parse->eomCallback(parse, parse->protocolIndex);
        }
    } else {
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
static bool mpUnicoreHashLineTermination(MP_PARSE_STATE *parse, uint8_t data)
{
    // 不添加当前字符到长度计数中
    parse->length -= 1;

    // 处理行终止符
    if (data == '\r' || data == '\n') {
        mpUnicoreHashValidateChecksum(parse);
        return false; // 消息处理完成
    }

    // 无效的行终止符，仍然验证并处理消息
    mpUnicoreHashValidateChecksum(parse);
    return false;
}

/**
 * @brief 读取第二个校验和字节
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return true=继续解析，false=状态错误
 */
static bool mpUnicoreHashChecksumByte2(MP_PARSE_STATE *parse, uint8_t data)
{
    // 验证第二个校验和字符是否为有效的十六进制字符
    if (msgp_util_asciiToNibble(parse->buffer[parse->length - 1]) >= 0) {
        parse->state = mpUnicoreHashLineTermination;
        return true;
    }

    // 无效的校验和字符
    msgp_util_safePrintf(parse->printDebug,
        "MP: %s Unicore Hash无效的第二个校验和字符: 0x%02X",
        parse->parserName ? parse->parserName : "Unknown", data);
    return false;
}

/**
 * @brief 读取第一个校验和字节
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return true=继续解析，false=状态错误
 */
static bool mpUnicoreHashChecksumByte1(MP_PARSE_STATE *parse, uint8_t data)
{
    // 验证第一个校验和字符是否为有效的十六进制字符
    if (msgp_util_asciiToNibble(parse->buffer[parse->length - 1]) >= 0) {
        parse->state = mpUnicoreHashChecksumByte2;
        return true;
    }

    // 无效的校验和字符
    msgp_util_safePrintf(parse->printDebug,
        "MP: %s Unicore Hash无效的第一个校验和字符: 0x%02X",
        parse->parserName ? parse->parserName : "Unknown", data);
    return false;
}

/**
 * @brief 查找分号（;）分隔符
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return true=继续解析，false=缓冲区溢出
 */
static bool mpUnicoreHashFindSemicolon(MP_PARSE_STATE *parse, uint8_t data)
{
    if (data == ';') {
        // 找到分号，开始读取校验和
        parse->state = mpUnicoreHashChecksumByte1;
    } else if (data == '*') {
        // 找到星号，也开始读取校验和（某些变体使用*而不是;）
        parse->state = mpUnicoreHashChecksumByte1;
    } else {
        // 包含在校验和计算中的数据字节
        parse->crc ^= data;

        // 验证缓冲区空间是否足够
        if ((uint32_t)(parse->length + UNICORE_HASH_BUFFER_OVERHEAD) > parse->bufferLength) {
            // 命令过长
            msgp_util_safePrintf(parse->printDebug,
                "MP: %s Unicore Hash命令过长, 增加缓冲区大小 > %d",
                parse->parserName ? parse->parserName : "Unknown",
                parse->bufferLength);
            return false;
        }
    }
    return true;
}

/**
 * @brief 查找第一个逗号并读取命令名称
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return true=继续解析，false=格式错误
 */
static bool mpUnicoreHashFindFirstComma(MP_PARSE_STATE *parse, uint8_t data)
{
    parse->crc ^= data;
    
    if (data != ',') {
        // 验证命令名称字符的有效性
        uint8_t upper = data & ~0x20; // 转换为大写
        if (((upper < 'A') || (upper > 'Z')) && ((data < '0') || (data > '9'))) {
            msgp_util_safePrintf(parse->printDebug,
                "MP: %s Unicore Hash无效命令名字符: 0x%02X ('%c')",
                parse->parserName ? parse->parserName : "Unknown", data, 
                (data >= 32 && data <= 126) ? data : '?');
            return false;
        }
    } else {
        // 找到第一个逗号，命令名称读取完成
        parse->state = mpUnicoreHashFindSemicolon;
        
        // 提取命令名称用于调试
        char commandName[32] = {0};
        int nameLen = parse->length - 2; // 不包括#和当前逗号
        if (nameLen > 0 && nameLen < 31) {
            memcpy(commandName, parse->buffer + 1, nameLen);
            commandName[nameLen] = '\0';
            msgp_util_safePrintf(parse->printDebug,
                "MP: Unicore Hash命令名: %s", commandName);
        }
    }
    return true;
}

//----------------------------------------
// 公共接口函数
//----------------------------------------

/**
 * @brief Unicore Hash协议前导字节检测
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return true=检测到Unicore Hash协议前导，false=不是Unicore Hash协议
 */
bool msgp_unicore_hash_preamble(MP_PARSE_STATE *parse, uint8_t data)
{
    if (data != '#') return false;

    parse->length = 0;
    parse->buffer[parse->length++] = data;
    parse->crc = 0;
    parse->computeCrc = NULL;
    parse->state = mpUnicoreHashFindFirstComma;
    
    msgp_util_safePrintf(parse->printDebug, 
        "MP: 检测到Unicore Hash协议前导字符 '#'");
    
    return true;
}

//----------------------------------------
// Unicore Hash协议辅助函数
//----------------------------------------
const char* msgp_unicore_hash_getSentenceName(const MP_PARSE_STATE *parse)
{
    static char commandName[32];
    if (!parse || parse->length < 2) return "";

    int nameLen = 0;
    for (int i = 1; i < parse->length && i < 32; i++) {
        if (parse->buffer[i] == ',') {
            nameLen = i - 1;
            break;
        }
    }
    if(nameLen > 0) {
        memcpy(commandName, parse->buffer + 1, nameLen);
        commandName[nameLen] = '\0';
        return commandName;
    }
    return "";
}

int msgp_unicore_hash_parseFields(const MP_PARSE_STATE *parse, char fields[][64], int maxFields)
{
    if (!parse || !fields || maxFields <= 0) return 0;

    char sentence[MP_MINIMUM_BUFFER_LENGTH];
    uint16_t len = (parse->length < sizeof(sentence) -1) ? parse->length : sizeof(sentence) - 1;
    memcpy(sentence, parse->buffer, len);
    sentence[len] = '\0';

    return msgp_util_parse_delimited_fields(sentence, fields, maxFields, 64, ',', '*');
}

const char* msgp_unicore_hash_getCommandType(const char *commandName)
{
    if (!commandName) return "Unknown";
    
    // 常见的Unicore Hash命令类型
    if (strncmp(commandName, "BESTPOSA", 8) == 0) {
        return "Best Position in ASCII";
    } else if (strncmp(commandName, "BESTPOSB", 8) == 0) {
        return "Best Position in Binary";
    } else if (strncmp(commandName, "BESTVELA", 8) == 0) {
        return "Best Velocity in ASCII";
    } else if (strncmp(commandName, "BESTVELB", 8) == 0) {
        return "Best Velocity in Binary";
    } else if (strncmp(commandName, "RANGEA", 6) == 0) {
        return "Range Measurements in ASCII";
    } else if (strncmp(commandName, "RANGEB", 6) == 0) {
        return "Range Measurements in Binary";
    } else if (strncmp(commandName, "VERSIONA", 8) == 0) {
        return "Receiver Version in ASCII";
    } else if (strncmp(commandName, "VERSIONB", 8) == 0) {
        return "Receiver Version in Binary";
    } else if (strncmp(commandName, "LOGLISTA", 8) == 0) {
        return "Log List in ASCII";
    } else if (strncmp(commandName, "LOGLISTB", 8) == 0) {
        return "Log List in Binary";
    } else if (strncmp(commandName, "TRACKSTATA", 10) == 0) {
        return "Tracking Status in ASCII";
    } else if (strncmp(commandName, "TRACKSTATB", 10) == 0) {
        return "Tracking Status in Binary";
    } else if (strncmp(commandName, "RXSTATUSA", 9) == 0) {
        return "Receiver Status in ASCII";
    } else if (strncmp(commandName, "RXSTATUSB", 9) == 0) {
        return "Receiver Status in Binary";
    }
    
    return "Unknown Unicore Command";
}

/**
 * @brief 构建Unicore Hash命令
 * @param commandName 命令名称
 * @param fields 字段数组
 * @param fieldCount 字段数量
 * @param output 输出缓冲区
 * @param outputSize 输出缓冲区大小
 * @return 实际输出的字符数，-1表示失败
 */
int msgp_unicore_hash_buildCommand(const char *commandName, const char *fields[], 
                             int fieldCount, char *output, int outputSize)
{
    if (!commandName || !output || outputSize < 16) {
        return -1;
    }
    
    int pos = 0;
    uint8_t checksum = 0;
    
    // 添加前导字符和命令名称
    pos += snprintf(output + pos, outputSize - pos, "#%s", commandName);
    if (pos >= outputSize - 8) return -1; // 预留校验和和结束符空间
    
    // 计算命令名称的校验和
    for (int i = 0; commandName[i]; i++) {
        checksum ^= commandName[i];
    }
    
    // 添加字段
    for (int i = 0; i < fieldCount && pos < outputSize - 8; i++) {
        pos += snprintf(output + pos, outputSize - pos, ",%s", 
                       fields[i] ? fields[i] : "");
        
        // 计算逗号和字段的校验和
        checksum ^= ',';
        if (fields[i]) {
            for (int j = 0; fields[i][j]; j++) {
                checksum ^= fields[i][j];
            }
        }
    }
    
    // 添加分隔符和校验和
    pos += snprintf(output + pos, outputSize - pos, "*%02X\r\n", checksum);
    
    return pos;
} 