/**
 * @file func_test.c
 * @brief 解析器功能测试程序
 * @details 逐一测试每个协议解析器的功能是否正常
 * @version 1.0
 * @date 2024-12
 */

#include <stdio.h>
#include <string.h>
#include "../Message_Parser.h"
#include "../Parse_NMEA.h"
#include "../Parse_RTCM.h"

//----------------------------------------
// 测试状态
//----------------------------------------
typedef struct {
    int test_count;
    int pass_count;
    const char* current_protocol;
    bool message_parsed;
    uint16_t parsed_type;
} TestState;

TestState g_test_state;

//----------------------------------------
// 回调函数
//----------------------------------------
void eomCallback(SEMP_PARSE_STATE *parse, uint16_t type) {
    printf("  [Callback] 协议 '%s' 消息解析成功, 类型索引: %d, 长度: %d\n",
        sempGetTypeName(parse, type), type, parse->msg_length);
    g_test_state.message_parsed = true;
    g_test_state.parsed_type = type;
}

bool badCrcCallback(SEMP_PARSE_STATE *parse) {
    printf("  [Callback] 协议 '%s' 消息解析失败, 类型索引: %d, 长度: %d\n",
        sempGetTypeName(parse, parse->parser_type), parse->parser_type, parse->msg_length);
    g_test_state.message_parsed = false;
    g_test_state.parsed_type = parse->parser_type;
    return true;
}

void printError(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

void printDebug(const char *format, ...) {
    // 在功能测试中可以暂时关闭详细调试信息，或根据需要开启
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

int main() {
    printf("=================================\n");
    printf("  解析器功能测试 v1.0\n");
    printf("=================================\n");

    // 1. 定义协议解析器表
    const SEMP_PARSE_ROUTINE parsersTable[] = {
        sempNmeaPreamble,
        sempRtcmPreamble,
        sempCustomPreamble
    };
    const char *parserNamesTable[] = {
        "NMEA", "RTCM3", "Custom-BIN"
    };
    const uint8_t parserCount = sizeof(parsersTable) / sizeof(parsersTable[0]);

    // 2. 初始化主解析器
    SEMP_PARSE_STATE *parser = sempBeginParser(
        "FuncTestParser",
        parsersTable, parserCount,
        parserNamesTable, parserCount,
        1024, // scratchPadBytes
        2048, // bufferLength
        eomCallback,
        printError,
        printDebug,
        badCrcCallback
    );

    uint8_t testMessage1[] = {
        // 同步字节
        0xAA, 0x44, 0x18,
        // 头部长度
        0x14,
        // 消息ID (LED控制)
        0x01, 0x00,
        // 保留字段
        0x00, 0x00,
        // 时间戳
        0x00, 0x00, 0x00, 0x00,
        // 消息长度 (1字节数据)
        0x01, 0x00,
        // 保留字段
        0x00, 0x00,
        // 发送者
        0x01,
        // 消息类型 (控制命令)
        0x01,
        // 协议版本
        0x01,
        // 消息间隔
        0x00,
        // 数据：LED开启
        0x01,
        // CRC32 (需要根据实际数据计算)
        0x8E, 0x42, 0x7A, 0x75
    };
    
    // 测试数据2：温度传感器数据
    uint8_t testMessage2[] = {
        // 同步字节
        0xAA, 0x44, 0x18,
        // 头部长度
        0x14,
        // 消息ID (传感器数据)
        0x01, 0x10,
        // 保留字段
        0x00, 0x00,
        // 时间戳
        0x00, 0x00, 0x00, 0x00,
        // 消息长度 (4字节数据)
        0x04, 0x00,
        // 保留字段
        0x00, 0x00,
        // 发送者
        0x02,
        // 消息类型 (数据传输)
        0x02,
        // 协议版本
        0x01,
        // 消息间隔
        0x00,
        // 数据：温度值 23.5°C (IEEE 754 float)
        0x00, 0x00, 0xBC, 0x41,
        // CRC32
        0x00, 0x00, 0x00, 0x00  // 需要计算实际值
    };

    for (int i = 0; i < sizeof(testMessage1); i++) {
        sempParseNextByte(parser, testMessage1[i]);
    }

    for (int i = 0; i < sizeof(testMessage2); i++) {
        sempParseNextByte(parser, testMessage2[i]);
    }
} 