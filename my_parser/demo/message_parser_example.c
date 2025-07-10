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
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>

//----------------------------------------
// 应用配置
//----------------------------------------

#define APP_BUFFER_SIZE         512
#define MAX_MESSAGE_QUEUE       10
#define UART_SIMULATE_INTERVAL  100  // 毫秒

//----------------------------------------
// 消息队列结构
//----------------------------------------

typedef struct {
    MP_PROTOCOL_TYPE protocol;
    uint8_t data[APP_BUFFER_SIZE];
    uint16_t length;
    time_t timestamp;
} MESSAGE_ENTRY;

static MESSAGE_ENTRY messageQueue[MAX_MESSAGE_QUEUE];
static int messageCount = 0;

//----------------------------------------
// 模拟混合数据流
//----------------------------------------

// 模拟的混合数据流（多协议交错）
static const uint8_t mixed_data_stream[] = {
    // NMEA语句
    '$', 'G', 'P', 'G', 'G', 'A', ',', '1', '2', '3', '5', '1', '9', ',',
    '4', '8', '0', '7', '.', '0', '3', '8', ',', 'N', ',', '0', '1', '1',
    '3', '1', '.', '0', '0', '0', ',', 'E', ',', '1', ',', '0', '8', ',',
    '0', '.', '9', ',', '5', '4', '5', '.', '4', ',', 'M', ',', '4', '6',
    '.', '9', ',', 'M', ',', ',', '*', '4', '7', '\r', '\n',
    
    // SEMP协议数据
    0xAA, 0x44, 0x18, 0x14, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0xC4, 0xD8, 0xDB, 0x6B,
    
    // 另一个NMEA语句
    '$', 'G', 'P', 'R', 'M', 'C', ',', '1', '2', '3', '5', '1', '9', ',',
    'A', ',', '4', '8', '0', '7', '.', '0', '3', '8', ',', 'N', ',', '0',
    '1', '1', '3', '1', '.', '0', '0', '0', ',', 'E', ',', '0', '2', '2',
    '.', '4', ',', '0', '8', '4', '.', '4', ',', '2', '3', '0', '3', '9',
    '4', ',', '0', '0', '3', '.', '1', ',', 'W', '*', '6', 'A', '\r', '\n',
    
    // u-blox消息（简化）
    0xB5, 0x62, 0x01, 0x07, 0x04, 0x00, 0x00, 0x01, 0x02, 0x03, 0x14, 0x24,
    
    // 中海达Hash命令
    '#', 'V', 'E', 'R', 'S', 'I', 'O', 'N', 'A', ',', 'C', 'O', 'M', '1',
    ',', '0', ',', '5', '5', '.', '0', ',', 'F', 'I', 'N', 'E', 'S', 'T',
    'E', 'E', 'R', 'I', 'N', 'G', '*', '3', 'B', '\r', '\n'
};

//----------------------------------------
// 回调函数实现
//----------------------------------------

static void onMessageReceived(MP_PARSE_STATE *parse, MP_PROTOCOL_TYPE protocolType)
{
    if (messageCount >= MAX_MESSAGE_QUEUE) {
        printf("⚠️  消息队列已满，丢弃最早的消息\n");
        // 移动队列，删除最早的消息
        for (int i = 0; i < MAX_MESSAGE_QUEUE - 1; i++) {
            messageQueue[i] = messageQueue[i + 1];
        }
        messageCount--;
    }
    
    // 添加新消息到队列
    MESSAGE_ENTRY *entry = &messageQueue[messageCount++];
    entry->protocol = protocolType;
    entry->length = parse->length;
    entry->timestamp = time(NULL);
    memcpy(entry->data, parse->buffer, parse->length);
    
    // 输出接收信息
    printf("📨 接收到 %s 消息 (%d字节) [队列: %d/%d]\n",
           mpGetProtocolName(protocolType), parse->length, messageCount, MAX_MESSAGE_QUEUE);
}

static bool onCrcError(MP_PARSE_STATE *parse)
{
    printf("❌ CRC错误 - %s协议\n", mpGetProtocolName(parse->type));
    return false; // 显示错误信息
}

static void debugCallback(const char *format, ...)
{
    // 在实际应用中，可以选择性地启用调试输出
    // printf("DEBUG: ");
    // va_list args;
    // va_start(args, format);
    // vprintf(format, args);
    // va_end(args);
    // printf("\n");
    (void)format; // 避免未使用参数警告
}

static void errorCallback(const char *format, ...)
{
    printf("ERROR: ");
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

//----------------------------------------
// 消息处理函数
//----------------------------------------

static void processSemp(const MESSAGE_ENTRY *msg)
{
    MP_BT_HEADER *header;
    if (mpBtGetHeaderInfo(msg->data, msg->length, &header)) {
        printf("  • SEMP消息ID: 0x%04X, 类型: 0x%02X\n", 
               header->messageId, header->messageType);
        printf("  • 数据长度: %d字节\n", header->messageLength);
        
        // 获取数据部分
        const uint8_t *dataPtr;
        uint16_t dataLength;
        if (mpBtGetMessageData(msg->data, msg->length, &dataPtr, &dataLength)) {
            printf("  • 数据内容: ");
            for (int i = 0; i < dataLength && i < 8; i++) {
                printf("0x%02X ", dataPtr[i]);
            }
            if (dataLength > 8) printf("...");
            printf("\n");
        }
    }
}

static void processNmea(const MESSAGE_ENTRY *msg)
{
    char sentence[256];
    int len = msg->length < sizeof(sentence) - 1 ? msg->length : sizeof(sentence) - 1;
    memcpy(sentence, msg->data, len);
    sentence[len] = '\0';
    
    // 提取语句名称
    char sentenceName[16] = {0};
    if (sentence[0] == '$') {
        int commaPos = 0;
        for (int i = 1; i < len && commaPos == 0; i++) {
            if (sentence[i] == ',') commaPos = i;
        }
        if (commaPos > 1 && commaPos < 15) {
            memcpy(sentenceName, sentence + 1, commaPos - 1);
            sentenceName[commaPos - 1] = '\0';
        }
    }
    
    printf("  • NMEA语句: %s\n", sentenceName);
    printf("  • 类型: %s\n", mpNmeaGetSentenceType(sentenceName));
    
    // 验证格式
    if (mpNmeaValidateSentence(sentence)) {
        printf("  • 校验和: 正确 ✓\n");
        
        // 解析字段（前5个）
        char fields[10][32];
        int fieldCount = mpNmeaParseFields(sentence, fields, 10);
        printf("  • 字段数: %d", fieldCount);
        if (fieldCount > 0) {
            printf(" (前3个: ");
            for (int i = 0; i < fieldCount && i < 3; i++) {
                printf("'%s'%s", fields[i], i < 2 && i < fieldCount - 1 ? ", " : "");
            }
            printf(")");
        }
        printf("\n");
    } else {
        printf("  • 校验和: 错误 ✗\n");
    }
}

static void processUblox(const MESSAGE_ENTRY *msg)
{
    if (msg->length >= 6) {
        uint8_t messageClass = msg->data[2];
        uint8_t messageId = msg->data[3];
        uint16_t length = msg->data[4] | (msg->data[5] << 8);
        
        printf("  • u-blox消息: %s\n", mpUbloxGetMessageName(messageClass, messageId));
        printf("  • 类别: 0x%02X, ID: 0x%02X\n", messageClass, messageId);
        printf("  • 负载长度: %d字节\n", length);
        
        // 验证完整性
        if (mpUbloxVerifyMessage(msg->data, msg->length)) {
            printf("  • 校验和: 正确 ✓\n");
        } else {
            printf("  • 校验和: 错误 ✗\n");
        }
    }
}

static void processRtcm(const MESSAGE_ENTRY *msg)
{
    MP_RTCM_HEADER *header;
    if (mpRtcmGetHeaderInfo(msg->data, msg->length, &header)) {
        printf("  • RTCM消息: %s\n", mpRtcmGetMessageName(header->messageNumber));
        printf("  • 消息编号: %d\n", header->messageNumber);
        printf("  • 消息长度: %d字节\n", header->messageLength);
        
        // 验证完整性
        if (mpRtcmVerifyMessage(msg->data, msg->length)) {
            printf("  • CRC24: 正确 ✓\n");
        } else {
            printf("  • CRC24: 错误 ✗\n");
        }
    }
}

static void processUnicoreHash(const MESSAGE_ENTRY *msg)
{
    char command[512];
    int len = msg->length < sizeof(command) - 1 ? msg->length : sizeof(command) - 1;
    memcpy(command, msg->data, len);
    command[len] = '\0';
    
    char commandName[32];
    if (mpUnicoreHashGetCommandName(msg->data, msg->length, commandName, sizeof(commandName))) {
        printf("  • Unicore命令: %s\n", commandName);
        printf("  • 类型: %s\n", mpUnicoreHashGetCommandType(commandName));
        
        // 验证格式
        if (mpUnicoreHashValidateCommand(command)) {
            printf("  • 校验和: 正确 ✓\n");
            
            // 解析字段
            char fields[20][64];
            int fieldCount = mpUnicoreHashParseFields(command, fields, 20);
            printf("  • 字段数: %d", fieldCount);
            if (fieldCount > 0) {
                printf(" (前3个: ");
                for (int i = 0; i < fieldCount && i < 3; i++) {
                    printf("'%s'%s", fields[i], i < 2 && i < fieldCount - 1 ? ", " : "");
                }
                printf(")");
            }
            printf("\n");
        } else {
            printf("  • 校验和: 错误 ✗\n");
        }
    }
}

static void processMessageQueue(void)
{
    if (messageCount == 0) {
        printf("📭 消息队列为空\n");
        return;
    }
    
    printf("\n📋 处理消息队列 (%d条消息):\n", messageCount);
    printf("═══════════════════════════════════════════════════════\n");
    
    for (int i = 0; i < messageCount; i++) {
        const MESSAGE_ENTRY *msg = &messageQueue[i];
        struct tm *timeinfo = localtime(&msg->timestamp);
        
        printf("\n%d. %s [%02d:%02d:%02d]\n",
               i + 1, mpGetProtocolName(msg->protocol),
               timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        
        switch (msg->protocol) {
            case MP_PROTOCOL_BT:
                processSemp(msg);
                break;
            case MP_PROTOCOL_NMEA:
                processNmea(msg);
                break;
            case MP_PROTOCOL_UBLOX:
                processUblox(msg);
                break;
            case MP_PROTOCOL_RTCM:
                processRtcm(msg);
                break;
            case MP_PROTOCOL_UNICORE_HASH:
                processUnicoreHash(msg);
                break;
            default:
                printf("  • 未知协议类型\n");
                break;
        }
    }
    
    printf("\n═══════════════════════════════════════════════════════\n");
}

//----------------------------------------
// UART数据流模拟
//----------------------------------------

static void simulateUartDataStream(MP_PARSE_STATE *parser)
{
    printf("\n🔄 模拟UART数据流接收...\n");
    printf("─────────────────────────────────────────\n");
    
    // 模拟字节级数据接收
    for (size_t i = 0; i < sizeof(mixed_data_stream); i++) {
        uint8_t byte = mixed_data_stream[i];
        
        // 模拟UART中断处理
        mpProcessByte(parser, byte);
        
        // 模拟处理延迟（可选）
        // usleep(1000); // 1ms delay
    }
    
    printf("✅ UART数据流处理完成\n");
}

//----------------------------------------
// DMA批量处理模拟
//----------------------------------------

static void simulateDmaProcessing(MP_PARSE_STATE *parser)
{
    printf("\n📦 模拟DMA批量数据处理...\n");
    printf("─────────────────────────────────────────\n");
    
    // 清空消息队列
    messageCount = 0;
    
    // 批量处理整个数据流
    uint16_t processed = mpProcessBuffer(parser, (uint8_t *)mixed_data_stream, 
                                        sizeof(mixed_data_stream));
    
    printf("✅ DMA处理完成: %d/%zu 字节\n", processed, sizeof(mixed_data_stream));
}

//----------------------------------------
// 统计信息分析
//----------------------------------------

static void analyzeStatistics(const MP_PARSE_STATE *parser)
{
    printf("\n📊 详细统计分析:\n");
    printf("═══════════════════════════════════════════════════════\n");
    
    MP_PROTOCOL_STATS stats[MP_PROTOCOL_COUNT];
    uint16_t statCount = mpGetProtocolStats(parser, stats, MP_PROTOCOL_COUNT);
    
    uint32_t totalMessages = 0;
    uint32_t totalErrors = 0;
    
    for (int i = 0; i < statCount; i++) {
        totalMessages += stats[i].messagesProcessed;
        totalErrors += stats[i].crcErrors;
    }
    
    printf("总体统计:\n");
    printf("  • 总消息数: %u\n", totalMessages);
    printf("  • 总错误数: %u\n", totalErrors);
    printf("  • 总成功率: %.1f%%\n", 
           totalMessages + totalErrors > 0 ? 
           (float)totalMessages / (totalMessages + totalErrors) * 100.0f : 0.0f);
    printf("  • 协议切换: %u次\n", parser->protocolSwitches);
    printf("  • 处理字节: %u字节\n", parser->totalBytes);
    
    if (statCount > 0) {
        printf("\n各协议详情:\n");
        for (int i = 0; i < statCount; i++) {
            const char *activeMarker = stats[i].isActive ? " [当前活动]" : "";
            printf("  %s%s:\n", stats[i].protocolName, activeMarker);
            printf("    - 成功: %u条消息\n", stats[i].messagesProcessed);
            printf("    - 错误: %u条消息\n", stats[i].crcErrors);
            printf("    - 成功率: %.1f%%\n", stats[i].successRate);
            
            // 计算协议占比
            if (totalMessages > 0) {
                float percentage = (float)stats[i].messagesProcessed / totalMessages * 100.0f;
                printf("    - 占比: %.1f%%\n", percentage);
            }
        }
    }
    
    printf("═══════════════════════════════════════════════════════\n");
}

//----------------------------------------
// 主程序
//----------------------------------------

int main(void)
{
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║                  模块化消息解析器应用示例                    ║\n");
    printf("║               嵌入式多协议数据流处理演示                     ║\n");
    printf("║                     版本 %s                            ║\n", mpGetVersion());
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    // 初始化解析器
    MP_PARSE_STATE parser;
    uint8_t buffer[APP_BUFFER_SIZE];
    
    if (!mpInitParser(&parser, buffer, sizeof(buffer),
                      onMessageReceived, onCrcError, "应用解析器",
                      errorCallback, debugCallback)) {
        printf("❌ 解析器初始化失败！\n");
        return -1;
    }
    
    printf("\n✅ 解析器初始化成功\n");
    printf("📊 缓冲区大小: %d字节\n", APP_BUFFER_SIZE);
    printf("📊 消息队列容量: %d条\n", MAX_MESSAGE_QUEUE);
    
    // 场景1：UART数据流模拟
    printf("\n" "▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓" "\n");
    printf("📡 场景1: UART字节流处理\n");
    printf("▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓" "\n");
    
    simulateUartDataStream(&parser);
    processMessageQueue();
    
    // 重置统计信息准备下一个场景
    mpResetStats(&parser);
    messageCount = 0;
    
    // 场景2：DMA批量处理模拟
    printf("\n\n" "▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓" "\n");
    printf("📦 场景2: DMA批量数据处理\n");
    printf("▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓" "\n");
    
    simulateDmaProcessing(&parser);
    processMessageQueue();
    
    // 最终统计分析
    printf("\n\n" "▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓" "\n");
    printf("📈 场景3: 统计分析和性能评估\n");
    printf("▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓" "\n");
    
    analyzeStatistics(&parser);
    
    // 显示应用建议
    printf("\n💡 实际应用建议:\n");
    printf("─────────────────────────────────────────\n");
    printf("1. UART中断处理: 使用 mpProcessByte() 逐字节处理\n");
    printf("2. DMA批量处理: 使用 mpProcessBuffer() 批量处理\n");
    printf("3. 错误处理: 实现 badCrcCallback 处理校验错误\n");
    printf("4. 内存管理: 根据应用调整缓冲区大小\n");
    printf("5. 性能优化: 监控统计信息优化处理逻辑\n");
    printf("6. 协议扩展: 参考现有协议解析器添加新协议\n");
    
    printf("\n🔧 配置参数:\n");
    printf("─────────────────────────────────────────\n");
    printf("• 缓冲区大小: %d字节 (根据最大消息长度调整)\n", APP_BUFFER_SIZE);
    printf("• 消息队列: %d条 (根据处理速度调整)\n", MAX_MESSAGE_QUEUE);
    printf("• 内存使用: 约%zu字节 (解析器状态 + 缓冲区)\n", 
           sizeof(MP_PARSE_STATE) + APP_BUFFER_SIZE);
    
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║                        应用示例结束                          ║\n");
    printf("║                                                            ║\n");
    printf("║  📚 更多信息:                                               ║\n");
    printf("║  • 查看 'MULTI_PROTOCOL_README.md' 获取完整文档              ║\n");
    printf("║  • 查看 'PROJECT_STRUCTURE.md' 了解项目结构                 ║\n");
    printf("║  • 运行 'make help' 查看编译选项                           ║\n");
    printf("║  • 查看各协议解析器源码了解实现细节                           ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    return 0;
} 