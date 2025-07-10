/**
 * @file Parse_UBLOX.c
 * @brief u-blox GPS二进制协议解析器
 * @details 解析u-blox GNSS模块的UBX二进制协议，支持Fletcher校验和
 * @version 1.0
 * @date 2024-12
 */

#include "Message_Parser.h"

//----------------------------------------
// 协议格式说明
//----------------------------------------
//
//  u-blox UBX Protocol Format:
//  +----------+----------+----------+----------+----------+----------+----------+
//  | SYNC1    | SYNC2    | CLASS    | ID       | LENGTH   | PAYLOAD  | CHECKSUM |
//  | 8 bits   | 8 bits   | 8 bits   | 8 bits   | 16 bits  | n bytes  | 16 bits  |
//  | 0xB5     | 0x62     |          |          | little   |          | FletcherA|
//  |          |          |          |          | endian   |          | FletcherB|
//  +----------+----------+----------+----------+----------+----------+----------+
//                         |                      |                    |
//                         |<------- Checksum Calculation ----------->|
//
//  Examples:
//  - NAV-PVT: B5 62 01 07 5C 00 [92 bytes data] [CK_A] [CK_B]
//  - NAV-SAT: B5 62 01 35 [length] [data] [CK_A] [CK_B]

//----------------------------------------
// 内部状态函数前向声明
//----------------------------------------

static bool mpUbloxSync2(MP_PARSE_STATE *parse, uint8_t data);
static bool mpUbloxReadClass(MP_PARSE_STATE *parse, uint8_t data);
static bool mpUbloxReadId(MP_PARSE_STATE *parse, uint8_t data);
static bool mpUbloxReadLengthLow(MP_PARSE_STATE *parse, uint8_t data);
static bool mpUbloxReadLengthHigh(MP_PARSE_STATE *parse, uint8_t data);
static bool mpUbloxReadPayload(MP_PARSE_STATE *parse, uint8_t data);
static bool mpUbloxReadChecksumA(MP_PARSE_STATE *parse, uint8_t data);
static bool mpUbloxReadChecksumB(MP_PARSE_STATE *parse, uint8_t data);

//----------------------------------------
// u-blox校验和计算
//----------------------------------------

/**
 * @brief 计算u-blox Fletcher校验和
 * @param parse 解析器状态指针
 * @param data 当前字节
 */
static void mpUbloxComputeChecksum(MP_PARSE_STATE *parse, uint8_t data)
{
    MP_SCRATCH_PAD *scratchPad = &parse->scratchPad;
    scratchPad->ublox.checksumA += data;
    scratchPad->ublox.checksumB += scratchPad->ublox.checksumA;
}

//----------------------------------------
// u-blox协议解析状态机
//----------------------------------------

/**
 * @brief 读取并验证第二个校验和字节 (CK_B)
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return false=消息处理完成
 */
static bool mpUbloxReadChecksumB(MP_PARSE_STATE *parse, uint8_t data)
{
    MP_SCRATCH_PAD *scratchPad = &parse->scratchPad;
    
    // 验证第二个校验和字节
    if (data == scratchPad->ublox.checksumB) {
        // 校验和正确，消息解析成功
        parse->messagesProcessed[MP_PROTOCOL_UBLOX]++;
        if (parse->eomCallback) {
            parse->eomCallback(parse, MP_PROTOCOL_UBLOX);
        }
        
        mpSafePrintf(parse->printDebug,
            "MP: u-blox消息解析成功, 类别=0x%02X, ID=0x%02X, 长度=%d",
            parse->buffer[2], parse->buffer[3], scratchPad->ublox.bytesRemaining);
    } else {
        // 校验和B错误
        parse->crcErrors[MP_PROTOCOL_UBLOX]++;
        mpSafePrintf(parse->printDebug,
            "MP: %s u-blox校验和B错误, 接收: 0x%02X, 计算: 0x%02X",
            parse->parserName ? parse->parserName : "Unknown", 
            data, scratchPad->ublox.checksumB);
    }
    
    return false; // 消息处理完成
}

/**
 * @brief 读取并验证第一个校验和字节 (CK_A)
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return true=继续解析，false=校验和错误
 */
static bool mpUbloxReadChecksumA(MP_PARSE_STATE *parse, uint8_t data)
{
    MP_SCRATCH_PAD *scratchPad = &parse->scratchPad;
    
    // 验证第一个校验和字节
    if (data == scratchPad->ublox.checksumA) {
        parse->state = mpUbloxReadChecksumB;
        return true;
    } else {
        // 校验和A错误
        parse->crcErrors[MP_PROTOCOL_UBLOX]++;
        mpSafePrintf(parse->printDebug,
            "MP: %s u-blox校验和A错误, 接收: 0x%02X, 计算: 0x%02X",
            parse->parserName ? parse->parserName : "Unknown", 
            data, scratchPad->ublox.checksumA);
        return false;
    }
}

/**
 * @brief 读取消息负载数据
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return true=继续解析，false=状态错误
 */
static bool mpUbloxReadPayload(MP_PARSE_STATE *parse, uint8_t data)
{
    MP_SCRATCH_PAD *scratchPad = &parse->scratchPad;
    
    // 计算校验和
    mpUbloxComputeChecksum(parse, data);
    
    // 检查是否读取完所有负载数据
    if (!--scratchPad->ublox.bytesRemaining) {
        // 负载读取完成，开始读取校验和
        parse->state = mpUbloxReadChecksumA;
    }
    
    return true;
}

/**
 * @brief 读取消息长度高字节
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return true=继续解析，false=状态错误
 */
static bool mpUbloxReadLengthHigh(MP_PARSE_STATE *parse, uint8_t data)
{
    MP_SCRATCH_PAD *scratchPad = &parse->scratchPad;
    
    // 计算校验和
    mpUbloxComputeChecksum(parse, data);
    
    // 组合完整的长度值（小端序）
    scratchPad->ublox.bytesRemaining |= (data << 8);
    
    mpSafePrintf(parse->printDebug,
        "MP: u-blox消息长度: %d字节", scratchPad->ublox.bytesRemaining);
    
    // 检查消息长度是否合理
    if (scratchPad->ublox.bytesRemaining > (parse->bufferLength - parse->length - 2)) {
        mpSafePrintf(parse->printDebug,
            "MP: u-blox消息长度过大: %d字节, 缓冲区剩余: %d字节",
            scratchPad->ublox.bytesRemaining, 
            parse->bufferLength - parse->length - 2);
        return false;
    }
    
    // 如果负载长度为0，直接读取校验和
    if (scratchPad->ublox.bytesRemaining == 0) {
        parse->state = mpUbloxReadChecksumA;
    } else {
        parse->state = mpUbloxReadPayload;
    }
    
    return true;
}

/**
 * @brief 读取消息长度低字节
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return true=继续解析
 */
static bool mpUbloxReadLengthLow(MP_PARSE_STATE *parse, uint8_t data)
{
    MP_SCRATCH_PAD *scratchPad = &parse->scratchPad;
    
    // 计算校验和
    mpUbloxComputeChecksum(parse, data);
    
    // 保存长度低字节
    scratchPad->ublox.bytesRemaining = data;
    parse->state = mpUbloxReadLengthHigh;
    
    return true;
}

/**
 * @brief 读取消息ID
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return true=继续解析
 */
static bool mpUbloxReadId(MP_PARSE_STATE *parse, uint8_t data)
{
    MP_SCRATCH_PAD *scratchPad = &parse->scratchPad;
    
    // 重置并开始计算校验和（从CLASS开始）
    scratchPad->ublox.checksumA = 0;
    scratchPad->ublox.checksumB = 0;
    
    // 计算CLASS和ID的校验和
    mpUbloxComputeChecksum(parse, parse->buffer[parse->length - 2]); // CLASS
    mpUbloxComputeChecksum(parse, data); // ID
    
    mpSafePrintf(parse->printDebug,
        "MP: u-blox CLASS=0x%02X, ID=0x%02X", 
        parse->buffer[parse->length - 2], data);
    
    parse->state = mpUbloxReadLengthLow;
    return true;
}

/**
 * @brief 读取消息类别
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return true=继续解析
 */
static bool mpUbloxReadClass(MP_PARSE_STATE *parse, uint8_t data)
{
    (void)data; // 避免未使用参数警告
    parse->state = mpUbloxReadId;
    return true;
}

/**
 * @brief 检测第二个同步字节 (0x62)
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return true=继续解析，false=同步失败
 */
static bool mpUbloxSync2(MP_PARSE_STATE *parse, uint8_t data)
{
    if (data != 0x62) {
        mpSafePrintf(parse->printDebug,
            "MP: u-blox第二个同步字节错误: 0x%02X", data);
        return false;
    }
    
    parse->state = mpUbloxReadClass;
    return true;
}

//----------------------------------------
// 公共接口函数
//----------------------------------------

/**
 * @brief u-blox协议前导字节检测
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return true=检测到u-blox协议前导，false=不是u-blox协议
 */
bool mpUbloxPreamble(MP_PARSE_STATE *parse, uint8_t data)
{
    // 检测第一个同步字节 (0xB5)
    if (data != 0xB5) {
        return false;
    }

    // 初始化u-blox解析状态
    parse->computeCrc = NULL; // u-blox使用自己的校验和算法
    parse->state = mpUbloxSync2;
    
    mpSafePrintf(parse->printDebug, 
        "MP: 检测到u-blox协议前导字节 0x%02X", data);
    
    return true;
}

//----------------------------------------
// u-blox协议辅助函数
//----------------------------------------

/**
 * @brief 获取u-blox消息头部信息
 * @param buffer 消息缓冲区
 * @param length 消息长度
 * @param header 输出的头部信息指针
 * @return true=成功解析头部，false=头部格式错误
 */
bool mpUbloxGetHeaderInfo(const uint8_t *buffer, uint16_t length, MP_UBLOX_HEADER **header)
{
    if (!buffer || !header || length < sizeof(MP_UBLOX_HEADER)) {
        return false;
    }
    
    *header = (MP_UBLOX_HEADER *)buffer;
    
    // 验证同步字节
    if ((*header)->sync1 != 0xB5 || (*header)->sync2 != 0x62) {
        return false;
    }
    
    return true;
}

/**
 * @brief 获取u-blox消息类型名称
 * @param messageClass 消息类别
 * @param messageId 消息ID
 * @return 消息类型名称字符串
 */
const char* mpUbloxGetMessageName(uint8_t messageClass, uint8_t messageId)
{
    // 常见的u-blox消息类型
    switch (messageClass) {
        case 0x01: // NAV (Navigation)
            switch (messageId) {
                case 0x07: return "NAV-PVT (Position Velocity Time)";
                case 0x35: return "NAV-SAT (Satellite Information)";
                case 0x03: return "NAV-STATUS (Receiver Navigation Status)";
                case 0x02: return "NAV-POSLLH (Position in LLH)";
                case 0x12: return "NAV-VELNED (Velocity in NED)";
                case 0x21: return "NAV-TIMEUTC (UTC Time Solution)";
                case 0x30: return "NAV-SVINFO (Space Vehicle Information)";
                default: return "NAV-Unknown";
            }
            
        case 0x02: // RXM (Receiver Manager)
            switch (messageId) {
                case 0x10: return "RXM-RAW (Raw Measurement Data)";
                case 0x11: return "RXM-SFRB (Subframe Buffer)";
                case 0x15: return "RXM-RAWX (Multi-GNSS Raw Measurement)";
                case 0x13: return "RXM-SFRBX (Broadcast Navigation Data)";
                default: return "RXM-Unknown";
            }
            
        case 0x04: // INF (Information)
            switch (messageId) {
                case 0x00: return "INF-ERROR (Error Message)";
                case 0x01: return "INF-WARNING (Warning Message)";
                case 0x02: return "INF-NOTICE (Notice Message)";
                case 0x03: return "INF-TEST (Test Message)";
                case 0x04: return "INF-DEBUG (Debug Message)";
                default: return "INF-Unknown";
            }
            
        case 0x05: // ACK (Acknowledge)
            switch (messageId) {
                case 0x00: return "ACK-NAK (Not Acknowledged)";
                case 0x01: return "ACK-ACK (Acknowledged)";
                default: return "ACK-Unknown";
            }
            
        case 0x06: // CFG (Configuration)
            switch (messageId) {
                case 0x00: return "CFG-PRT (Port Configuration)";
                case 0x01: return "CFG-MSG (Message Configuration)";
                case 0x02: return "CFG-INF (Information Message Configuration)";
                case 0x09: return "CFG-CFG (Configuration Management)";
                case 0x08: return "CFG-RATE (Navigation/Measurement Rate Settings)";
                default: return "CFG-Unknown";
            }
            
        case 0x0A: // MON (Monitoring)
            switch (messageId) {
                case 0x04: return "MON-VER (Receiver/Software Version)";
                case 0x02: return "MON-IO (I/O Subsystem Status)";
                case 0x06: return "MON-MSGPP (Message Parse and Process)";
                case 0x07: return "MON-RXBUF (Receiver Buffer Status)";
                case 0x08: return "MON-TXBUF (Transmitter Buffer Status)";
                case 0x09: return "MON-HW (Hardware Status)";
                default: return "MON-Unknown";
            }
            
        default:
            return "Unknown Class";
    }
}

/**
 * @brief 验证u-blox消息的完整性
 * @param buffer 消息缓冲区
 * @param length 消息长度
 * @return true=消息完整且校验和正确，false=消息损坏
 */
bool mpUbloxVerifyMessage(const uint8_t *buffer, uint16_t length)
{
    if (!buffer || length < sizeof(MP_UBLOX_HEADER) + 2) {
        return false;
    }
    
    MP_UBLOX_HEADER *header;
    if (!mpUbloxGetHeaderInfo(buffer, length, &header)) {
        return false;
    }
    
    // 验证消息长度
    uint16_t expectedLength = sizeof(MP_UBLOX_HEADER) + header->length + 2;
    if (length != expectedLength) {
        return false;
    }
    
    // 计算并验证Fletcher校验和
    uint8_t checksumA = 0, checksumB = 0;
    
    // 从CLASS字段开始计算校验和
    for (int i = 2; i < (length - 2); i++) {
        checksumA += buffer[i];
        checksumB += checksumA;
    }
    
    // 验证校验和
    return (buffer[length - 2] == checksumA && buffer[length - 1] == checksumB);
}

/**
 * @brief 解析u-blox NAV-PVT消息获取位置信息
 * @param buffer 消息缓冲区
 * @param length 消息长度
 * @param latitude 输出纬度（度）
 * @param longitude 输出经度（度）
 * @param height 输出高度（毫米）
 * @return true=解析成功，false=消息格式错误
 */
bool mpUbloxParseNavPvt(const uint8_t *buffer, uint16_t length, 
                        double *latitude, double *longitude, int32_t *height)
{
    if (!buffer || !latitude || !longitude || !height || length < (6 + 92 + 2)) {
        return false;
    }
    
    MP_UBLOX_HEADER *header = (MP_UBLOX_HEADER *)buffer;
    
    // 验证这是NAV-PVT消息
    if (header->messageClass != 0x01 || header->messageId != 0x07 || header->length != 92) {
        return false;
    }
    
    // 提取位置信息（从消息负载偏移位置获取）
    const uint8_t *payload = buffer + sizeof(MP_UBLOX_HEADER);
    
    // 纬度和经度以1e-7度为单位存储在偏移24和28处
    int32_t lat_raw = *(int32_t*)(payload + 24);
    int32_t lon_raw = *(int32_t*)(payload + 28);
    int32_t height_raw = *(int32_t*)(payload + 32); // 海拔高度（椭球面）
    
    *latitude = lat_raw * 1e-7;
    *longitude = lon_raw * 1e-7;
    *height = height_raw;
    
    return true;
} 