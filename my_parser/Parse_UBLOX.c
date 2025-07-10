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
        if (parse->eomCallback) {
            parse->eomCallback(parse, parse->protocolIndex);
        }
    } else {
        if (parse->badCrc) {
            parse->badCrc(parse);
        }
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
        if(parse->badCrc) parse->badCrc(parse);
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
    
    msgp_util_safePrintf(parse->printDebug,
        "MP: u-blox消息长度: %d字节", scratchPad->ublox.bytesRemaining);
    
    // 检查消息长度是否合理
    if (scratchPad->ublox.bytesRemaining > (parse->bufferLength - parse->length - 2)) {
        msgp_util_safePrintf(parse->printDebug,
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
    
    msgp_util_safePrintf(parse->printDebug,
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
        msgp_util_safePrintf(parse->printDebug,
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
bool msgp_ublox_preamble(MP_PARSE_STATE *parse, uint8_t data)
{
    if (data != 0xB5) return false;

    parse->length = 0;
    parse->buffer[parse->length++] = data;
    parse->computeCrc = NULL;
    parse->state = mpUbloxSync2;
    
    msgp_util_safePrintf(parse->printDebug, 
        "MP: 检测到u-blox协议前导字节 0x%02X", data);
    
    return true;
}

//----------------------------------------
// u-blox协议辅助函数
//----------------------------------------

uint16_t msgp_ublox_getMessageNumber(const MP_PARSE_STATE *parse)
{
    if (!parse || parse->length < 4) return 0;
    return ((uint16_t)parse->buffer[2] << 8) | parse->buffer[3];
}

uint8_t msgp_ublox_getClass(const MP_PARSE_STATE *parse)
{
    if (!parse || parse->length < 3) return 0;
    return parse->buffer[2];
}

uint8_t msgp_ublox_getId(const MP_PARSE_STATE *parse)
{
    if (!parse || parse->length < 4) return 0;
    return parse->buffer[3];
}

const uint8_t* msgp_ublox_getPayload(const MP_PARSE_STATE *parse, uint16_t *length)
{
    if (!parse || !length || parse->length < 8) { // header(6) + ck(2)
        if (length) *length = 0;
        return NULL;
    }
    MP_UBLOX_HEADER* header = (MP_UBLOX_HEADER*)parse->buffer;
    if ( (sizeof(MP_UBLOX_HEADER) + header->length + 2) != parse->length) {
         *length = 0;
        return NULL;
    }

    *length = header->length;
    return parse->buffer + sizeof(MP_UBLOX_HEADER);
}

/**
 * @brief 获取u-blox消息类型名称
 * @param messageClass 消息类别
 * @param messageId 消息ID
 * @return 消息类型名称字符串
 */
const char* msgp_ublox_getMessageName(uint8_t messageClass, uint8_t messageId)
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