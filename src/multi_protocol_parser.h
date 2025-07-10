#ifndef MULTI_PROTOCOL_PARSER_H
#define MULTI_PROTOCOL_PARSER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//----------------------------------------
// 常量定义
//----------------------------------------

#define MMP_ALIGNMENT_MASK        7
#define MMP_MINIMUM_BUFFER_LENGTH 256
#define MMP_MAX_PARSER_NAME_LEN   32
#define MMP_MAX_SENTENCE_NAME     16

//----------------------------------------
// 宏定义
//----------------------------------------

#define MMP_ALIGN(x)   ((x + MMP_ALIGNMENT_MASK) & (~MMP_ALIGNMENT_MASK))

//----------------------------------------
// 前向声明
//----------------------------------------

typedef struct _MMP_PARSE_STATE MMP_PARSE_STATE;

//----------------------------------------
// 回调函数类型定义
//----------------------------------------

// 解析器函数类型
typedef bool (*MMP_PARSE_ROUTINE)(MMP_PARSE_STATE *parse, uint8_t data);

// 消息结束回调
typedef void (*MMP_EOM_CALLBACK)(MMP_PARSE_STATE *parse, uint16_t parserType);

// CRC错误回调
typedef bool (*MMP_BAD_CRC_CALLBACK)(MMP_PARSE_STATE *parse);

// 调试输出回调
typedef void (*MMP_PRINTF_CALLBACK)(const char *format, ...);

//----------------------------------------
// 协议类型枚举
//----------------------------------------

typedef enum {
    MMP_PROTOCOL_SEMP = 0,      // SEMP协议 (我们的协议)
    MMP_PROTOCOL_NMEA,          // NMEA GPS协议
    MMP_PROTOCOL_UBLOX,         // u-blox GPS协议
    MMP_PROTOCOL_RTCM,          // RTCM差分GPS协议
    MMP_PROTOCOL_SPARTN,        // SPARTN高精度协议
    MMP_PROTOCOL_SBF,           // Septentrio Binary Format
    MMP_PROTOCOL_UNICORE_HASH,  // 中海达Hash协议
    MMP_PROTOCOL_UNICORE_BIN,   // 中海达Binary协议
    MMP_PROTOCOL_COUNT          // 协议总数
} MMP_PROTOCOL_TYPE;

//----------------------------------------
// 消息头部结构体定义
//----------------------------------------

// SEMP协议头部 (20字节)
typedef struct _MMP_SEMP_HEADER
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
} MMP_SEMP_HEADER;

// NMEA协议信息
typedef struct _MMP_NMEA_INFO
{
    char sentenceName[MMP_MAX_SENTENCE_NAME]; // 语句名称
    uint8_t sentenceNameLength;               // 名称长度
} MMP_NMEA_INFO;

// u-blox协议头部
typedef struct _MMP_UBLOX_HEADER
{
    uint8_t sync1;              // 0xB5
    uint8_t sync2;              // 0x62
    uint8_t messageClass;       // 消息类别
    uint8_t messageId;          // 消息ID
    uint16_t length;            // 消息长度
} MMP_UBLOX_HEADER;

//----------------------------------------
// 协议特定数据结构
//----------------------------------------

typedef struct _MMP_SEMP_DATA
{
    uint32_t crc;               // CRC值
    uint16_t bytesRemaining;    // 剩余字节数
} MMP_SEMP_DATA;

typedef struct _MMP_NMEA_DATA
{
    MMP_NMEA_INFO info;         // NMEA信息
    uint8_t checksum;           // 校验和
} MMP_NMEA_DATA;

typedef struct _MMP_UBLOX_DATA
{
    uint8_t checksumA;          // 校验和A
    uint8_t checksumB;          // 校验和B
    uint16_t bytesRemaining;    // 剩余字节数
} MMP_UBLOX_DATA;

typedef struct _MMP_RTCM_DATA
{
    uint16_t messageLength;     // 消息长度
    uint16_t bytesRemaining;    // 剩余字节数
    uint32_t crc;               // CRC值
} MMP_RTCM_DATA;

//----------------------------------------
// 统一的临时数据区
//----------------------------------------

typedef struct _MMP_SCRATCH_PAD
{
    union {
        MMP_SEMP_DATA semp;
        MMP_NMEA_DATA nmea;
        MMP_UBLOX_DATA ublox;
        MMP_RTCM_DATA rtcm;
        // 可以根据需要添加更多协议
    };
} MMP_SCRATCH_PAD;

//----------------------------------------
// 主解析器状态结构体
//----------------------------------------

typedef struct _MMP_PARSE_STATE
{
    // 解析器配置
    const MMP_PARSE_ROUTINE *parsers;      // 解析器表
    const char * const *parserNames;       // 解析器名称表
    uint16_t parserCount;                  // 解析器数量
    const char *parserName;                // 解析器实例名称
    
    // 回调函数
    MMP_EOM_CALLBACK eomCallback;          // 消息结束回调
    MMP_BAD_CRC_CALLBACK badCrc;          // CRC错误回调
    MMP_PRINTF_CALLBACK printError;        // 错误输出
    MMP_PRINTF_CALLBACK printDebug;        // 调试输出
    
    // 状态管理
    MMP_PARSE_ROUTINE state;               // 当前状态函数
    uint16_t type;                         // 当前协议类型
    
    // CRC计算
    uint32_t (*computeCrc)(MMP_PARSE_STATE *parse, uint8_t data); // CRC计算函数
    uint32_t crc;                          // 当前CRC值
    
    // 缓冲区管理
    uint8_t *buffer;                       // 消息缓冲区
    uint16_t length;                       // 当前消息长度
    uint16_t bufferLength;                 // 缓冲区总长度
    
    // 临时数据区
    void *scratchPad;                      // 协议特定数据区
    
    // 统计信息
    uint32_t messagesProcessed[MMP_PROTOCOL_COUNT]; // 每种协议处理的消息数
    uint32_t crcErrors[MMP_PROTOCOL_COUNT];         // 每种协议的CRC错误数
    uint32_t totalBytes;                            // 总处理字节数
} MMP_PARSE_STATE;

//----------------------------------------
// 协议统计信息
//----------------------------------------

typedef struct _MMP_PROTOCOL_STATS
{
    const char *protocolName;
    uint32_t messagesProcessed;
    uint32_t crcErrors;
    float successRate;
} MMP_PROTOCOL_STATS;

//----------------------------------------
// 公共API函数声明
//----------------------------------------

/**
 * @brief 初始化多协议解析器
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
bool mmpInitParser(MMP_PARSE_STATE *parse,
                   uint8_t *buffer,
                   uint16_t bufferLength,
                   MMP_EOM_CALLBACK eomCallback,
                   MMP_BAD_CRC_CALLBACK badCrcCallback,
                   const char *parserName,
                   MMP_PRINTF_CALLBACK printError,
                   MMP_PRINTF_CALLBACK printDebug);

/**
 * @brief 处理单个接收字节
 * @param parse 解析器状态结构体指针
 * @param data 接收到的字节
 * @return true=继续接收，false=消息处理完成或错误
 */
bool mmpProcessByte(MMP_PARSE_STATE *parse, uint8_t data);

/**
 * @brief 批量处理数据缓冲区
 * @param parse 解析器状态结构体指针
 * @param data 数据缓冲区
 * @param length 数据长度
 * @return 已处理的字节数
 */
uint16_t mmpProcessBuffer(MMP_PARSE_STATE *parse, uint8_t *data, uint16_t length);

/**
 * @brief 获取当前活动的协议类型
 * @param parse 解析器状态结构体指针
 * @return 协议类型
 */
MMP_PROTOCOL_TYPE mmpGetActiveProtocol(const MMP_PARSE_STATE *parse);

/**
 * @brief 获取协议名称
 * @param protocolType 协议类型
 * @return 协议名称字符串
 */
const char* mmpGetProtocolName(MMP_PROTOCOL_TYPE protocolType);

/**
 * @brief 获取协议统计信息
 * @param parse 解析器状态结构体指针
 * @param stats 统计信息输出数组
 * @param maxCount 数组最大长度
 * @return 实际返回的统计信息数量
 */
uint16_t mmpGetProtocolStats(const MMP_PARSE_STATE *parse, 
                            MMP_PROTOCOL_STATS *stats, 
                            uint16_t maxCount);

/**
 * @brief 重置统计信息
 * @param parse 解析器状态结构体指针
 */
void mmpResetStats(MMP_PARSE_STATE *parse);

/**
 * @brief 打印协议统计信息
 * @param parse 解析器状态结构体指针
 */
void mmpPrintStats(const MMP_PARSE_STATE *parse);

//----------------------------------------
// 协议解析器前导函数声明
//----------------------------------------

bool mmpSempPreamble(MMP_PARSE_STATE *parse, uint8_t data);      // SEMP协议
bool mmpNmeaPreamble(MMP_PARSE_STATE *parse, uint8_t data);      // NMEA协议
bool mmpUbloxPreamble(MMP_PARSE_STATE *parse, uint8_t data);     // u-blox协议
bool mmpRtcmPreamble(MMP_PARSE_STATE *parse, uint8_t data);      // RTCM协议

//----------------------------------------
// 实用工具函数
//----------------------------------------

/**
 * @brief ASCII转半字节
 * @param data ASCII字符
 * @return 半字节值，失败返回-1
 */
int mmpAsciiToNibble(int data);

/**
 * @brief 安全的printf包装
 * @param callback printf回调函数
 * @param format 格式字符串
 * @param ... 可变参数
 */
void mmpSafePrintf(MMP_PRINTF_CALLBACK callback, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif // MULTI_PROTOCOL_PARSER_H 