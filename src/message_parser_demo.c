/**
 * @file message_parser_demo.c
 * @brief 模块化消息解析器演示程序
 * @details 演示如何使用新的模块化消息解析器处理多种协议
 * @version 1.0
 * @date 2024-12
 */

#include "Message_Parser.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

//----------------------------------------
// 测试数据
//----------------------------------------

// SEMP/BT协议测试数据
static const uint8_t semp_test_data[] = {
    0xAA, 0x44, 0x18, 0x14, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0xC4, 0xD8, 0xDB, 0x6B
};

// NMEA协议测试数据
static const char nmea_test_data[] = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";

// u-blox协议测试数据（简化的NAV-PVT消息）
static const uint8_t ublox_test_data[] = {
    0xB5, 0x62, 0x01, 0x07, 0x5C, 0x00,
    // 92字节的负载数据（此处简化为前几个字节）
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // ... (实际应该是92字节)
    // 校验和
    0x5A, 0x4B
};

// RTCM协议测试数据（简化版）
static const uint8_t rtcm_test_data[] = {
    0xD3, 0x00, 0x13, // 前导和长度
    0x3E, 0xD0, 0x00, 0x01, // 消息类型1005的前几个字节
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x18, 0x64, 0xCF // CRC24
};

// 中海达Hash协议测试数据
static const char unicore_hash_test_data[] = "#BESTPOSA,COM1,0,55.0,FINESTEERING,1419,340033.000,02000040,d821,15564;SOL_COMPUTED,SINGLE,51.15043711333,-114.03067143712,1063.9274,-16.2712,WGS84,1.6851,2.1068,3.8446,\"\",0.000,0.000,35,30,30,30,00,06,00,01*2A\r\n";

//----------------------------------------
// 回调函数
//----------------------------------------

static void onMessageComplete(MP_PARSE_STATE *parse, MP_PROTOCOL_TYPE protocolType)
{
    printf("\n=== 消息解析完成 ===\n");
    printf("协议类型: %s\n", mpGetProtocolName(protocolType));
    printf("协议描述: %s\n", mpGetProtocolDescription(protocolType));
    printf("消息长度: %d 字节\n", parse->length);
    
    // 显示消息的十六进制数据
    printf("消息数据: ");
    for (int i = 0; i < parse->length && i < 32; i++) {
        printf("%02X ", parse->buffer[i]);
    }
    if (parse->length > 32) {
        printf("... (显示前32字节)");
    }
    printf("\n");
    
    // 根据协议类型显示特定信息
    switch (protocolType) {
        case MP_PROTOCOL_BT: {
            MP_BT_HEADER *header;
            if (mpBtGetHeaderInfo(parse->buffer, parse->length, &header)) {
                printf("SEMP消息ID: 0x%04X, 消息类型: 0x%02X\n", 
                       header->messageId, header->messageType);
                printf("数据长度: %d 字节\n", header->messageLength);
            }
            break;
        }
        
        case MP_PROTOCOL_NMEA: {
            const char *sentenceName = mpNmeaGetSentenceName(parse);
            printf("NMEA语句: %s\n", sentenceName);
            printf("语句类型: %s\n", mpNmeaGetSentenceType(sentenceName));
            
            // 解析字段
            char fields[20][32];
            int fieldCount = mpNmeaParseFields((const char *)parse->buffer, fields, 20);
            printf("字段数量: %d\n", fieldCount);
            for (int i = 0; i < fieldCount && i < 5; i++) {
                printf("  字段%d: %s\n", i+1, fields[i]);
            }
            break;
        }
        
        case MP_PROTOCOL_UBLOX: {
            if (parse->length >= 6) {
                uint8_t messageClass = parse->buffer[2];
                uint8_t messageId = parse->buffer[3];
                printf("u-blox消息: %s\n", mpUbloxGetMessageName(messageClass, messageId));
                printf("消息类别: 0x%02X, 消息ID: 0x%02X\n", messageClass, messageId);
            }
            break;
        }
        
        case MP_PROTOCOL_RTCM: {
            MP_RTCM_HEADER *header;
            if (mpRtcmGetHeaderInfo(parse->buffer, parse->length, &header)) {
                printf("RTCM消息: %s\n", mpRtcmGetMessageName(header->messageNumber));
                printf("消息编号: %d, 消息长度: %d\n", 
                       header->messageNumber, header->messageLength);
            }
            break;
        }
        
        case MP_PROTOCOL_UNICORE_HASH: {
            char commandName[32];
            if (mpUnicoreHashGetCommandName(parse->buffer, parse->length, commandName, sizeof(commandName))) {
                printf("Unicore命令: %s\n", commandName);
                printf("命令类型: %s\n", mpUnicoreHashGetCommandType(commandName));
            }
            break;
        }
        
        default:
            printf("未知协议类型\n");
            break;
    }
    
    printf("==================\n\n");
}

static bool onCrcError(MP_PARSE_STATE *parse)
{
    printf("⚠️  CRC校验错误 - 协议: %s\n", mpGetProtocolName(parse->type));
    return false; // 返回false表示要显示错误信息
}

static void onDebugMessage(const char *format, ...)
{
    printf("🔍 ");
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

static void onErrorMessage(const char *format, ...)
{
    printf("❌ ");
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

//----------------------------------------
// 测试函数
//----------------------------------------

static void testProtocol(const char *protocolName, const uint8_t *testData, 
                        uint16_t dataLength, MP_PARSE_STATE *parser)
{
    printf("\n" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "\n");
    printf("测试协议: %s\n", protocolName);
    printf("=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "\n");
    
    printf("测试数据长度: %d 字节\n", dataLength);
    printf("测试数据: ");
    for (int i = 0; i < dataLength && i < 32; i++) {
        printf("%02X ", testData[i]);
    }
    if (dataLength > 32) {
        printf("... (显示前32字节)");
    }
    printf("\n\n");
    
    // 逐字节处理数据
    uint16_t processed = mpProcessBuffer(parser, (uint8_t *)testData, dataLength);
    
    printf("处理了 %d/%d 字节\n", processed, dataLength);
}

static void demonstrateCapabilities(void)
{
    printf("\n📋 支持的协议列表:\n");
    printf("─────────────────────────────────────────\n");
    printf("1. BT/SEMP     - 蓝牙/SEMP协议 (0xAA 0x44 0x18)\n");
    printf("2. NMEA        - NMEA GPS协议 ($)\n");
    printf("3. u-blox      - u-blox二进制协议 (0xB5 0x62)\n");
    printf("4. RTCM        - RTCM差分GPS协议 (0xD3)\n");
    printf("5. Unicore-Bin - 中海达二进制协议 (0xAA 0x44 0x12)\n");
    printf("6. Unicore-Hash- 中海达Hash协议 (#)\n");
    printf("\n");
    
    printf("🔧 主要特性:\n");
    printf("─────────────────────────────────────────\n");
    printf("• 自动协议识别\n");
    printf("• 实时数据流处理\n");
    printf("• 完整的错误检查和校验\n");
    printf("• 统计信息跟踪\n");
    printf("• 模块化架构，易于扩展\n");
    printf("• 内存高效设计\n");
    printf("• 中断友好的字节级处理\n");
    printf("\n");
}

//----------------------------------------
// 主程序
//----------------------------------------

int main(void)
{
    printf("╔════════════════════════════════════════════════════╗\n");
    printf("║              模块化消息解析器演示程序                ║\n");
    printf("║                    版本 %s                      ║\n", mpGetVersion());
    printf("╚════════════════════════════════════════════════════╝\n");
    
    demonstrateCapabilities();
    
    // 初始化解析器
    MP_PARSE_STATE parser;
    uint8_t buffer[1024];
    
    if (!mpInitParser(&parser, buffer, sizeof(buffer),
                      onMessageComplete, onCrcError, "演示解析器",
                      onErrorMessage, onDebugMessage)) {
        printf("❌ 解析器初始化失败！\n");
        return -1;
    }
    
    printf("✅ 解析器初始化成功！\n");
    
    // 显示支持的协议
    mpListSupportedProtocols(&parser);
    
    // 测试各种协议
    testProtocol("SEMP/BT协议", semp_test_data, sizeof(semp_test_data), &parser);
    testProtocol("NMEA协议", (const uint8_t *)nmea_test_data, strlen(nmea_test_data), &parser);
    testProtocol("u-blox协议", ublox_test_data, sizeof(ublox_test_data), &parser);
    testProtocol("RTCM协议", rtcm_test_data, sizeof(rtcm_test_data), &parser);
    testProtocol("Unicore Hash协议", (const uint8_t *)unicore_hash_test_data, 
                 strlen(unicore_hash_test_data), &parser);
    
    // 显示统计信息
    printf("\n" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "\n");
    printf("📊 最终统计报告\n");
    printf("=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "\n");
    mpPrintStats(&parser);
    
    // 获取详细统计信息
    MP_PROTOCOL_STATS stats[MP_PROTOCOL_COUNT];
    uint16_t statCount = mpGetProtocolStats(&parser, stats, MP_PROTOCOL_COUNT);
    
    if (statCount > 0) {
        printf("\n📈 详细协议统计:\n");
        printf("─────────────────────────────────────────\n");
        for (int i = 0; i < statCount; i++) {
            const char *activeMarker = stats[i].isActive ? " [当前活动]" : "";
            printf("%s%s:\n", stats[i].protocolName, activeMarker);
            printf("  描述: %s\n", stats[i].description);
            printf("  成功消息: %u\n", stats[i].messagesProcessed);
            printf("  CRC错误: %u\n", stats[i].crcErrors);
            printf("  成功率: %.1f%%\n", stats[i].successRate);
            printf("\n");
        }
    }
    
    printf("╔════════════════════════════════════════════════════╗\n");
    printf("║                  演示程序结束                      ║\n");
    printf("║                                                    ║\n");
    printf("║  使用说明:                                         ║\n");
    printf("║  • 运行 './bin/message_parser_example' 查看更多示例 ║\n");
    printf("║  • 查看 'MULTI_PROTOCOL_README.md' 获取详细文档     ║\n");
    printf("║  • 运行 'make help' 查看所有编译选项               ║\n");
    printf("╚════════════════════════════════════════════════════╝\n");
    
    return 0;
} 