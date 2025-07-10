/**
 * @file Message_Parser.c
 * @brief 统一消息解析器框架 - 主体解析器实现
 * @details 管理多种协议解析器，提供统一的API接口和协议识别机制
 * @version 1.0
 * @date 2024-12
 */

#include "Message_Parser.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

//----------------------------------------
// 版本信息
//----------------------------------------

#define VERSION_STRING "1.0.0"

//----------------------------------------
// 内部常量
//----------------------------------------

// CRC32查找表
static const uint32_t mp_crc32Table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
    0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
    0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
    0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
    0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
    0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
    0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
    0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
    0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
    0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
    0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
    0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
    0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
    0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
    0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
    0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
    0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
    0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693,
    0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

//----------------------------------------
// 内部函数声明
//----------------------------------------

// 主状态机 - 寻找前导字节
static bool mpFindPreamble(MP_PARSE_STATE *parse, uint8_t data);

//----------------------------------------
// 工具函数实现
//----------------------------------------

const char* mpGetVersion(void)
{
    return VERSION_STRING;
}

int mpAsciiToNibble(int data)
{
    // 转换为小写
    data |= 0x20;
    if ((data >= 'a') && (data <= 'f'))
        return data - 'a' + 10;
    if ((data >= '0') && (data <= '9'))
        return data - '0';
    return -1;
}

void mpSafePrintf(MP_PRINTF_CALLBACK callback, const char *format, ...)
{
    if (callback) {
        va_list args;
        va_start(args, format);
        
        char buffer[512];
        vsnprintf(buffer, sizeof(buffer), format, args);
        callback("%s", buffer);
        
        va_end(args);
    }
}

int mpHexToString(const uint8_t *data, uint16_t length, char *output, uint16_t outputSize)
{
    if (!data || !output || outputSize < 3) return 0;
    
    int pos = 0;
    for (uint16_t i = 0; i < length && pos < (outputSize - 3); i++) {
        pos += snprintf(output + pos, outputSize - pos, "%02X ", data[i]);
    }
    
    if (pos > 0 && output[pos - 1] == ' ') {
        output[pos - 1] = '\0'; // 移除最后一个空格
        pos--;
    }
    
    return pos;
}

uint8_t mpCalculateChecksum(const uint8_t *data, uint16_t length)
{
    uint8_t checksum = 0;
    for (uint16_t i = 0; i < length; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

//----------------------------------------
// 协议信息函数
//----------------------------------------

const char* mpGetProtocolName(const MP_PARSE_STATE *parse, MP_PROTOCOL_TYPE protocolType)
{
    if (!parse || protocolType >= parse->parserCount) {
        return "Unknown";
    }
    return parse->parsers[protocolType].name;
}

const char* mpGetProtocolDescription(MP_PROTOCOL_TYPE protocolType)
{
    // This function is no longer supported as the description has been removed.
    // The information should be handled by the user application.
    if (protocolType >= MP_PROTOCOL_COUNT) return "未知协议";
    return "描述已移除";
}

//----------------------------------------
// 统计函数
//----------------------------------------

/**
 * @brief 获取协议统计信息
 * @param parse 解析器状态
 * @param stats 统计信息数组
 * @param maxCount 数组最大长度
 * @return 实际返回的统计信息数量
 */
uint16_t mpGetProtocolStats(const MP_PARSE_STATE *parse, 
                           MP_PROTOCOL_STATS *stats, 
                           uint16_t maxCount)
{
    if (!parse || !stats || maxCount == 0) return 0;
    
    uint16_t count = (parse->parserCount < maxCount) ? parse->parserCount : maxCount;
    
    for (uint16_t i = 0; i < count; i++) {
        stats[i].protocolName = parse->parsers[i].name;
        // Description is removed from the core library
        stats[i].description = "-"; 
        stats[i].messagesProcessed = parse->messagesProcessed[i];
        stats[i].crcErrors = parse->crcErrors[i];
        
        uint32_t total = stats[i].messagesProcessed + stats[i].crcErrors;
        stats[i].successRate = (total > 0) ? ((float)stats[i].messagesProcessed / total) * 100.0f : 100.0f;
        stats[i].isActive = (parse->type == (MP_PROTOCOL_TYPE)i);
    }
    
    return count;
}

void mpResetStats(MP_PARSE_STATE *parse)
{
    if (parse) {
        memset(parse->messagesProcessed, 0, sizeof(parse->messagesProcessed));
        memset(parse->crcErrors, 0, sizeof(parse->crcErrors));
        parse->totalBytes = 0;
        parse->protocolSwitches = 0;
    }
}

void mpPrintStats(const MP_PARSE_STATE *parse)
{
    if (!parse || !parse->printDebug) return;

    mpSafePrintf(parse->printDebug, "=== 消息解析器统计报告 ===");
    mpSafePrintf(parse->printDebug, "解析器版本: %s", mpGetVersion());
    mpSafePrintf(parse->printDebug, "总处理字节数: %u", parse->totalBytes);
    mpSafePrintf(parse->printDebug, "协议切换次数: %u", parse->protocolSwitches);
    mpSafePrintf(parse->printDebug, "当前活动协议: %s", 
                 parse->type < MP_PROTOCOL_COUNT ? 
                 parse->parsers[parse->type].name : "无");
    
    bool hasData = false;
    for (int i = 0; i < MP_PROTOCOL_COUNT; i++) {
        if (parse->messagesProcessed[i] > 0 || parse->crcErrors[i] > 0) {
            uint32_t total = parse->messagesProcessed[i] + parse->crcErrors[i];
            float successRate = total > 0 ? 
                (float)parse->messagesProcessed[i] / total * 100.0f : 0.0f;
            
            const char *activeMarker = (parse->type == (MP_PROTOCOL_TYPE)i) ? " [活动]" : "";
            mpSafePrintf(parse->printDebug, "%s%s: 成功=%u, 错误=%u, 成功率=%.1f%%",
                parse->parsers[i].name, activeMarker,
                parse->messagesProcessed[i], parse->crcErrors[i], successRate);
            hasData = true;
        }
    }
    
    if (!hasData) {
        mpSafePrintf(parse->printDebug, "暂无协议数据处理记录");
    }
    mpSafePrintf(parse->printDebug, "========================");
}

void mpListSupportedProtocols(const MP_PARSE_STATE *parse)
{
    if (!parse) return;

    mpSafePrintf(parse->printDebug, "\n--- %s 支持的协议列表 ---", parse->parserName);
    for (int i = 0; i < parse->parserCount; i++) {
        mpSafePrintf(parse->printDebug, "  - %s", parse->parsers[i].name);
    }
    mpSafePrintf(parse->printDebug, "-----------------------------------\n");
}

//----------------------------------------
// 主状态机
//----------------------------------------

// 主状态机 - 寻找前导字节
static bool mpFindPreamble(MP_PARSE_STATE *parse, uint8_t data)
{
    if (!parse) return false;

    // 重置状态
    parse->crc = 0;
    parse->computeCrc = NULL;
    parse->length = 0;
    MP_PROTOCOL_TYPE oldType = parse->type;
    parse->type = MP_PROTOCOL_COUNT; // 表示未识别
    parse->buffer[parse->length++] = data;
    parse->totalBytes++;

    // 遍历所有注册的解析器
    for (int index = 0; index < parse->parserCount; index++) {
        if (parse->parsers[index].preambleFunction(parse, data)) {
            if (oldType != (MP_PROTOCOL_TYPE)index && oldType != MP_PROTOCOL_COUNT) {
                parse->protocolSwitches++;
            }
            parse->type = (MP_PROTOCOL_TYPE)index;
            return true;
        }
    }

    // 未找到匹配的协议前导
    parse->state = mpFindPreamble;
    return false;
}

//----------------------------------------
// 公共API实现
//----------------------------------------

bool mpInitParser(MP_PARSE_STATE *parse,
                  uint8_t *buffer,
                  uint16_t bufferLength,
                  const MP_PARSER_INFO *userParsers,
                  uint16_t userParserCount,
                  MP_EOM_CALLBACK eomCallback,
                  MP_BAD_CRC_CALLBACK badCrcCallback,
                  const char *parserName,
                  MP_PRINTF_CALLBACK printError,
                  MP_PRINTF_CALLBACK printDebug)
{
    // 1. 参数校验
    if (!parse || !buffer || bufferLength < MP_MINIMUM_BUFFER_LENGTH || !eomCallback || !userParsers || userParserCount == 0) {
        if (printError) {
            printError("[MP] 初始化失败: 无效的参数\n");
        }
        return false;
    }

    // 2. 清理状态结构体
    memset(parse, 0, sizeof(MP_PARSE_STATE));

    // 3. 设置用户提供的配置
    parse->buffer = buffer;
    parse->bufferLength = bufferLength;
    parse->eomCallback = eomCallback;
    parse->badCrc = badCrcCallback;
    parse->parserName = parserName ? parserName : "DefaultParser";
    parse->printError = printError;
    parse->printDebug = printDebug;
    parse->parsers = userParsers;
    parse->parserCount = userParserCount;

    // 4. 设置初始状态
    parse->state = mpFindPreamble;
    parse->type = MP_PROTOCOL_COUNT; // 初始状态为未知

    mpSafePrintf(parse->printDebug, "[MP] 解析器 '%s' 初始化成功，包含 %d 个协议\n",
                 parse->parserName, parse->parserCount);

    return true;
}

bool mpProcessByte(MP_PARSE_STATE *parse, uint8_t data)
{
    if (!parse) return false;

    // 检查缓冲区空间
    if (parse->length >= parse->bufferLength) {
        mpSafePrintf(parse->printError,
            "MP %s: 消息过长, 增加缓冲区大小 > %d",
            parse->parserName ? parse->parserName : "Unknown", parse->bufferLength);
        return mpFindPreamble(parse, data);
    }

    // 保存数据字节
    parse->buffer[parse->length++] = data;
    parse->totalBytes++;

    // 计算CRC（如果需要）
    if (parse->computeCrc) {
        parse->crc = parse->computeCrc(parse, data);
    }

    // 调用状态处理函数
    bool result = parse->state(parse, data);

    // 如果状态函数返回false，表示当前消息处理结束（无论成功或失败）
    // 或者状态被显式设为NULL，表示需要立即重置
    if (!result || parse->state == NULL) {
        // 重置状态机以寻找下一个消息的前导字节
        return mpFindPreamble(parse, data);
    }

    return result;
}

MP_PROTOCOL_TYPE mpGetActiveProtocol(const MP_PARSE_STATE *parse)
{
    if (!parse || parse->type >= MP_PROTOCOL_COUNT) {
        return MP_PROTOCOL_COUNT; // 未知协议
    }
    return parse->type;
}

//----------------------------------------
// CRC32计算函数（供各协议使用）
//----------------------------------------

uint32_t mpComputeCrc32(MP_PARSE_STATE *parse, uint8_t data)
{
    (void)parse; // 避免未使用参数警告
    uint32_t crc = parse->crc;
    crc = mp_crc32Table[(crc ^ data) & 0xff] ^ (crc >> 8);
    return crc;
} 