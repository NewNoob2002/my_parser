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
        if (parse->eomCallback) {
            parse->eomCallback(parse, parse->protocolIndex);
        }
    } else {
        if (parse->badCrc) {
            parse->badCrc(parse);
        }
    }
    return false;
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
        
        msgp_util_safePrintf(parse->printDebug,
            "MP: RTCM消息长度错误: %d字节 (最大1023字节, 缓冲区剩余: %d字节)",
            scratchPad->rtcm.messageLength,
            parse->bufferLength - parse->length - 3);
        return false;
    }
    
    msgp_util_safePrintf(parse->printDebug,
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
bool msgp_rtcm_preamble(MP_PARSE_STATE *parse, uint8_t data)
{
    if (data != 0xD3) return false;

    parse->length = 0;
    parse->buffer[parse->length++] = data;
    parse->computeCrc = NULL;
    parse->state = mpRtcmReadLengthLow;
    
    msgp_util_safePrintf(parse->printDebug, 
        "MP: 检测到RTCM协议前导字节 0x%02X", data);
    
    return true;
}

//----------------------------------------
// RTCM协议辅助函数
//----------------------------------------

uint16_t msgp_rtcm_getMessageNumber(const MP_PARSE_STATE *parse)
{
    if (!parse || parse->length < 3) return 0;
    // RTCM message number is in the 12 bits after the 12-bit length field
    return ((uint16_t)(parse->buffer[1] & 0x0F) << 8) | parse->buffer[2];
}

const uint8_t* msgp_rtcm_getPayload(const MP_PARSE_STATE *parse, uint16_t *length)
{
    if (!parse || !length || parse->length < 6) { // header(3) + crc(3)
        if (length) *length = 0;
        return NULL;
    }
    // Length is in the 10 bits after the preamble
    uint16_t payloadLength = ((uint16_t)(parse->buffer[1] & 0x03) << 8) | parse->buffer[2];
    
    if ( (3 + payloadLength + 3) != parse->length) {
        *length = 0;
        return NULL;
    }

    *length = payloadLength;
    return parse->buffer + 3;
} 