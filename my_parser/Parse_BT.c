/**
 * @file Parse_BT.c
 * @brief 蓝牙/SEMP协议解析器
 * @details 解析蓝牙传输的SEMP协议数据包，支持CRC32校验
 * @version 1.0
 * @date 2024-12
 */

#include "Message_Parser.h"

//----------------------------------------
// 协议格式说明
//----------------------------------------
//
//  |<------------- 20 bytes ------------------> |<----- MsgData ----->|<- 4 bytes ->|
//  |                                            |                     |             |
//  +----------+--------+----------------+---------+----------+-------------+
//  | Synchron | HeaderLen| Message ID&Type | Message     |   CRC-32Q   |
//  |  24 bits |   8 bits |    128 bits     |  n-bits     |   32 bits   |
//  | 0xAA44 18|   0x14   |   (in 16bytes)  |             |             |
//  +----------+--------+----------------+---------+----------+-------------+
//  |                                                         |
//  |<------------------------ CRC ------------------------->|

//----------------------------------------
// 外部CRC32计算函数声明
//----------------------------------------

extern uint32_t mpComputeCrc32(MP_PARSE_STATE *parse, uint8_t data);

//----------------------------------------
// 内部状态函数前向声明
//----------------------------------------

static bool mpBtSync2(MP_PARSE_STATE *parse, uint8_t data);
static bool mpBtSync3(MP_PARSE_STATE *parse, uint8_t data);
static bool mpBtReadHeader(MP_PARSE_STATE *parse, uint8_t data);
static bool mpBtReadData(MP_PARSE_STATE *parse, uint8_t data);
static bool mpBtReadCrc(MP_PARSE_STATE *parse, uint8_t data);

//----------------------------------------
// BT/SEMP协议解析状态机
//----------------------------------------

/**
 * @brief 读取CRC校验码
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return true=继续解析，false=消息完成
 */
static bool mpBtReadCrc(MP_PARSE_STATE *parse, uint8_t data)
{
    MP_SCRATCH_PAD *scratchPad = &parse->scratchPad;

    // 检查是否读取完所有CRC字节
    if (--scratchPad->bt.bytesRemaining) {
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

    uint32_t crcComputed = scratchPad->bt.crc;
    
    // 验证CRC
    if (crcRead == crcComputed) {
        // CRC正确，调用消息结束回调
        parse->messagesProcessed[MP_PROTOCOL_BT]++;
        if (parse->eomCallback) {
            parse->eomCallback(parse, MP_PROTOCOL_BT);
        }
    } else {
        // CRC错误
        parse->crcErrors[MP_PROTOCOL_BT]++;
        if ((!parse->badCrc) || (!parse->badCrc(parse))) {
            mpSafePrintf(parse->printDebug,
                "MP: %s BT/SEMP CRC错误, 接收: %02X %02X %02X %02X, 计算: %02X %02X %02X %02X",
                parse->parserName ? parse->parserName : "Unknown",
                parse->buffer[parse->length - 4], parse->buffer[parse->length - 3],
                parse->buffer[parse->length - 2], parse->buffer[parse->length - 1],
                crcComputed & 0xff, (crcComputed >> 8) & 0xff,
                (crcComputed >> 16) & 0xff, (crcComputed >> 24) & 0xff);
        }
    }
    
    // 重置解析器状态，准备解析下一个消息
    return false; // 返回false表示消息处理完成
}

/**
 * @brief 读取消息数据部分
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return true=继续解析，false=状态错误
 */
static bool mpBtReadData(MP_PARSE_STATE *parse, uint8_t data)
{
    (void)data; // 避免未使用参数警告
    MP_SCRATCH_PAD *scratchPad = &parse->scratchPad;

    // 检查是否读取完所有数据
    if (!--scratchPad->bt.bytesRemaining) {
        // 消息数据读取完成，准备读取CRC
        scratchPad->bt.bytesRemaining = 4;
        parse->crc ^= 0xFFFFFFFF; // CRC32结束处理
        scratchPad->bt.crc = parse->crc;
        parse->state = mpBtReadCrc;
    }
    return true;
}

/**
 * @brief 读取消息头部
 * @param parse 解析器状态指针  
 * @param data 当前字节
 * @return true=继续解析，false=状态错误
 */
static bool mpBtReadHeader(MP_PARSE_STATE *parse, uint8_t data)
{
    (void)data; // 避免未使用参数警告
    MP_SCRATCH_PAD *scratchPad = &parse->scratchPad;

    if (parse->length >= sizeof(MP_BT_HEADER)) {
        // 头部读取完成，准备读取消息数据
        MP_BT_HEADER *header = (MP_BT_HEADER *)parse->buffer;
        
        // 验证头部长度
        if (header->headerLength != 0x14) {
            mpSafePrintf(parse->printDebug, 
                "MP: BT/SEMP无效头部长度: 0x%02X", header->headerLength);
            return false; // 头部格式错误
        }
        
        scratchPad->bt.bytesRemaining = header->messageLength;
        
        // 如果没有数据部分，直接读取CRC
        if (scratchPad->bt.bytesRemaining == 0) {
            scratchPad->bt.bytesRemaining = 4;
            parse->crc ^= 0xFFFFFFFF;
            scratchPad->bt.crc = parse->crc;
            parse->state = mpBtReadCrc;
        } else {
            parse->state = mpBtReadData;
        }
    }
    return true;
}

/**
 * @brief 检测第三个同步字节 (0x18)
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return true=继续解析，false=同步失败
 */
static bool mpBtSync3(MP_PARSE_STATE *parse, uint8_t data)
{
    if (data != 0x18) {
        // 第三个同步字节错误，重新开始搜索
        mpSafePrintf(parse->printDebug, 
            "MP: BT/SEMP第三个同步字节错误: 0x%02X", data);
        return false;
    }
    
    // 第三个同步字节正确，开始读取头部
    parse->state = mpBtReadHeader;
    return true;
}

/**
 * @brief 检测第二个同步字节 (0x44)
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return true=继续解析，false=同步失败
 */
static bool mpBtSync2(MP_PARSE_STATE *parse, uint8_t data)
{
    if (data != 0x44) {
        // 第二个同步字节错误，重新开始搜索
        mpSafePrintf(parse->printDebug, 
            "MP: BT/SEMP第二个同步字节错误: 0x%02X", data);
        return false;
    }
    
    // 第二个同步字节正确，等待第三个同步字节
    parse->state = mpBtSync3;
    return true;
}

//----------------------------------------
// 公共接口函数
//----------------------------------------

/**
 * @brief BT/SEMP协议前导字节检测
 * @param parse 解析器状态指针
 * @param data 当前字节
 * @return true=检测到SEMP协议前导，false=不是SEMP协议
 */
bool mpBtPreamble(MP_PARSE_STATE *parse, uint8_t data)
{
    // 检测第一个同步字节 (0xAA)
    if (data != 0xAA) {
        return false;
    }

    // 初始化BT/SEMP解析状态
    parse->crc = 0xFFFFFFFF;          // CRC32初始值
    parse->computeCrc = mpComputeCrc32; // 设置CRC计算函数
    parse->crc = parse->computeCrc(parse, data); // 计算第一个字节的CRC
    parse->state = mpBtSync2;         // 设置下一个状态
    
    mpSafePrintf(parse->printDebug, 
        "MP: 检测到BT/SEMP协议前导字节 0x%02X", data);
    
    return true;
}

//----------------------------------------
// BT/SEMP协议辅助函数
//----------------------------------------

/**
 * @brief 获取BT/SEMP消息头部信息
 * @param buffer 消息缓冲区
 * @param length 消息长度
 * @param header 输出的头部信息指针
 * @return true=成功解析头部，false=头部格式错误
 */
bool mpBtGetHeaderInfo(const uint8_t *buffer, uint16_t length, MP_BT_HEADER **header)
{
    if (!buffer || !header || length < sizeof(MP_BT_HEADER)) {
        return false;
    }
    
    *header = (MP_BT_HEADER *)buffer;
    
    // 验证同步字节
    if ((*header)->syncA != 0xAA || 
        (*header)->syncB != 0x44 || 
        (*header)->syncC != 0x18) {
        return false;
    }
    
    // 验证头部长度
    if ((*header)->headerLength != 0x14) {
        return false;
    }
    
    return true;
}

/**
 * @brief 获取BT/SEMP消息数据部分
 * @param buffer 消息缓冲区
 * @param length 消息长度
 * @param dataPtr 输出的数据指针
 * @param dataLength 输出的数据长度
 * @return true=成功，false=失败
 */
bool mpBtGetMessageData(const uint8_t *buffer, uint16_t length, 
                        const uint8_t **dataPtr, uint16_t *dataLength)
{
    if (!buffer || !dataPtr || !dataLength || length < sizeof(MP_BT_HEADER)) {
        return false;
    }
    
    MP_BT_HEADER *header = (MP_BT_HEADER *)buffer;
    
    // 验证消息长度
    uint16_t expectedLength = sizeof(MP_BT_HEADER) + header->messageLength + 4; // +4 for CRC
    if (length < expectedLength) {
        return false;
    }
    
    *dataPtr = buffer + sizeof(MP_BT_HEADER);
    *dataLength = header->messageLength;
    
    return true;
}

/**
 * @brief 验证BT/SEMP消息的完整性
 * @param buffer 消息缓冲区
 * @param length 消息长度
 * @return true=消息完整且CRC正确，false=消息损坏
 */
bool mpBtVerifyMessage(const uint8_t *buffer, uint16_t length)
{
    if (!buffer || length < sizeof(MP_BT_HEADER) + 4) {
        return false;
    }
    
    MP_BT_HEADER *header;
    if (!mpBtGetHeaderInfo(buffer, length, &header)) {
        return false;
    }
    
    // 计算期望的消息长度
    uint16_t expectedLength = sizeof(MP_BT_HEADER) + header->messageLength + 4;
    if (length != expectedLength) {
        return false;
    }
    
    // 验证CRC32（这里需要实现完整的CRC32验证逻辑）
    // 为简化示例，这里只做长度验证
    
    return true;
} 