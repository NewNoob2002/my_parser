/**
 * @file Message_Parser.h
 * @brief 统一消息解析器框架 - 主体解析器头文件
 * @details 管理多种协议解析器，提供统一的API接口和协议识别机制
 * @version 1.0
 * @date 2024-12
 */

#ifndef MESSAGE_PARSER_H
#define MESSAGE_PARSER_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef nullptr
#define nullptr NULL
#endif

//----------------------------------------
// 版本信息
//----------------------------------------

#define MESSAGE_PARSER_VERSION_MAJOR 1
#define MESSAGE_PARSER_VERSION_MINOR 0
#define MESSAGE_PARSER_VERSION_PATCH 0

extern const unsigned long semp_crc32Table[256];
//----------------------------------------
// 配置常量
//----------------------------------------

#define SEMP_MAX_PARSER_NAME_LEN 32
#define SEMP_MINIMUM_BUFFER_LENGTH 256
#define SEMP_ALIGNMENT_MASK 7

#define SEMP_ALIGN(x) ((x + SEMP_ALIGNMENT_MASK) & (~SEMP_ALIGNMENT_MASK))

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

// Call the application back at specified routine address.  Pass in the
// parse data structure containing the buffer containing the address of
// the message data and the length field containing the number of valid
// data bytes in the buffer.
//
// The type field contains the index into the parseTable which specifies
// the parser that successfully processed the incoming data.
//
// End of message callback routine
typedef void (*SEMP_EOM_CALLBACK)(SEMP_PARSE_STATE *parse, uint16_t type);

// CRC callback routine
typedef uint32_t (*SEMP_COMPUTE_CRC)(SEMP_PARSE_STATE *parse, uint8_t dataByte);

// Normally this routine pointer is set to nullptr.  The parser calls
// the badCrcCallback routine when the default CRC or checksum calculation
// fails.  This allows an upper layer to adjust the CRC calculation if
// necessary.  Return true when the CRC calculation fails otherwise
// return false if the alternate CRC or checksum calculation is successful.
typedef bool (*SEMP_BAD_CRC_CALLBACK)(SEMP_PARSE_STATE *parse); // Parser state

// 调试输出回调
typedef void (*SEMP_PRINTF_CALLBACK)(const char *format, ...);

// Length of the sentence name array
#define SEMP_NMEA_SENTENCE_NAME_BYTES    16

// NMEA parser scratch area
typedef struct _SEMP_NMEA_VALUES
{
    uint8_t sentenceName[SEMP_NMEA_SENTENCE_NAME_BYTES]; // Sentence name
    uint8_t sentenceNameLength; // Length of the sentence name
} SEMP_NMEA_VALUES;

// RTCM parser scratch area
typedef struct _SEMP_RTCM_VALUES
{
    uint32_t crc;            // Copy of CRC calculation before CRC bytes
    uint16_t bytesRemaining; // Bytes remaining in RTCM CRC calculation
    uint16_t message;        // Message number
} SEMP_RTCM_VALUES;

// UBLOX parser scratch area
typedef struct _SEMP_UBLOX_VALUES
{
    uint16_t bytesRemaining; // Bytes remaining in field
    uint16_t message;        // Message number
    uint8_t ck_a;            // U-blox checksum byte 1
    uint8_t ck_b;            // U-blox checksum byte 2
} SEMP_UBLOX_VALUES;

// Unicore parser scratch area
typedef struct _SEMP_UNICORE_BINARY_VALUES
{
    uint32_t crc;            // Copy of CRC calculation before CRC bytes
    uint16_t bytesRemaining; // Bytes remaining in RTCM CRC calculation
} SEMP_UNICORE_BINARY_VALUES;

// Length of the sentence name array
#define SEMP_UNICORE_HASH_SENTENCE_NAME_BYTES    16

// Unicore hash (#) parser scratch area
typedef struct _SEMP_UNICORE_HASH_VALUES
{
    uint8_t bytesRemaining;     // Bytes remaining in field
    uint8_t checksumBytes;      // Number of checksum bytes
    uint8_t sentenceName[SEMP_UNICORE_HASH_SENTENCE_NAME_BYTES]; // Sentence name
    uint8_t sentenceNameLength; // Length of the sentence name
} SEMP_UNICORE_HASH_VALUES;

typedef struct _SEMP_CUSTOM_VALUES
{
    uint32_t crc;            // Copy of CRC calculation before CRC bytes
    uint16_t bytesRemaining; // Bytes remaining in RTCM CRC calculation
} SEMP_CUSTOM_VALUES;

// Overlap the scratch areas since only one parser is active at a time
typedef union
{
    SEMP_NMEA_VALUES nmea;       // NMEA specific values
    SEMP_RTCM_VALUES rtcm;       // RTCM specific values
    SEMP_UBLOX_VALUES ublox;     // U-blox specific values
    SEMP_UNICORE_BINARY_VALUES unicoreBinary; // Unicore binary specific values
    SEMP_UNICORE_HASH_VALUES unicoreHash;     // Unicore hash (#) specific values
    SEMP_CUSTOM_VALUES custom;
} SEMP_SCRATCH_PAD;
//----------------------------------------
// 主解析器状态结构体
//----------------------------------------

typedef struct _SEMP_PARSE_STATE {
  // 解析器配置
  const char *parserName; // Name of parser

  const SEMP_PARSE_ROUTINE *parsers_table; // Table of parsers
  const char *const *parserNames_table;    // Table of parser names
  uint8_t parsers_count;   // Number of parsers
  uint8_t parser_type;      // Current parser type

  SEMP_PARSE_ROUTINE state;      // Parser state routine
  SEMP_EOM_CALLBACK eomCallback; // End of message callback routine
  SEMP_BAD_CRC_CALLBACK badCrc;  // Bad CRC callback routine
  SEMP_COMPUTE_CRC computeCrc;   // Routine to compute the CRC when set

  SEMP_PRINTF_CALLBACK printError; // 错误输出
  SEMP_PRINTF_CALLBACK printDebug; // 调试输出

  void *scratchPad; // Parser scratchpad area
  // CRC计算
  uint32_t crc; // 当前CRC值

  // 缓冲区管理
  uint8_t *buffer;       // 消息缓冲区
  uint16_t msg_length;   // 当前消息长度
  uint16_t buffer_length; // 缓冲区总长度

} SEMP_PARSE_STATE;

//----------------------------------------
// Protocol specific types
//----------------------------------------

// Define the Unicore message header
typedef struct _SEMP_UNICORE_HEADER
{
    uint8_t syncA;            // 0xaa
    uint8_t syncB;            // 0x44
    uint8_t syncC;            // 0xb5
    uint8_t cpuIdlePercent;   // CPU Idle Percentage 0-100
    uint16_t messageId;       // Message ID
    uint16_t messageLength;   // Message Length
    uint8_t referenceTime;    // Reference time（GPST or BDST)
    uint8_t timeStatus;       // Time status
    uint16_t weekNumber;      // Reference week number
    uint32_t secondsOfWeek;   // GPS seconds from the beginning of the
                              // reference week, accurate to the millisecond
    uint32_t RESERVED;

    uint8_t releasedVersion;  // Release version
    uint8_t leapSeconds;      // Leap sec
    uint16_t outputDelayMSec; // Output delay time, ms
} SEMP_UNICORE_HEADER;

typedef struct _SEMP_CUSTOM_HEADER
{
    uint8_t syncA;          // 0xaa
    uint8_t syncB;          // 0x44
    uint8_t syncC;          // 0x18
    uint8_t headerLength;     // CPU Idle Percentage 0-100
    uint16_t messageId;     // Message ID
    uint16_t RESERVED1;      // reserved
    uint32_t RESERVED_time; // reserved time stamp
    uint16_t messageLength;     // massage length
    uint16_t RESERVED2;

    uint8_t sender;  // Release version
    uint8_t messageType;      // Leap sec
    uint8_t Protocol; // Output delay time, ms

    int8_t MsgInterval;

} SEMP_CUSTOM_HEADER;

//----------------------------------------
// 主要API函数声明
//----------------------------------------

// Allocate and initialize a parse data structure
SEMP_PARSE_STATE * sempBeginParser(
    const char *parserName, \
    const SEMP_PARSE_ROUTINE *parsersTable, \
    uint8_t parsersCount, \
    const char * const *parserNamesTable, \
    uint8_t parserNamesCount, \
    uint16_t scratchPadBytes, \
    uint16_t bufferLength, \
    SEMP_EOM_CALLBACK eomCallback, \
    SEMP_PRINTF_CALLBACK printError, \
    SEMP_PRINTF_CALLBACK printDebug, \
    SEMP_BAD_CRC_CALLBACK badCrcCallback);

// The routine sempFirstByte is used to determine if the first byte
// is the preamble for a message.
bool sempFirstByte(SEMP_PARSE_STATE *parse, uint8_t data);

// The routine sempParseNextByte is used to parse the next data byte from a raw data stream.
void sempParseNextByte(SEMP_PARSE_STATE *parse, uint8_t data);

// The routine sempStopParser frees the parse data structure and sets
// the pointer value to nullptr to prevent future references to the
// freed structure.
void sempStopParser(SEMP_PARSE_STATE **parse);

// Print the contents of the parser data structure
void sempPrintParserConfiguration(SEMP_PARSE_STATE *parse, SEMP_PRINTF_CALLBACK print);

// Format and print a line of text
void sempPrintf(SEMP_PRINTF_CALLBACK print, const char *format, ...);

// Print a line of text
void sempPrintln(SEMP_PRINTF_CALLBACK print, const char *string);

// Translates state value into an ASCII state name
const char * sempGetStateName(const SEMP_PARSE_STATE *parse);
// Translate the type value into an ASCII type name
const char * sempGetTypeName(SEMP_PARSE_STATE *parse, uint16_t type);

void sempPrintParserConfiguration(SEMP_PARSE_STATE *parse, SEMP_PRINTF_CALLBACK print);

// Enable or disable debug output
void sempEnableDebugOutput(SEMP_PARSE_STATE *parse, SEMP_PRINTF_CALLBACK print);
void sempDisableDebugOutput(SEMP_PARSE_STATE *parse);

// Enable or disable error output
void sempEnableErrorOutput(SEMP_PARSE_STATE *parse, SEMP_PRINTF_CALLBACK print);
void sempDisableErrorOutput(SEMP_PARSE_STATE *parse);

//----------------------------------------
// 各协议解析器前导函数声明（在各自的文件中实现）
//----------------------------------------
bool sempCustomPreamble(SEMP_PARSE_STATE *parse, uint8_t data);

//----------------------------------------
// 工具函数
//----------------------------------------

// 内存分配函数(User implementation)
void * semp_util_malloc(size_t size);
void semp_util_free(void *ptr);

/**
 * @brief 分配解析结构体
 * @param printDebug 调试输出回调函数
 * @param scratchPadBytes 解析器scratch区域大小
 * @param bufferLength 缓冲区大小
 * @return 解析结构体指针
 */
SEMP_PARSE_STATE * sempAllocateParseStructure(
    SEMP_PRINTF_CALLBACK printDebug,
    uint16_t scratchPadBytes,
    uint16_t bufferLength
    );
/**
 * @brief ASCII转半字节
 * @param data ASCII字符
 * @return 半字节值，失败返回-1
 */
int semp_util_asciiToNibble(int data);

/**
 * @brief 十六进制数据转字符串
 * @param data 数据缓冲区
 * @param length 数据长度
 * @param output 输出字符串缓冲区
 * @param outputSize 输出缓冲区大小
 * @return 实际输出的字符数
 */
int semp_util_hexToString(const uint8_t *data, uint16_t length, char *output,
                          uint16_t outputSize);

/**
 * @brief 计算数据的简单校验和
 * @param data 数据缓冲区
 * @param length 数据长度
 * @return 校验和值
 */
uint8_t semp_util_calculateChecksum(const uint8_t *data, uint16_t length);

/**
 * @brief 解析由分隔符分割的字段
 * @param sentence 输入字符串
 * @param fields 输出的字段数组
 * @param maxFields 字段数组的最大容量
 * @param delimiter 字段分隔符 (e.g., ',')
 * @param terminator 句子终止符 (e.g., '*')
 * @return 解析出的字段数量
 */
int semp_util_parse_delimited_fields(const char *sentence, void *fields,
                                     int maxFields, int fieldSize,
                                     char delimiter, char terminator);

#ifdef __cplusplus
}
#endif

#endif // MESSAGE_PARSER_H