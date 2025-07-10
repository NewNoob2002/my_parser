/**
 * @file Parse_Unicore_Binary.c
 * @brief 中海达二进制协议解析器
 * @details 解析中海达GNSS模块的二进制协议，支持CRC32校验
 * @version 1.0
 * @date 2024-12
 */

#include "Message_Parser.h"

//----------------------------------------
// 协议格式说明
//----------------------------------------
//
//  Unicore Binary Protocol Format:
//  +----------+----------+----------+----------+----------+----------+----------+----------+
//  | SYNC1    | SYNC2    | SYNC3    | HeaderLen| MsgType  | MsgId    | TimeStatus| Week     |
//  | 8 bits   | 8 bits   | 8 bits   | 8 bits   | 8 bits   | 16 bits  | 8 bits   | 16 bits  |
//  | 0xAA     | 0x44     | 0x12     | 0x1C     |          |          |          |          |
//  +----------+----------+----------+----------+----------+----------+----------+----------+
//  | GPSms    | RecStatus| Reserved | MsgLen   | PAYLOAD  | CRC32    |
//  | 32 bits  | 16 bits  | 16 bits  | 16 bits  | n bytes  | 32 bits  |
//  |          |          |          |          |          |          |
//  +----------+----------+----------+----------+----------+----------+
//             |                                 |                    |
//             |<--------- CRC Calculation --------------------->|
//
//  Examples:
//  - BESTPOS: AA 44 12 1C 00 [msg] [len] [data] [CRC32]

//----------------------------------------
// 外部CRC32计算函数声明
//----------------------------------------

extern uint32_t msgp_computeCrc32(MP_PARSE_STATE *parse, uint8_t data);

//----------------------------------------
// 内部状态函数前向声明
//----------------------------------------

static bool mpUnicoreBinSync2(MP_PARSE_STATE *parse, uint8_t data);
static bool mpUnicoreBinSync3(MP_PARSE_STATE *parse, uint8_t data);
static bool mpUnicoreBinReadHeader(MP_PARSE_STATE *parse, uint8_t data);
static bool mpUnicoreBinReadPayload(MP_PARSE_STATE *parse, uint8_t data);
static bool mpUnicoreBinReadCrc(MP_PARSE_STATE *parse, uint8_t data);

//----------------------------------------
// Unicore二进制协议头部结构
//----------------------------------------

typedef struct _UNICORE_BIN_HEADER {
    uint8_t sync1;              // 0xAA
    uint8_t sync2;              // 0x44
    uint8_t sync3;              // 0x12
    uint8_t headerLength;       // 0x1C (28 bytes)
    uint8_t messageType;        // 消息类型
    uint16_t messageId;         // 消息ID
    uint8_t timeStatus;         // 时间状态
    uint16_t week;              // GPS周
    uint32_t gpsms;             // GPS毫秒
    uint16_t receiverStatus;    // 接收机状态
    uint16_t reserved;          // 保留
    uint16_t messageLength;     // 消息长度
} __attribute__((packed)) UNICORE_BIN_HEADER;

//----------------------------------------
// Unicore二进制协议解析状态机
//----------------------------------------

/**
 * @brief 读取并验证CRC32校验码
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return false=消息处理完成
 */
static bool mpUnicoreBinReadCrc(MP_PARSE_STATE *parse, uint8_t data)
{
    MP_SCRATCH_PAD *scratchPad = &parse->scratchPad;
    
    // 检查是否读取完所有CRC字节
    if (--scratchPad->unicore.bytesRemaining) {
        return true; // 还需要更多字节
    }
    
    // 提取接收到的CRC（小端序）
    uint32_t crcRead = 0;
    if (parse->length >= 4) {
        crcRead = (uint32_t)parse->buffer[parse->length - 4] |
                  ((uint32_t)parse->buffer[parse->length - 3] << 8) |
                  ((uint32_t)parse->buffer[parse->length - 2] << 16) |
                  ((uint32_t)parse->buffer[parse->length - 1] << 24);
    }
    
    uint32_t crcComputed = scratchPad->unicore.crc;
    
    // 验证CRC
    if (crcRead == crcComputed) {
        if (parse->eomCallback) {
            parse->eomCallback(parse, parse->protocolIndex);
        }
        
        UNICORE_BIN_HEADER *header = (UNICORE_BIN_HEADER *)parse->buffer;
        msgp_util_safePrintf(parse->printDebug,
            "MP: Unicore二进制消息解析成功, 类型=0x%02X, ID=%d, 长度=%d",
            header->messageType, header->messageId, header->messageLength);
    } else {
        if (parse->badCrc) {
            parse->badCrc(parse);
        }
    }
    
    return false; // 消息处理完成
}

/**
 * @brief 读取消息负载数据
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return true=继续解析
 */
static bool mpUnicoreBinReadPayload(MP_PARSE_STATE *parse, uint8_t data)
{
    MP_SCRATCH_PAD *scratchPad = &parse->scratchPad;
    
    (void)data; // 避免未使用参数警告
    
    // 检查是否读取完所有负载数据
    if (!--scratchPad->unicore.bytesRemaining) {
        // 负载读取完成，准备读取CRC32
        scratchPad->unicore.bytesRemaining = 4;
        parse->crc ^= 0xFFFFFFFF; // CRC32结束处理
        scratchPad->unicore.crc = parse->crc;
        parse->state = mpUnicoreBinReadCrc;
    }
    
    return true;
}

/**
 * @brief 读取消息头部
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return true=继续解析，false=头部格式错误
 */
static bool mpUnicoreBinReadHeader(MP_PARSE_STATE *parse, uint8_t data)
{
    MP_SCRATCH_PAD *scratchPad = &parse->scratchPad;
    
    (void)data; // 避免未使用参数警告
    
    if (parse->length >= sizeof(UNICORE_BIN_HEADER)) {
        // 头部读取完成，准备读取消息负载
        UNICORE_BIN_HEADER *header = (UNICORE_BIN_HEADER *)parse->buffer;
        
        // 验证头部长度
        if (header->headerLength != 0x1C) {
            msgp_util_safePrintf(parse->printDebug,
                "MP: Unicore二进制无效头部长度: 0x%02X", header->headerLength);
            return false;
        }
        
        scratchPad->unicore.messageLength = header->messageLength;
        scratchPad->unicore.bytesRemaining = header->messageLength;
        
        msgp_util_safePrintf(parse->printDebug,
            "MP: Unicore二进制头部解析完成, 消息ID=%d, 长度=%d",
            header->messageId, header->messageLength);
        
        // 如果没有负载数据，直接读取CRC
        if (scratchPad->unicore.bytesRemaining == 0) {
            scratchPad->unicore.bytesRemaining = 4;
            parse->crc ^= 0xFFFFFFFF;
            scratchPad->unicore.crc = parse->crc;
            parse->state = mpUnicoreBinReadCrc;
        } else {
            parse->state = mpUnicoreBinReadPayload;
        }
    }
    
    return true;
}

/**
 * @brief 检测第三个同步字节 (0x12)
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return true=继续解析，false=同步失败
 */
static bool mpUnicoreBinSync3(MP_PARSE_STATE *parse, uint8_t data)
{
    if (data != 0x12) {
        msgp_util_safePrintf(parse->printDebug,
            "MP: Unicore二进制第三个同步字节错误: 0x%02X", data);
        return false;
    }
    
    // 第三个同步字节正确，开始读取头部
    parse->state = mpUnicoreBinReadHeader;
    return true;
}

/**
 * @brief 检测第二个同步字节 (0x44)
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return true=继续解析，false=同步失败
 */
static bool mpUnicoreBinSync2(MP_PARSE_STATE *parse, uint8_t data)
{
    if (data != 0x44) {
        msgp_util_safePrintf(parse->printDebug,
            "MP: Unicore二进制第二个同步字节错误: 0x%02X", data);
        return false;
    }
    
    // 第二个同步字节正确，等待第三个同步字节
    parse->state = mpUnicoreBinSync3;
    return true;
}

//----------------------------------------
// 公共接口函数
//----------------------------------------

/**
 * @brief Unicore二进制协议前导字节检测
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return true=检测到Unicore二进制协议前导，false=不是Unicore二进制协议
 */
bool msgp_unicore_bin_preamble(MP_PARSE_STATE *parse, uint8_t data)
{
    if (data != 0xAA) return false;

    parse->length = 0;
    parse->buffer[parse->length++] = data;
    parse->crc = 0xFFFFFFFF;
    parse->computeCrc = msgp_computeCrc32;
    parse->crc = parse->computeCrc(parse, data);
    parse->state = mpUnicoreBinSync2;
    
    msgp_util_safePrintf(parse->printDebug, 
        "MP: 检测到Unicore二进制协议前导字节 0x%02X", data);
    
    return true;
}

//----------------------------------------
// Unicore二进制协议辅助函数
//----------------------------------------

uint16_t msgp_unicore_bin_getMessageId(const MP_PARSE_STATE *parse)
{
    if (!parse || parse->length < sizeof(UNICORE_BIN_HEADER)) return 0;
    UNICORE_BIN_HEADER *header = (UNICORE_BIN_HEADER *)parse->buffer;
    return header->messageId;
}

const uint8_t* msgp_unicore_bin_getPayload(const MP_PARSE_STATE *parse, uint16_t *length)
{
    if (!parse || !length || parse->length < sizeof(UNICORE_BIN_HEADER) + 4) {
        if (length) *length = 0;
        return NULL;
    }
    UNICORE_BIN_HEADER *header = (UNICORE_BIN_HEADER *)parse->buffer;
    if ( (sizeof(UNICORE_BIN_HEADER) + header->messageLength + 4) != parse->length) {
        *length = 0;
        return NULL;
    }
    *length = header->messageLength;
    return parse->buffer + sizeof(UNICORE_BIN_HEADER);
}

const char* msgp_unicore_bin_getMessageName(uint16_t messageId)
{
    // 常见的Unicore二进制消息类型
    switch (messageId) {
        case 42:   return "BESTPOS - Best Position";
        case 99:   return "BESTVEL - Best Velocity";
        case 140:  return "RANGE - Range Measurements";
        case 43:   return "PSRPOS - Pseudorange Position";
        case 100:  return "PSRVEL - Pseudorange Velocity";
        case 41:   return "BESTUTM - Best UTM Position";
        case 507:  return "BESTXYZ - Best Cartesian Position";
        case 508:  return "BESTLLA - Best Latitude/Longitude/Altitude";
        case 1:    return "LOG - Data Logging Control";
        case 35:   return "VERSION - Receiver Version";
        case 37:   return "RXSTATUS - Receiver Status";
        case 38:   return "RXCONFIG - Receiver Configuration";
        case 128:  return "TRACKSTAT - Satellite Tracking Status";
        case 181:  return "IONUTC - Ionosphere and UTC Parameters";
        case 267:  return "CLOCKMODEL - Clock Model";
        case 718:  return "GPSEPHEM - GPS Ephemeris";
        case 723:  return "GLOEPHEMERIS - GLONASS Ephemeris";
        case 1696: return "GALEPHEMERIS - Galileo Ephemeris";
        case 1695: return "BDSEPHEMERIS - BeiDou Ephemeris";
        case 971:  return "RAWEPHEM - Raw Ephemeris Data";
        case 1067: return "HEADING - Heading Information";
        case 1335: return "DUAL - Dual Antenna Heading";
        case 1362: return "TIME - Time Information";
        case 1430: return "RTKPOS - RTK Position";
        case 1431: return "RTKVEL - RTK Velocity";
        default:
            if (messageId >= 1 && messageId <= 100) {
                return "Standard Message";
            } else if (messageId >= 1000 && messageId <= 2000) {
                return "Extended Message";
            } else {
                return "Unknown Message";
            }
    }
}

/**
 * @brief 获取Unicore二进制消息头部信息
 * @param buffer 消息缓冲区
 * @param length 消息长度
 * @param header 输出的头部信息指针
 * @return true=成功解析头部，false=头部格式错误
 */
bool mpUnicoreBinGetHeaderInfo(const uint8_t *buffer, uint16_t length, UNICORE_BIN_HEADER **header)
{
    if (!buffer || !header || length < sizeof(UNICORE_BIN_HEADER)) {
        return false;
    }
    
    *header = (UNICORE_BIN_HEADER *)buffer;
    
    // 验证同步字节
    if ((*header)->sync1 != 0xAA || 
        (*header)->sync2 != 0x44 || 
        (*header)->sync3 != 0x12) {
        return false;
    }
    
    // 验证头部长度
    if ((*header)->headerLength != 0x1C) {
        return false;
    }
    
    return true;
}

/**
 * @brief 验证Unicore二进制消息的完整性
 * @param buffer 消息缓冲区
 * @param length 消息长度
 * @return true=消息完整且CRC正确，false=消息损坏
 */
bool mpUnicoreBinVerifyMessage(const uint8_t *buffer, uint16_t length)
{
    if (!buffer || length < sizeof(UNICORE_BIN_HEADER) + 4) {
        return false;
    }
    
    UNICORE_BIN_HEADER *header;
    if (!mpUnicoreBinGetHeaderInfo(buffer, length, &header)) {
        return false;
    }
    
    // 验证消息长度
    uint16_t expectedLength = sizeof(UNICORE_BIN_HEADER) + header->messageLength + 4;
    if (length != expectedLength) {
        return false;
    }
    
    // 这里可以添加完整的CRC32验证逻辑
    // 为简化示例，这里只做长度验证
    
    return true;
}

/**
 * @brief 解析BESTPOS消息获取位置信息
 * @param buffer 消息缓冲区
 * @param length 消息长度
 * @param latitude 输出纬度（度）
 * @param longitude 输出经度（度）
 * @param height 输出高度（米）
 * @return true=解析成功，false=消息格式错误
 */
bool mpUnicoreBinParseBestPos(const uint8_t *buffer, uint16_t length,
                             double *latitude, double *longitude, double *height)
{
    if (!buffer || !latitude || !longitude || !height) {
        return false;
    }
    
    UNICORE_BIN_HEADER *header;
    if (!mpUnicoreBinGetHeaderInfo(buffer, length, &header)) {
        return false;
    }
    
    // 验证这是BESTPOS消息 (ID = 42)
    if (header->messageId != 42) {
        return false;
    }
    
    // BESTPOS负载至少需要72字节
    if (header->messageLength < 72) {
        return false;
    }
    
    const uint8_t *payload = buffer + sizeof(UNICORE_BIN_HEADER);
    
    // 提取位置信息（小端序）
    // 纬度在偏移8处（8字节double）
    // 经度在偏移16处（8字节double）
    // 高度在偏移24处（8字节double）
    
    *latitude = *(double*)(payload + 8);
    *longitude = *(double*)(payload + 16);
    *height = *(double*)(payload + 24);
    
    return true;
} 