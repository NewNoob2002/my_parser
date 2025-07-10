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

//----------------------------------------
// 前向声明
//----------------------------------------

typedef struct _MP_PARSE_STATE MP_PARSE_STATE;

//----------------------------------------
// 协议类型枚举
//----------------------------------------

typedef enum {
    MP_PROTOCOL_BT = 0,          // 蓝牙/SEMP协议
    MP_PROTOCOL_NMEA,            // NMEA GPS协议
    MP_PROTOCOL_UBLOX,           // u-blox GPS协议
    MP_PROTOCOL_RTCM,            // RTCM差分GPS协议
    MP_PROTOCOL_UNICORE_BIN,     // 中海达二进制协议
    MP_PROTOCOL_UNICORE_HASH,    // 中海达Hash协议
    MP_PROTOCOL_COUNT            // 协议总数
} MP_PROTOCOL_TYPE;

//----------------------------------------
// 回调函数类型定义
//----------------------------------------

// 解析器函数类型
typedef bool (*MP_PARSE_ROUTINE)(MP_PARSE_STATE *parse, uint8_t data);

// 消息结束回调
typedef void (*MP_EOM_CALLBACK)(MP_PARSE_STATE *parse, MP_PROTOCOL_TYPE protocolType);

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
    uint8_t preambleBytes[4];           // 前导字节模式（最多4字节）
    uint8_t preambleLength;             // 前导字节长度
    const char *description;            // 协议描述
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

typedef struct _MP_PARSE_STATE {
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
    MP_PROTOCOL_TYPE type;                // 当前协议类型
    
    // CRC计算
    uint32_t (*computeCrc)(MP_PARSE_STATE *parse, uint8_t data); // CRC计算函数
    uint32_t crc;                         // 当前CRC值
    
    // 缓冲区管理
    uint8_t *buffer;                      // 消息缓冲区
    uint16_t length;                      // 当前消息长度
    uint16_t bufferLength;                // 缓冲区总长度
    
    // 临时数据区
    MP_SCRATCH_PAD scratchPad;            // 协议特定数据区
    
    // 统计信息
    uint32_t messagesProcessed[MP_PROTOCOL_COUNT]; // 每种协议处理的消息数
    uint32_t crcErrors[MP_PROTOCOL_COUNT];         // 每种协议的CRC错误数
    uint32_t totalBytes;                           // 总处理字节数
    uint32_t protocolSwitches;                     // 协议切换次数
} MP_PARSE_STATE;

//----------------------------------------
// 协议统计信息
//----------------------------------------

typedef struct _MP_PROTOCOL_STATS {
    const char *protocolName;
    const char *description;
    uint32_t messagesProcessed;
    uint32_t crcErrors;
    float successRate;
    bool isActive;
} MP_PROTOCOL_STATS;

//----------------------------------------
// 主要API函数声明
//----------------------------------------

/**
 * @brief 获取解析器版本信息
 * @return 版本字符串
 */
const char* mpGetVersion(void);

/**
 * @brief 初始化消息解析器
 * @param parse 解析器状态结构体指针
 * @param buffer 消息缓冲区
 * @param bufferLength 缓冲区长度
 * @param eomCallback 消息结束回调函数
 * @param badCrcCallback CRC错误回调函数（可为NULL）
 * @param parserName 解析器名称
 * @param printError 错误输出回调（可为NULL）
 * @param printDebug 调试输出回调（可为NULL）
 * @return true=初始化成功，false=失败
 */
bool mpInitParser(MP_PARSE_STATE *parse,
                  uint8_t *buffer,
                  uint16_t bufferLength,
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
bool mpProcessByte(MP_PARSE_STATE *parse, uint8_t data);

/**
 * @brief 批量处理数据缓冲区
 * @param parse 解析器状态结构体指针
 * @param data 数据缓冲区
 * @param length 数据长度
 * @return 已处理的字节数
 */
uint16_t mpProcessBuffer(MP_PARSE_STATE *parse, uint8_t *data, uint16_t length);

/**
 * @brief 获取当前活动的协议类型
 * @param parse 解析器状态结构体指针
 * @return 协议类型
 */
MP_PROTOCOL_TYPE mpGetActiveProtocol(const MP_PARSE_STATE *parse);

/**
 * @brief 获取协议名称
 * @param protocolType 协议类型
 * @return 协议名称字符串
 */
const char* mpGetProtocolName(MP_PROTOCOL_TYPE protocolType);

/**
 * @brief 获取协议描述
 * @param protocolType 协议类型
 * @return 协议描述字符串
 */
const char* mpGetProtocolDescription(MP_PROTOCOL_TYPE protocolType);

/**
 * @brief 获取协议统计信息
 * @param parse 解析器状态结构体指针
 * @param stats 统计信息输出数组
 * @param maxCount 数组最大长度
 * @return 实际返回的统计信息数量
 */
uint16_t mpGetProtocolStats(const MP_PARSE_STATE *parse, 
                           MP_PROTOCOL_STATS *stats, 
                           uint16_t maxCount);

/**
 * @brief 重置统计信息
 * @param parse 解析器状态结构体指针
 */
void mpResetStats(MP_PARSE_STATE *parse);

/**
 * @brief 打印协议统计信息
 * @param parse 解析器状态结构体指针
 */
void mpPrintStats(const MP_PARSE_STATE *parse);

/**
 * @brief 列出所有支持的协议
 * @param parse 解析器状态结构体指针
 */
void mpListSupportedProtocols(const MP_PARSE_STATE *parse);

//----------------------------------------
// 各协议解析器前导函数声明（在各自的文件中实现）
//----------------------------------------

bool mpBtPreamble(MP_PARSE_STATE *parse, uint8_t data);          // Parse_BT.c
bool mpNmeaPreamble(MP_PARSE_STATE *parse, uint8_t data);        // Parse_NMEA.c
bool mpUbloxPreamble(MP_PARSE_STATE *parse, uint8_t data);       // Parse_UBLOX.c
bool mpRtcmPreamble(MP_PARSE_STATE *parse, uint8_t data);        // Parse_RTCM.c
bool mpUnicoreBinPreamble(MP_PARSE_STATE *parse, uint8_t data);  // Parse_Unicore_Binary.c
bool mpUnicoreHashPreamble(MP_PARSE_STATE *parse, uint8_t data); // Parse_Unicore_Hash.c

//----------------------------------------
// 协议特定辅助函数声明
//----------------------------------------

// BT/SEMP协议辅助函数
bool mpBtGetHeaderInfo(const uint8_t *buffer, uint16_t length, MP_BT_HEADER **header);
bool mpBtGetMessageData(const uint8_t *buffer, uint16_t length, 
                        const uint8_t **dataPtr, uint16_t *dataLength);
bool mpBtVerifyMessage(const uint8_t *buffer, uint16_t length);

// NMEA协议辅助函数
const char* mpNmeaGetSentenceName(const MP_PARSE_STATE *parse);
int mpNmeaParseFields(const char *sentence, char fields[][32], int maxFields);
bool mpNmeaValidateSentence(const char *sentence);
const char* mpNmeaGetSentenceType(const char *sentenceName);

// u-blox协议辅助函数
bool mpUbloxGetHeaderInfo(const uint8_t *buffer, uint16_t length, MP_UBLOX_HEADER **header);
const char* mpUbloxGetMessageName(uint8_t messageClass, uint8_t messageId);
bool mpUbloxVerifyMessage(const uint8_t *buffer, uint16_t length);
bool mpUbloxParseNavPvt(const uint8_t *buffer, uint16_t length, 
                        double *latitude, double *longitude, int32_t *height);

// RTCM协议辅助函数
bool mpRtcmGetHeaderInfo(const uint8_t *buffer, uint16_t length, MP_RTCM_HEADER **header);
const char* mpRtcmGetMessageName(uint16_t messageNumber);
bool mpRtcmVerifyMessage(const uint8_t *buffer, uint16_t length);
bool mpRtcmGetPayload(const uint8_t *buffer, uint16_t length,
                      const uint8_t **payload, uint16_t *payloadLength);

// Unicore Hash协议辅助函数
bool mpUnicoreHashGetCommandName(const uint8_t *buffer, uint16_t length,
                                char *commandName, uint16_t commandNameSize);
bool mpUnicoreHashValidateCommand(const char *command);
int mpUnicoreHashParseFields(const char *command, char fields[][64], int maxFields);
const char* mpUnicoreHashGetCommandType(const char *commandName);
int mpUnicoreHashBuildCommand(const char *commandName, const char *fields[], 
                             int fieldCount, char *output, int outputSize);

//----------------------------------------
// 工具函数
//----------------------------------------

/**
 * @brief ASCII转半字节
 * @param data ASCII字符
 * @return 半字节值，失败返回-1
 */
int mpAsciiToNibble(int data);

/**
 * @brief 安全的printf包装
 * @param callback printf回调函数
 * @param format 格式字符串
 * @param ... 可变参数
 */
void mpSafePrintf(MP_PRINTF_CALLBACK callback, const char *format, ...);

/**
 * @brief 十六进制数据转字符串
 * @param data 数据缓冲区
 * @param length 数据长度
 * @param output 输出字符串缓冲区
 * @param outputSize 输出缓冲区大小
 * @return 实际输出的字符数
 */
int mpHexToString(const uint8_t *data, uint16_t length, char *output, uint16_t outputSize);

/**
 * @brief 计算数据的简单校验和
 * @param data 数据缓冲区
 * @param length 数据长度
 * @return 校验和值
 */
uint8_t mpCalculateChecksum(const uint8_t *data, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif // MESSAGE_PARSER_H 