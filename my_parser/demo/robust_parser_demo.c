/**
 * @file robust_parser_demo.c
 * @brief 健壮性解析器演示
 * @details 演示解析器如何处理一个包含正确、错误和噪声数据的混合数据流
 */
#include "../Message_Parser.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

// 引入各协议的Preamble函数声明
bool mpBtPreamble(MP_PARSE_STATE *parse, uint8_t data);
bool mpNmeaPreamble(MP_PARSE_STATE *parse, uint8_t data);
bool mpUbloxPreamble(MP_PARSE_STATE *parse, uint8_t data);

// 定义我们想要使用的协议解析器列表（在这个demo中我们只用3个）
static const MP_PARSER_INFO robustParsers[] = {
    { "BT/SEMP",  mpBtPreamble },
    { "NMEA",     mpNmeaPreamble },
    { "u-blox",   mpUbloxPreamble },
};
static const uint16_t robustParserCount = sizeof(robustParsers) / sizeof(robustParsers[0]);

// 一个大型混合数据包，包含正常、错误CRC和噪声数据
static const uint8_t mixed_stream[] = {
    // 1. 一条正常的NMEA消息
    '$', 'G', 'P', 'R', 'M', 'C', ',', '1', '2', '3', '5', '1', '9', ',', 'A', ',', '4', '8', '0', '7', '.', '0', '3', '8', ',', 'N', ',', '0', '1', '1', '3', '1', '.', '0', '0', '0', ',', 'E', ',', '0', '2', '2', '.', '4', ',', '0', '8', '4', '.', '4', ',', '2', '3', '0', '3', '9', '4', ',', '0', '0', '3', '.', '1', ',', 'W', '*', '6', 'A', '\r', '\n',
    // 随机噪声
    0xDE, 0xAD, 0xBE, 0xEF,
    // 2. 一条CRC错误的SEMP/BT消息 (最后一位CRC从0x6B改为0x00)
    0xAA, 0x44, 0x18, 0x14, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0xC4, 0xD8, 0xDB, 0x00,
    // 又一些噪声
    '$', 'G', 'A', 'R', 'B', 'A', 'G', 'E',
    // 3. 一条正常的u-blox消息 (NAV-STATUS, 0x01 0x03)
    0xB5, 0x62, 0x01, 0x03, 0x00, 0x00, 0x04, 0x0D,
    // 4. 一条CRC错误的NMEA消息 (校验和从*47改为*AB)
    '$', 'G', 'P', 'G', 'G', 'A', ',', '1', '2', '3', '5', '1', '9', ',', '4', '8', '0', '7', '.', '0', '3', '8', ',', 'N', ',', '0', '1', '1', '3', '1', '.', '0', '0', '0', ',', 'E', ',', '1', ',', '0', '8', ',', '0', '.', '9', ',', '5', '4', '5', '.', '4', ',', 'M', ',', '4', '6', '.', '9', ',', 'M', ',', ',', '*', 'A', 'B', '\r', '\n',
    // 5. 另一条正常的SEMP/BT消息
    0xAA, 0x44, 0x18, 0x14, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xDE, 0xAD, 0xBE, 0xEF, 0x50, 0x8C, 0x7E, 0x51
};

// --- 回调函数 ---
static void onMessageParsed(MP_PARSE_STATE *parse, MP_PROTOCOL_TYPE protocolType) {
    printf("\n\033[0;32m✅ SUCCESS: 解析到一条完整的消息!\033[0m\n");
    printf("  - 协议: %s\n", mpGetProtocolName(parse, protocolType));
    printf("  - 长度: %d 字节\n", parse->length);
    printf("  - 数据: ");
    for (int i = 0; i < parse->length && i < 24; i++) {
        printf("%02X ", parse->buffer[i]);
    }
    if (parse->length > 24) printf("...");
    printf("\n\n");
}

static bool onCrcFailed(MP_PARSE_STATE *parse) {
    printf("\n\033[0;31m❌ FAILURE: 检测到CRC/校验和错误!\033[0m\n");
    printf("  - 协议: %s\n", mpGetProtocolName(parse, parse->type));
    printf("  - 丢弃 %d 字节的数据\n\n", parse->length);
    return true; // 重置解析器，继续寻找下一条消息
}

void print_byte(uint8_t byte) {
    if (byte >= 32 && byte <= 126) {
        printf("处理字节: '%c' (0x%02X)\n", byte, byte);
    } else {
        printf("处理字节: . (0x%02X)\n", byte, byte);
    }
}


// --- 主程序 ---
int main(void) {
    printf("╔═══════════════════════════════════════════════════════╗\n");
    printf("║                健壮性解析器演示程序                     ║\n");
    printf("║      处理包含正确、错误CRC和噪声的混合数据流            ║\n");
    printf("╚═══════════════════════════════════════════════════════╝\n\n");

    // 初始化解析器
    static MP_PARSE_STATE parser;
    static uint8_t buffer[1024];

    if (!mpInitParser(&parser, buffer, sizeof(buffer),
                      robustParsers, robustParserCount,
                      onMessageParsed, onCrcFailed, "Robust-Demo",
                      NULL, NULL)) { // 在这个demo中我们不打印内部调试信息
        printf("❌ 解析器初始化失败！\n");
        return -1;
    }

    printf("解析器初始化成功。开始处理 %zu 字节的混合数据流...\n", sizeof(mixed_stream));
    printf("-----------------------------------------------------------\n");

    // 逐字节处理数据流
    for (size_t i = 0; i < sizeof(mixed_stream); i++) {
        // print_byte(mixed_stream[i]); // 如果需要，取消此行注释以查看每个字节的处理过程
        mpProcessByte(&parser, mixed_stream[i]);
    }

    printf("-----------------------------------------------------------\n");
    printf("数据流处理完毕。\n\n");

    // 打印最终统计
    mpPrintStats(&parser);

    printf("\n✅ 演示结束。\n");

    return 0;
} 