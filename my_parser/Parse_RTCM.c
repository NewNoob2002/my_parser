/**
 * @file Parse_RTCM.c
 * @brief RTCM差分GPS协议解析器
 * @details 解析RTCM SC-104版本3协议，支持CRC24校验
 * @version 1.0
 * @date 2024-12
 */

#include "Message_Parser.h"

//----------------------------------------
// 协议格式说明
//----------------------------------------
//
//  RTCM Version 3 Protocol Format:
//  +----------+----------+----------+----------+----------+----------+
//  | Preamble | Reserved | Length   | Message  | Payload  | CRC24    |
//  | 8 bits   | 6 bits   | 10 bits  | 12 bits  | n bytes  | 24 bits  |
//  | 0xD3     | 000000   |          |          |          |          |
//  +----------+----------+----------+----------+----------+----------+
//             |                                 |          |
//             |<--------- CRC Calculation -------------------->|
//
//  Examples:
//  - RTCM 1005: D3 00 13 3ED 00001 [19 bytes] [CRC24]
//  - RTCM 1077: D3 00 xx 435 [data] [CRC24]

//----------------------------------------
// 内部常量
//----------------------------------------

#define RTCM_CRC24_POLY    0x1864CFB    // CRC24多项式

//----------------------------------------
// 内部状态函数前向声明
//----------------------------------------

static bool mpRtcmReadLengthLow(MP_PARSE_STATE *parse, uint8_t data);
static bool mpRtcmReadPayload(MP_PARSE_STATE *parse, uint8_t data);
static bool mpRtcmReadCrc24_1(MP_PARSE_STATE *parse, uint8_t data);
static bool mpRtcmReadCrc24_2(MP_PARSE_STATE *parse, uint8_t data);
static bool mpRtcmReadCrc24_3(MP_PARSE_STATE *parse, uint8_t data);

//----------------------------------------
// RTCM CRC24计算
//----------------------------------------

/**
 * @brief 计算RTCM CRC24校验码
 * @param data 数据缓冲区
 * @param length 数据长度
 * @return CRC24值
 */
static uint32_t mpRtcmComputeCrc24(const uint8_t *data, uint16_t length)
{
    uint32_t crc = 0;
    
    for (uint16_t i = 0; i < length; i++) {
        crc ^= (uint32_t)data[i] << 16;
        
        for (int j = 0; j < 8; j++) {
            if (crc & 0x800000) {
                crc = (crc << 1) ^ RTCM_CRC24_POLY;
            } else {
                crc <<= 1;
            }
            crc &= 0xFFFFFF; // 保持24位
        }
    }
    
    return crc;
}

//----------------------------------------
// RTCM协议解析状态机
//----------------------------------------

/**
 * @brief 读取第三个CRC24字节并验证消息
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return false=消息处理完成
 */
static bool mpRtcmReadCrc24_3(MP_PARSE_STATE *parse, uint8_t data)
{
    MP_SCRATCH_PAD *scratchPad = &parse->scratchPad;
    
    // 组合完整的CRC24值
    scratchPad->rtcm.crc |= data;
    
    // 计算接收数据的CRC24（不包括CRC本身）
    uint32_t computedCrc = mpRtcmComputeCrc24(parse->buffer, parse->length - 3);
    
    // 验证CRC24
    if (scratchPad->rtcm.crc == computedCrc) {
        // CRC正确，消息解析成功
        parse->messagesProcessed[MP_PROTOCOL_RTCM]++;
        if (parse->eomCallback) {
            parse->eomCallback(parse, MP_PROTOCOL_RTCM);
        }
        
        mpSafePrintf(parse->printDebug,
            "MP: RTCM消息解析成功, 类型=%d, 长度=%d",
            scratchPad->rtcm.messageNumber, scratchPad->rtcm.messageLength);
    } else {
        // CRC错误
        parse->crcErrors[MP_PROTOCOL_RTCM]++;
        mpSafePrintf(parse->printDebug,
            "MP: %s RTCM CRC24错误, 接收: 0x%06X, 计算: 0x%06X",
            parse->parserName ? parse->parserName : "Unknown",
            scratchPad->rtcm.crc, computedCrc);
    }
    
    return false; // 消息处理完成
}

/**
 * @brief 读取第二个CRC24字节
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return true=继续解析
 */
static bool mpRtcmReadCrc24_2(MP_PARSE_STATE *parse, uint8_t data)
{
    MP_SCRATCH_PAD *scratchPad = &parse->scratchPad;
    
    // 组合CRC24中间字节
    scratchPad->rtcm.crc |= ((uint32_t)data << 8);
    parse->state = mpRtcmReadCrc24_3;
    
    return true;
}

/**
 * @brief 读取第一个CRC24字节
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return true=继续解析
 */
static bool mpRtcmReadCrc24_1(MP_PARSE_STATE *parse, uint8_t data)
{
    MP_SCRATCH_PAD *scratchPad = &parse->scratchPad;
    
    // 开始组合CRC24高字节
    scratchPad->rtcm.crc = ((uint32_t)data << 16);
    parse->state = mpRtcmReadCrc24_2;
    
    return true;
}

/**
 * @brief 读取消息负载数据
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return true=继续解析
 */
static bool mpRtcmReadPayload(MP_PARSE_STATE *parse, uint8_t data)
{
    MP_SCRATCH_PAD *scratchPad = &parse->scratchPad;
    
    (void)data; // 避免未使用参数警告
    
    // 检查是否读取完所有负载数据
    if (!--scratchPad->rtcm.bytesRemaining) {
        // 负载读取完成，开始读取CRC24
        parse->state = mpRtcmReadCrc24_1;
    }
    
    return true;
}

/**
 * @brief 读取消息长度低字节并提取消息类型
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return true=继续解析，false=消息长度错误
 */
static bool mpRtcmReadLengthLow(MP_PARSE_STATE *parse, uint8_t data)
{
    MP_SCRATCH_PAD *scratchPad = &parse->scratchPad;
    
    // 组合完整的长度值（从之前的高字节和当前低字节）
    scratchPad->rtcm.messageLength |= data;
    scratchPad->rtcm.bytesRemaining = scratchPad->rtcm.messageLength;
    
    // 检查消息长度是否合理（RTCM最大消息长度为1023字节）
    if (scratchPad->rtcm.messageLength > 1023 || 
        scratchPad->rtcm.messageLength > (parse->bufferLength - parse->length - 3)) {
        
        mpSafePrintf(parse->printDebug,
            "MP: RTCM消息长度错误: %d字节 (最大1023字节, 缓冲区剩余: %d字节)",
            scratchPad->rtcm.messageLength,
            parse->bufferLength - parse->length - 3);
        return false;
    }
    
    mpSafePrintf(parse->printDebug,
        "MP: RTCM消息长度: %d字节", scratchPad->rtcm.messageLength);
    
    // 如果负载长度为0，直接读取CRC24
    if (scratchPad->rtcm.bytesRemaining == 0) {
        parse->state = mpRtcmReadCrc24_1;
    } else {
        parse->state = mpRtcmReadPayload;
    }
    
    return true;
}

//----------------------------------------
// 公共接口函数
//----------------------------------------

/**
 * @brief RTCM协议前导字节检测
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return true=检测到RTCM协议前导，false=不是RTCM协议
 */
bool mpRtcmPreamble(MP_PARSE_STATE *parse, uint8_t data)
{
    // 检测RTCM前导字节 (0xD3)
    if (data != 0xD3) {
        return false;
    }

    // 初始化RTCM解析状态
    parse->computeCrc = NULL; // RTCM使用自己的CRC24算法
    parse->state = mpRtcmReadLengthLow;
    
    mpSafePrintf(parse->printDebug, 
        "MP: 检测到RTCM协议前导字节 0x%02X", data);
    
    return true;
}

//----------------------------------------
// RTCM协议辅助函数
//----------------------------------------

/**
 * @brief 获取RTCM消息头部信息
 * @param buffer 消息缓冲区
 * @param length 消息长度
 * @param header 输出的头部信息指针
 * @return true=成功解析头部，false=头部格式错误
 */
bool mpRtcmGetHeaderInfo(const uint8_t *buffer, uint16_t length, MP_RTCM_HEADER **header)
{
    if (!buffer || !header || length < 3) {
        return false;
    }
    
    // RTCM头部需要手动解析，因为它是位字段格式
    static MP_RTCM_HEADER rtcmHeader;
    
    if (buffer[0] != 0xD3) {
        return false;
    }
    
    rtcmHeader.preamble = buffer[0];
    
    // 提取消息长度（10位）：高2位在buffer[1]的低2位，低8位在buffer[2]
    rtcmHeader.messageLength = ((buffer[1] & 0x03) << 8) | buffer[2];
    
    // 如果有足够的数据，提取消息编号（12位）
    if (length >= 5) {
        rtcmHeader.messageNumber = (buffer[3] << 4) | (buffer[4] >> 4);
    } else {
        rtcmHeader.messageNumber = 0;
    }
    
    *header = &rtcmHeader;
    return true;
}

/**
 * @brief 获取RTCM消息类型名称
 * @param messageNumber RTCM消息编号
 * @return 消息类型名称字符串
 */
const char* mpRtcmGetMessageName(uint16_t messageNumber)
{
    // 常见的RTCM消息类型
    switch (messageNumber) {
        case 1001: return "RTCM 1001 - L1-Only GPS RTK Observables";
        case 1002: return "RTCM 1002 - Extended L1-Only GPS RTK Observables";
        case 1003: return "RTCM 1003 - L1&L2 GPS RTK Observables";
        case 1004: return "RTCM 1004 - Extended L1&L2 GPS RTK Observables";
        case 1005: return "RTCM 1005 - Stationary RTK Reference Station ARP";
        case 1006: return "RTCM 1006 - Stationary RTK Reference Station ARP with Height";
        case 1007: return "RTCM 1007 - Antenna Descriptor";
        case 1008: return "RTCM 1008 - Antenna Descriptor & Serial Number";
        case 1009: return "RTCM 1009 - L1-Only GLONASS RTK Observables";
        case 1010: return "RTCM 1010 - Extended L1-Only GLONASS RTK Observables";
        case 1011: return "RTCM 1011 - L1&L2 GLONASS RTK Observables";
        case 1012: return "RTCM 1012 - Extended L1&L2 GLONASS RTK Observables";
        case 1013: return "RTCM 1013 - System Parameters";
        case 1019: return "RTCM 1019 - GPS Satellite Ephemeris Data";
        case 1020: return "RTCM 1020 - GLONASS Satellite Ephemeris Data";
        case 1033: return "RTCM 1033 - Receiver and Antenna Descriptors";
        case 1074: return "RTCM 1074 - GPS MSM4";
        case 1075: return "RTCM 1075 - GPS MSM5";
        case 1076: return "RTCM 1076 - GPS MSM6";
        case 1077: return "RTCM 1077 - GPS MSM7";
        case 1084: return "RTCM 1084 - GLONASS MSM4";
        case 1085: return "RTCM 1085 - GLONASS MSM5";
        case 1086: return "RTCM 1086 - GLONASS MSM6";
        case 1087: return "RTCM 1087 - GLONASS MSM7";
        case 1094: return "RTCM 1094 - Galileo MSM4";
        case 1095: return "RTCM 1095 - Galileo MSM5";
        case 1096: return "RTCM 1096 - Galileo MSM6";
        case 1097: return "RTCM 1097 - Galileo MSM7";
        case 1124: return "RTCM 1124 - BeiDou MSM4";
        case 1125: return "RTCM 1125 - BeiDou MSM5";
        case 1126: return "RTCM 1126 - BeiDou MSM6";
        case 1127: return "RTCM 1127 - BeiDou MSM7";
        case 1230: return "RTCM 1230 - GLONASS L1 and L2 Code-Phase Biases";
        default:
            if (messageNumber >= 1001 && messageNumber <= 1299) {
                return "RTCM Reserved Message";
            } else if (messageNumber >= 4001 && messageNumber <= 4095) {
                return "RTCM Proprietary Message";
            } else {
                return "RTCM Unknown Message";
            }
    }
}

/**
 * @brief 验证RTCM消息的完整性
 * @param buffer 消息缓冲区
 * @param length 消息长度
 * @return true=消息完整且CRC正确，false=消息损坏
 */
bool mpRtcmVerifyMessage(const uint8_t *buffer, uint16_t length)
{
    if (!buffer || length < 6) { // 最小长度：前导+长度+CRC
        return false;
    }
    
    MP_RTCM_HEADER *header;
    if (!mpRtcmGetHeaderInfo(buffer, length, &header)) {
        return false;
    }
    
    // 验证消息长度
    uint16_t expectedLength = 3 + header->messageLength + 3; // 前导+长度+负载+CRC24
    if (length != expectedLength) {
        return false;
    }
    
    // 计算并验证CRC24
    uint32_t computedCrc = mpRtcmComputeCrc24(buffer, length - 3);
    
    // 提取接收到的CRC24
    uint32_t receivedCrc = ((uint32_t)buffer[length - 3] << 16) |
                          ((uint32_t)buffer[length - 2] << 8) |
                          buffer[length - 1];
    
    return (computedCrc == receivedCrc);
}

/**
 * @brief 获取RTCM消息负载数据
 * @param buffer 消息缓冲区
 * @param length 消息长度
 * @param payload 输出的负载数据指针
 * @param payloadLength 输出的负载长度
 * @return true=成功，false=失败
 */
bool mpRtcmGetPayload(const uint8_t *buffer, uint16_t length,
                      const uint8_t **payload, uint16_t *payloadLength)
{
    if (!buffer || !payload || !payloadLength) {
        return false;
    }
    
    MP_RTCM_HEADER *header;
    if (!mpRtcmGetHeaderInfo(buffer, length, &header)) {
        return false;
    }
    
    // 验证消息长度
    if (length < (3 + header->messageLength + 3)) {
        return false;
    }
    
    *payload = buffer + 3; // 跳过前导和长度字段
    *payloadLength = header->messageLength;
    
    return true;
} 