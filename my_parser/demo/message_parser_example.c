/**
 * @file message_parser_example.c
 * @brief 模块化消息解析器实际应用示例
 * @details 演示在嵌入式应用中如何使用模块化消息解析器处理多种协议数据流
 * @version 1.0
 * @date 2024-12
 */

#include "../Message_Parser.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

// 引入各协议的Preamble函数声明
// 在实际项目中，这些可能位于各自的头文件中
bool mpBtPreamble(MP_PARSE_STATE *parse, uint8_t data);
bool mpNmeaPreamble(MP_PARSE_STATE *parse, uint8_t data);
bool mpUbloxPreamble(MP_PARSE_STATE *parse, uint8_t data);
bool mpRtcmPreamble(MP_PARSE_STATE *parse, uint8_t data);
bool mpUnicoreBinPreamble(MP_PARSE_STATE *parse, uint8_t data);
bool mpUnicoreHashPreamble(MP_PARSE_STATE *parse, uint8_t data);


// 定义我们想要使用的协议解析器列表
static const MP_PARSER_INFO myParsers[] = {
    { "BT/SEMP",      mpBtPreamble },
    { "NMEA",         mpNmeaPreamble },
    { "u-blox",       mpUbloxPreamble },
    { "RTCM",         mpRtcmPreamble },
    { "Unicore-Bin",  mpUnicoreBinPreamble },
    { "Unicore-Hash", mpUnicoreHashPreamble }
};
static const uint16_t myParserCount = sizeof(myParsers) / sizeof(myParsers[0]);


// 示例回调函数
void messageEndCallback(MP_PARSE_STATE *parse, MP_PROTOCOL_TYPE protocolType) {
    printf("✅  消息接收完毕! 协议: %s, 长度: %d\n", mpGetProtocolName(parse, protocolType), parse->length);
}

bool crcErrorCallback(MP_PARSE_STATE *parse) {
    printf("❌  CRC校验失败! 协议: %s\n", mpGetProtocolName(parse, parse->type));
    return true; /* 返回true表示我们希望重置解析器并继续 */
}

void printCallback(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}


int main(void) {
    printf("--- 消息解析器框架示例 (V2 - 动态注册) ---\n\n");

    /* 1. 初始化解析器状态和缓冲区 */
    static MP_PARSE_STATE parserState;
    static uint8_t messageBuffer[1024];

    /* 2. 初始化解析器 */
    bool initOk = mpInitParser(
        &parserState,
        messageBuffer,
        sizeof(messageBuffer),
        myParsers, /* 传入自定义的解析器列表 */
        myParserCount,
        messageEndCallback,
        crcErrorCallback,
        "MyCustomParser",
        printCallback,
        printCallback
    );

    if (!initOk) {
        printf("解析器初始化失败!\n");
        return -1;
    }

    printf("解析器初始化成功，注册了 %d 个协议。\n\n", myParserCount);
    mpListSupportedProtocols(&parserState);

    /* 3. 准备测试数据流 */
    uint8_t testStream[] = {
        /* NMEA 消息 */
        '$', 'G', 'P', 'G', 'G', 'A', ',', '1', '2', '3', '5', '1', '9', ',', '4', '8', '0', '7', '.', '0', '3', '8', ',', 'N', ',', '0', '1', '1', '3', '1', '.', '0', '0', '0', ',', 'E', ',', '1', ',', '0', '8', ',', '0', '.', '9', ',', '5', '4', '5', '.', '4', ',', 'M', ',', '4', '6', '.', '9', ',', 'M', ',', ',', '*','4', '7', '\r', '\n',
        /* u-blox 消息 */
        0xB5, 0x62, 0x01, 0x07, 0x00, 0x00, 0x08, 0x19,
        /* 无效数据 */
        0xDE, 0xAD, 0xBE, 0xEF
    };

    printf("--- 开始处理数据流 ---\n");

    /* 4. 逐字节处理数据 */
    for (size_t i = 0; i < sizeof(testStream); i++) {
        printf("-> '%c' (0x%02X)\n", (testStream[i] >= 32 && testStream[i] <= 126) ? testStream[i] : '.', testStream[i]);
        mpProcessByte(&parserState, testStream[i]);
    }

    printf("\n--- 数据流处理完毕 ---\n\n");

    /* 5. 打印最终统计信息 */
    mpPrintStats(&parserState);

    return 0;
} 