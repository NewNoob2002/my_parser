#ifndef SEMP_PARSER_H
#define SEMP_PARSER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 协议常量定义
#define SEMP_SYNC_BYTE_1    0xAA
#define SEMP_SYNC_BYTE_2    0x44
#define SEMP_SYNC_BYTE_3    0x18
#define SEMP_HEADER_LENGTH  0x14
#define SEMP_MAX_BUFFER_SIZE 1024
#define SEMP_CRC_SIZE       4

// 前向声明
typedef struct _SEMP_PARSE_STATE SEMP_PARSE_STATE;

// 回调函数类型定义
typedef bool (*sempStateFunction)(SEMP_PARSE_STATE *parse, uint8_t data);
typedef uint32_t (*sempCrcFunction)(SEMP_PARSE_STATE *parse, uint8_t data);
typedef void (*sempEomCallback)(SEMP_PARSE_STATE *parse, uint8_t messageType);
typedef bool (*sempBadCrcCallback)(SEMP_PARSE_STATE *parse);
typedef void (*sempPrintFunction)(bool enabled, const char *format, ...);

// 消息头部结构体 (20字节)
typedef struct _SEMP_MESSAGE_HEADER
{
    uint8_t syncA;              // 0xAA
    uint8_t syncB;              // 0x44
    uint8_t syncC;              // 0x18
    uint8_t headerLength;       // 头部长度 (0x14)
    uint16_t messageId;         // 消息ID
    uint16_t RESERVED1;         // 保留字段
    uint32_t RESERVED_time;     // 保留时间戳
    uint16_t messageLength;     // 消息长度
    uint16_t RESERVED2;         // 保留字段
    uint8_t sender;             // 发送者
    uint8_t messageType;        // 消息类型
    uint8_t Protocol;           // 协议版本
    int8_t MsgInterval;         // 消息间隔
} SEMP_MESSAGE_HEADER;

// 消息值结构体
typedef struct _SEMP_MESSAGE_VALUES
{
    uint32_t crc;               // CRC计算副本
    uint16_t bytesRemaining;    // 剩余字节数
} SEMP_MESSAGE_VALUES;

// 临时数据结构
typedef struct _SEMP_SCRATCH_PAD
{
    SEMP_MESSAGE_VALUES bluetooth;
} SEMP_SCRATCH_PAD;

// 解析器状态结构体
typedef struct _SEMP_PARSE_STATE
{
    sempStateFunction state;        // 当前状态函数
    sempCrcFunction computeCrc;     // CRC计算函数
    sempEomCallback eomCallback;    // 消息结束回调
    sempBadCrcCallback badCrc;      // CRC错误回调
    sempPrintFunction printDebug;   // 调试输出函数
    
    uint8_t *buffer;               // 消息缓冲区
    uint16_t length;               // 当前消息长度
    uint16_t maxLength;            // 最大缓冲区长度
    uint32_t crc;                  // 当前CRC值
    uint8_t type;                  // 消息类型
    const char *parserName;        // 解析器名称
    
    uint8_t scratchPad[sizeof(SEMP_SCRATCH_PAD)]; // 临时数据区
} SEMP_PARSE_STATE;

// 公共API函数声明

/**
 * @brief 初始化SEMP解析器
 * @param parse 解析器状态结构体指针
 * @param buffer 消息缓冲区
 * @param maxLength 缓冲区最大长度
 * @param eomCallback 消息结束回调函数
 * @param badCrcCallback CRC错误回调函数（可为NULL）
 * @param parserName 解析器名称（用于调试）
 */
void sempInitParser(SEMP_PARSE_STATE *parse, 
                   uint8_t *buffer, 
                   uint16_t maxLength,
                   sempEomCallback eomCallback,
                   sempBadCrcCallback badCrcCallback,
                   const char *parserName);

/**
 * @brief 处理单个接收字节
 * @param parse 解析器状态结构体指针
 * @param data 接收到的字节
 * @return true=继续接收字节，false=消息处理完成或错误
 */
bool sempProcessByte(SEMP_PARSE_STATE *parse, uint8_t data);

/**
 * @brief 处理数据缓冲区
 * @param parse 解析器状态结构体指针
 * @param data 数据缓冲区
 * @param length 数据长度
 * @return 已处理的字节数
 */
uint16_t sempProcessBuffer(SEMP_PARSE_STATE *parse, uint8_t *data, uint16_t length);

/**
 * @brief CRC32计算函数
 * @param parse 解析器状态结构体指针
 * @param data 要计算CRC的字节
 * @return 更新后的CRC值
 */
uint32_t sempBluetoothComputeCrc(SEMP_PARSE_STATE *parse, uint8_t data);

/**
 * @brief 调试输出函数
 * @param enabled 是否启用输出
 * @param format 格式字符串
 * @param ... 可变参数
 */
void sempPrintf(bool enabled, const char *format, ...);

// 内部状态函数声明（高级用户可直接使用）
bool sempFirstByte(SEMP_PARSE_STATE *parse, uint8_t data);
bool sempBluetoothPreamble(SEMP_PARSE_STATE *parse, uint8_t data);
bool sempBluetoothSync2(SEMP_PARSE_STATE *parse, uint8_t data);
bool sempBluetoothSync3(SEMP_PARSE_STATE *parse, uint8_t data);
bool sempBluetoothReadHeader(SEMP_PARSE_STATE *parse, uint8_t data);
bool sempBluetoothReadData(SEMP_PARSE_STATE *parse, uint8_t data);
bool sempBluetoothReadCrc(SEMP_PARSE_STATE *parse, uint8_t data);

// 使用示例宏定义
#define SEMP_INIT_PARSER_EXAMPLE(parser, buffer, eom_cb, bad_crc_cb, name) \
    sempInitParser(&parser, buffer, sizeof(buffer), eom_cb, bad_crc_cb, name)

#ifdef __cplusplus
}
#endif

#endif // SEMP_PARSER_H 