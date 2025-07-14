/**
 * @file main.cpp
 * @brief 消息解析器库的综合测试程序
 * @details 执行一系列严格的测试，以验证解析器的正确性、健壮性和性能。
 */
#include <stdio.h>
#include <string.h>
#include <vector>

// 核心解析器头文件
#include "Message_Parser.h" 

// C源文件中函数的C++链接声明
extern "C" {
    // 各协议的前导码检测函数
    bool msgp_bt_preamble(MP_PARSE_STATE *parse, uint8_t data);
    bool msgp_nmea_preamble(MP_PARSE_STATE *parse, uint8_t data);
    bool msgp_ublox_preamble(MP_PARSE_STATE *parse, uint8_t data);
    bool msgp_rtcm_preamble(MP_PARSE_STATE *parse, uint8_t data);
    bool msgp_unicore_bin_preamble(MP_PARSE_STATE *parse, uint8_t data);
    bool msgp_unicore_hash_preamble(MP_PARSE_STATE *parse, uint8_t data);
    
    // 辅助函数
    const char* msgp_getProtocolName(const MP_PARSE_STATE *parse, uint16_t protocolIndex);
    uint16_t msgp_bt_getMessageId(const MP_PARSE_STATE *parse);
    const char* msgp_nmea_getSentenceName(const MP_PARSE_STATE *parse);
    uint16_t msgp_ublox_getMessageNumber(const MP_PARSE_STATE *parse);
}

// --- 测试配置 ---

// 定义协议索引，便于在switch语句中使用
enum TestProtocols {
    PROTOCOL_BT,
    PROTOCOL_NMEA,
    PROTOCOL_UBLOX,
    PROTOCOL_RTCM,
    PROTOCOL_UNICORE_BIN,
    PROTOCOL_UNICORE_HASH
};

// 定义要参与测试的解析器列表
static const MP_PARSER_INFO testParsers[] = {
    { "BT/SEMP",      msgp_bt_preamble },
    { "NMEA",         msgp_nmea_preamble },
    { "u-blox",       msgp_ublox_preamble },
    { "RTCM",         msgp_rtcm_preamble },
    { "Unicore-Bin",  msgp_unicore_bin_preamble },
    { "Unicore-Hash", msgp_unicore_hash_preamble }
};
static const uint16_t testParserCount = sizeof(testParsers) / sizeof(testParsers[0]);


// --- 测试数据 ---
static const uint8_t valid_nmea[] = "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\r\n";
static const uint8_t valid_ublox[] = { 0xB5, 0x62, 0x01, 0x07, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0xD2 }; // NAV-PVT with length 4
static const uint8_t valid_bt[] = { 0xAA, 0x44, 0x18, 0x14, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xDE, 0xAD, 0xBE, 0xEF, 0x50, 0x8C, 0x7E, 0x51 };

static const uint8_t invalid_nmea[] = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*FF\r\n"; // Bad CRC
static const uint8_t noise[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x12, 0x34, 0x56, 0x78 };


// --- 全局测试计数器 ---
static int successCount = 0;
static int failureCount = 0;

// --- 回调函数实现 ---

void onMessageParsed(MP_PARSE_STATE *parse, uint16_t protocolIndex) {
    printf("\033[0;32m  [ ✓ ] SUCCESS:\033[0m 协议 '%s' (索引 %u), 长度 %d\n", msgp_getProtocolName(parse, protocolIndex), protocolIndex, parse->length);
    successCount++;

    // 使用 switch 语句处理不同协议的解析结果
    switch (protocolIndex) {
        case PROTOCOL_BT:
            printf("      └─ BT/SEMP Info: Message ID = 0x%04X\n", msgp_bt_getMessageId(parse));
            break;
        case PROTOCOL_NMEA:
            printf("      └─ NMEA Info: Sentence Name = %s\n", msgp_nmea_getSentenceName(parse));
            break;
        case PROTOCOL_UBLOX:
            printf("      └─ u-blox Info: Message Number (Class|ID) = 0x%04X\n", msgp_ublox_getMessageNumber(parse));
            break;
        // 在此可为其他协议添加处理逻辑
        default:
            printf("      └─ 未知或未处理的协议索引: %d\n", protocolIndex);
            break;
    }
}

bool onCrcError(MP_PARSE_STATE *parse) {
    printf("\033[0;31m  [ ✗ ] FAILURE:\033[0m 协议 '%s' CRC 错误, 丢弃 %d 字节\n", msgp_getProtocolName(parse, parse->protocolIndex), parse->length);
    failureCount++;
    return true; // 指示解析器重置并继续寻找下一个消息
}

// --- 测试执行框架 ---
void run_test_stream(MP_PARSE_STATE *parser, const char* testName, const std::vector<uint8_t>& stream) {
    printf("\n--- Running Test: %s ---\n", testName);
    for (uint8_t byte : stream) {
        msgp_processByte(parser, byte);
    }
}


// --- Main ---
int main() {
    printf("╔═════════════════════════════════════════════════╗\n");
    printf("║          消息解析器库 - 综合测试套件            ║\n");
    printf("╚═════════════════════════════════════════════════╝\n");

    static MP_PARSE_STATE parser;
    static uint8_t buffer[2048];
    if (!msgp_init(&parser, buffer, sizeof(buffer), testParsers, testParserCount, onMessageParsed, onCrcError, "Comprehensive-Tester", NULL, NULL)) {
        fprintf(stderr, "解析器初始化失败!\n");
        return 1;
    }

    // === 测试 1: 背靠背有效数据 ===
    std::vector<uint8_t> back_to_back_stream;
    back_to_back_stream.insert(back_to_back_stream.end(), valid_nmea, valid_nmea + sizeof(valid_nmea) - 1);
    back_to_back_stream.insert(back_to_back_stream.end(), valid_ublox, valid_ublox + sizeof(valid_ublox));
    back_to_back_stream.insert(back_to_back_stream.end(), valid_bt, valid_bt + sizeof(valid_bt));
    run_test_stream(&parser, "Back-to-Back Valid Messages", back_to_back_stream);

    // === 测试 2: 错误与噪声混合 ===
    std::vector<uint8_t> mixed_stream;
    mixed_stream.insert(mixed_stream.end(), valid_nmea, valid_nmea + sizeof(valid_nmea) - 1);
    mixed_stream.insert(mixed_stream.end(), noise, noise + sizeof(noise));
    mixed_stream.insert(mixed_stream.end(), invalid_nmea, invalid_nmea + sizeof(invalid_nmea) - 1);
    mixed_stream.insert(mixed_stream.end(), valid_bt, valid_bt + sizeof(valid_bt));
    run_test_stream(&parser, "Mixed Errors, Noise, and Fragments", mixed_stream);

    // === 测试 3: 单一协议大数据块 ===
    std::vector<uint8_t> bulk_stream;
    for(int i = 0; i < 50; i++) {
        bulk_stream.insert(bulk_stream.end(), valid_nmea, valid_nmea + sizeof(valid_nmea) - 1);
    }
    run_test_stream(&parser, "High-Volume NMEA Stream (50 messages)", bulk_stream);
    
    // --- 最终报告 ---
    printf("\n\n╔═════════════════════════════════════════════════╗\n");
    printf("║                  测试结果总结                   ║\n");
    printf("╚═════════════════════════════════════════════════╝\n");
    printf("  ▶ 成功解析的消息: %d\n", successCount);
    printf("  ▶ 检测到CRC错误: %d\n", failureCount);
    printf("\n✅ 综合测试完成。\n");

    return 0;
}
