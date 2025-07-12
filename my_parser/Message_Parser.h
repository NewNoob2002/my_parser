/**
 * @file Message_Parser.h
 * @brief 统一消息解析器框架 - 主体解析器头文件
 * @details 管理多种协议解析器，提供统一的API接口和协议识别机制
 * @version 1.0
 * @date 2024-12
 */

#ifndef MESSAGE_PARSER_H
#define MESSAGE_PARSER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//----------------------------------------
// 版本信息
//----------------------------------------

#define MESSAGE_PARSER_VERSION_MAJOR    1
#define MESSAGE_PARSER_VERSION_MINOR    0
#define MESSAGE_PARSER_VERSION_PATCH    0

//----------------------------------------
// 配置常量
//----------------------------------------

#define MP_MAX_PARSER_NAME_LEN     32
#define MP_MAX_SENTENCE_NAME       16
#define MP_MINIMUM_BUFFER_LENGTH   256
#define MP_ALIGNMENT_MASK          7

#define MP_ALIGN(x)   ((x + MP_ALIGNMENT_MASK) & (~MP_ALIGNMENT_MASK))

extern const unsigned long semp_crc32Table[256];
//----------------------------------------
// 前向声明
//----------------------------------------

typedef struct _SEMP_PARSE_STATE SEMP_PARSE_STATE;

//----------------------------------------
// 回调函数类型定义
//----------------------------------------

// 解析器函数类型
typedef bool (*SEMP_PARSE_ROUTINE)(SEMP_PARSE_STATE *parse, // Parser state
                                   uint8_t data); // Incoming data byte

// 消息结束回调
typedef void (*MP_EOM_CALLBACK)(MP_PARSE_STATE *parse, uint16_t protocolIndex);

// CRC错误回调
typedef bool (*MP_BAD_CRC_CALLBACK)(MP_PARSE_STATE *parse);

// 调试输出回调
typedef void (*MP_PRINTF_CALLBACK)(const char *format, ...);

//----------------------------------------
// 协议解析器信息结构
//----------------------------------------

typedef struct _MP_PARSER_INFO {
    const char *name;                    // 解析器名称
    MP_PARSE_ROUTINE preambleFunction;  // 前导字节检测函数
} MP_PARSER_INFO;

//----------------------------------------
// 消息头部结构体定义
//----------------------------------------

// 蓝牙/SEMP协议头部 (20字节)
typedef struct _MP_BT_HEADER {
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
    uint8_t protocol;           // 协议版本
    int8_t msgInterval;         // 消息间隔
} __attribute__((packed)) MP_BT_HEADER;

// NMEA协议信息
typedef struct _MP_NMEA_INFO {
    char sentenceName[MP_MAX_SENTENCE_NAME]; // 语句名称
    uint8_t sentenceNameLength;              // 名称长度
} MP_NMEA_INFO;

// u-blox协议头部
typedef struct _MP_UBLOX_HEADER {
    uint8_t sync1;              // 0xB5
    uint8_t sync2;              // 0x62
    uint8_t messageClass;       // 消息类别
    uint8_t messageId;          // 消息ID
    uint16_t length;            // 消息长度
} __attribute__((packed)) MP_UBLOX_HEADER;

// RTCM协议头部
typedef struct _MP_RTCM_HEADER {
    uint8_t preamble;           // 0xD3
    uint16_t messageLength;     // 消息长度（包含头部信息）
    uint16_t messageNumber;     // 消息编号
} __attribute__((packed)) MP_RTCM_HEADER;

//----------------------------------------
// 协议特定数据结构
//----------------------------------------

typedef struct _MP_BT_DATA {
    uint32_t crc;               // CRC值
    uint16_t bytesRemaining;    // 剩余字节数
} MP_BT_DATA;

typedef struct _MP_NMEA_DATA {
    MP_NMEA_INFO info;          // NMEA信息
    uint8_t checksum;           // 校验和
} MP_NMEA_DATA;

typedef struct _MP_UBLOX_DATA {
    uint8_t checksumA;          // 校验和A
    uint8_t checksumB;          // 校验和B
    uint16_t bytesRemaining;    // 剩余字节数
} MP_UBLOX_DATA;

typedef struct _MP_RTCM_DATA {
    uint16_t messageLength;     // 消息长度
    uint16_t bytesRemaining;    // 剩余字节数
    uint32_t crc;               // CRC值
    uint16_t messageNumber;     // 消息编号
} MP_RTCM_DATA;

typedef struct _MP_UNICORE_DATA {
    uint32_t crc;               // CRC值
    uint16_t bytesRemaining;    // 剩余字节数
    uint16_t messageLength;     // 消息长度
    uint8_t messageType;        // 消息类型
} MP_UNICORE_DATA;

//----------------------------------------
// 统一的临时数据区
//----------------------------------------

typedef struct _MP_SCRATCH_PAD {
    union {
        MP_BT_DATA bt;
        MP_NMEA_DATA nmea;
        MP_UBLOX_DATA ublox;
        MP_RTCM_DATA rtcm;
        MP_UNICORE_DATA unicore;
    };
} MP_SCRATCH_PAD;

//----------------------------------------
// 主解析器状态结构体
//----------------------------------------

typedef struct _SEMP_PARSE_STATE {
    // 解析器配置
    const MP_PARSER_INFO *parsers;        // 解析器信息表
    uint16_t parserCount;                 // 解析器数量
    const char *parserName;               // 解析器实例名称
    
    // 回调函数
    MP_EOM_CALLBACK eomCallback;          // 消息结束回调
    MP_BAD_CRC_CALLBACK badCrc;          // CRC错误回调
    MP_PRINTF_CALLBACK printError;        // 错误输出
    MP_PRINTF_CALLBACK printDebug;        // 调试输出
    
    // 状态管理
    MP_PARSE_ROUTINE state;               // 当前状态函数
    uint16_t protocolIndex;               // 当前协议索引
    
    // CRC计算
    uint32_t (*computeCrc)(MP_PARSE_STATE *parse, uint8_t data); // CRC计算函数
    uint32_t crc;                         // 当前CRC值
    
    // 缓冲区管理
    uint8_t *buffer;                      // 消息缓冲区
    uint16_t length;                      // 当前消息长度
    uint16_t bufferLength;                // 缓冲区总长度
    
    // 临时数据区
    MP_SCRATCH_PAD scratchPad;            // 协议特定数据区
    
} MP_PARSE_STATE;

//----------------------------------------
// 主要API函数声明
//----------------------------------------

/**
 * @brief 获取解析器版本信息
 * @return 版本字符串
 */
const char* msgp_getVersion(void);

/**
 * @brief 初始化消息解析器
 * @param parse 解析器状态结构体指针
 * @param buffer 消息缓冲区
 * @param bufferLength 缓冲区长度
 * @param userParsers 用户提供的解析器信息表
 * @param userParserCount 用户提供的解析器数量
 * @param eomCallback 消息结束回调函数
 * @param badCrcCallback CRC错误回调函数（可为NULL）
 * @param parserName 解析器名称
 * @param printError 错误输出回调（可为NULL）
 * @param printDebug 调试输出回调（可为NULL）
 * @return true=初始化成功，false=失败
 */
bool msgp_init(MP_PARSE_STATE *parse,
                  uint8_t *buffer,
                  uint16_t bufferLength,
                  const MP_PARSER_INFO *userParsers,
                  uint16_t userParserCount,
                  MP_EOM_CALLBACK eomCallback,
                  MP_BAD_CRC_CALLBACK badCrcCallback,
                  const char *parserName,
                  MP_PRINTF_CALLBACK printError,
                  MP_PRINTF_CALLBACK printDebug);

/**
 * @brief 处理单个接收字节
 * @param parse 解析器状态结构体指针
 * @param data 接收到的字节
 * @return true=继续接收，false=消息处理完成或错误
 */
bool msgp_processByte(MP_PARSE_STATE *parse, uint8_t data);

/**
 * @brief 获取当前活动的协议类型
 * @param parse 解析器状态结构体指针
 * @return 协议类型
 */
uint16_t msgp_getActiveProtocol(const MP_PARSE_STATE *parse);

/**
 * @brief 获取协议名称
 * @param parse 解析器状态结构体指针
 * @param protocolType 协议类型
 * @return 协议名称字符串
 */
const char* msgp_getProtocolName(const MP_PARSE_STATE *parse, uint16_t protocolIndex);

/**
 * @brief 获取协议描述
 * @param protocolType 协议类型
 * @return 协议描述字符串
 */
const char* msgp_getProtocolDescription(uint16_t protocolIndex);

//----------------------------------------
// 各协议解析器前导函数声明（在各自的文件中实现）
//----------------------------------------

bool msgp_bt_preamble(MP_PARSE_STATE *parse, uint8_t data);          // Parse_BT.c
bool msgp_nmea_preamble(MP_PARSE_STATE *parse, uint8_t data);        // Parse_NMEA.c
bool msgp_ublox_preamble(MP_PARSE_STATE *parse, uint8_t data);       // Parse_UBLOX.c
bool msgp_rtcm_preamble(MP_PARSE_STATE *parse, uint8_t data);        // Parse_RTCM.c
bool msgp_unicore_bin_preamble(MP_PARSE_STATE *parse, uint8_t data);  // Parse_Unicore_Binary.c
bool msgp_unicore_hash_preamble(MP_PARSE_STATE *parse, uint8_t data); // Parse_Unicore_Hash.c

//----------------------------------------
// 协议特定辅助函数声明
//----------------------------------------

// BT/SEMP
uint16_t msgp_bt_getMessageId(const MP_PARSE_STATE *parse);
uint8_t msgp_bt_getMessageType(const MP_PARSE_STATE *parse);
const uint8_t* msgp_bt_getPayload(const MP_PARSE_STATE *parse, uint16_t *length);

// NMEA
const char* msgp_nmea_getSentenceName(const MP_PARSE_STATE *parse);
int msgp_nmea_parseFields(const MP_PARSE_STATE *parse, char fields[][32], int maxFields);
const char* msgp_nmea_getSentenceType(const char *sentenceName);

// u-blox
uint16_t msgp_ublox_getMessageNumber(const MP_PARSE_STATE *parse); // (Class << 8) | ID
uint8_t msgp_ublox_getClass(const MP_PARSE_STATE *parse);
uint8_t msgp_ublox_getId(const MP_PARSE_STATE *parse);
const uint8_t* msgp_ublox_getPayload(const MP_PARSE_STATE *parse, uint16_t *length);

// RTCM
uint16_t msgp_rtcm_getMessageNumber(const MP_PARSE_STATE *parse);
const uint8_t* msgp_rtcm_getPayload(const MP_PARSE_STATE *parse, uint16_t *length);

// Unicore Binary
uint16_t msgp_unicore_bin_getMessageId(const MP_PARSE_STATE *parse);
const uint8_t* msgp_unicore_bin_getPayload(const MP_PARSE_STATE *parse, uint16_t *length);

// Unicore Hash
const char* msgp_unicore_hash_getSentenceName(const MP_PARSE_STATE *parse);
int msgp_unicore_hash_parseFields(const MP_PARSE_STATE *parse, char fields[][64], int maxFields);
const char* msgp_unicore_hash_getCommandType(const char *commandName);

//----------------------------------------
// 工具函数
//----------------------------------------

/**
 * @brief ASCII转半字节
 * @param data ASCII字符
 * @return 半字节值，失败返回-1
 */
int msgp_util_asciiToNibble(int data);

/**
 * @brief 安全的printf包装
 * @param callback printf回调函数
 * @param format 格式字符串
 * @param ... 可变参数
 */
void msgp_util_safePrintf(MP_PRINTF_CALLBACK callback, const char *format, ...);

/**
 * @brief 十六进制数据转字符串
 * @param data 数据缓冲区
 * @param length 数据长度
 * @param output 输出字符串缓冲区
 * @param outputSize 输出缓冲区大小
 * @return 实际输出的字符数
 */
int msgp_util_hexToString(const uint8_t *data, uint16_t length, char *output, uint16_t outputSize);

/**
 * @brief 计算数据的简单校验和
 * @param data 数据缓冲区
 * @param length 数据长度
 * @return 校验和值
 */
uint8_t msgp_util_calculateChecksum(const uint8_t *data, uint16_t length);

/**
 * @brief 解析由分隔符分割的字段
 * @param sentence 输入字符串
 * @param fields 输出的字段数组
 * @param maxFields 字段数组的最大容量
 * @param delimiter 字段分隔符 (e.g., ',')
 * @param terminator 句子终止符 (e.g., '*')
 * @return 解析出的字段数量
 */
int msgp_util_parse_delimited_fields(const char *sentence, void *fields, int maxFields, int fieldSize, char delimiter, char terminator);

#ifdef __cplusplus
}
#endif

#endif // MESSAGE_PARSER_H 