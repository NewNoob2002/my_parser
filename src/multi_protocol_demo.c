/**
 * @file multi_protocol_demo.c
 * @brief 多协议解析器演示程序
 * @details 展示如何使用多协议解析器同时处理多种通信协议
 */

#include "multi_protocol_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

// 函数前向声明
void handleSempMessage(MMP_PARSE_STATE *parse);
void handleNmeaMessage(MMP_PARSE_STATE *parse);
void handleUbloxMessage(MMP_PARSE_STATE *parse);

//----------------------------------------
// 演示回调函数
//----------------------------------------

// 消息处理回调
void onMultiProtocolMessage(MMP_PARSE_STATE *parse, uint16_t protocolType)
{
    const char *protocolName = mmpGetProtocolName((MMP_PROTOCOL_TYPE)protocolType);
    
    printf("🎯 检测到 %s 协议消息!\n", protocolName);
    printf("   消息长度: %d字节\n", parse->length);
    printf("   缓冲区前16字节: ");
    
    int displayLen = parse->length > 16 ? 16 : parse->length;
    for (int i = 0; i < displayLen; i++) {
        printf("%02X ", parse->buffer[i]);
    }
    if (parse->length > 16) printf("...");
    printf("\n");
    
    // 根据协议类型进行特定处理
    switch (protocolType) {
        case MMP_PROTOCOL_SEMP:
            handleSempMessage(parse);
            break;
        case MMP_PROTOCOL_NMEA:
            handleNmeaMessage(parse);
            break;
        case MMP_PROTOCOL_UBLOX:
            handleUbloxMessage(parse);
            break;
        default:
            printf("   协议处理函数待实现\n");
            break;
    }
    printf("\n");
}

// SEMP协议特定处理
void handleSempMessage(MMP_PARSE_STATE *parse)
{
    if (parse->length >= sizeof(MMP_SEMP_HEADER)) {
        MMP_SEMP_HEADER *header = (MMP_SEMP_HEADER *)parse->buffer;
        printf("   📋 SEMP详细信息:\n");
        printf("      消息ID: 0x%04X\n", header->messageId);
        printf("      消息类型: 0x%02X\n", header->messageType);
        printf("      发送者: 0x%02X\n", header->sender);
        printf("      协议版本: 0x%02X\n", header->Protocol);
        printf("      数据长度: %d字节\n", header->messageLength);
    }
}

// NMEA协议特定处理
void handleNmeaMessage(MMP_PARSE_STATE *parse)
{
    printf("   📋 NMEA详细信息:\n");
    printf("      语句内容: %.*s\n", parse->length - 2, parse->buffer); // 去掉\r\n
    
    // 解析NMEA语句类型
    if (parse->length > 6) {
        char sentenceType[7] = {0};
        memcpy(sentenceType, parse->buffer + 1, 6); // 跳过$
        printf("      语句类型: %s\n", sentenceType);
        
        if (strncmp(sentenceType, "GPGGA", 5) == 0) {
            printf("      类型: GPS固定数据\n");
        } else if (strncmp(sentenceType, "GPRMC", 5) == 0) {
            printf("      类型: 推荐最小定位信息\n");
        } else if (strncmp(sentenceType, "GPGSV", 5) == 0) {
            printf("      类型: 可见卫星信息\n");
        }
    }
}

// u-blox协议特定处理
void handleUbloxMessage(MMP_PARSE_STATE *parse)
{
    if (parse->length >= sizeof(MMP_UBLOX_HEADER)) {
        MMP_UBLOX_HEADER *header = (MMP_UBLOX_HEADER *)parse->buffer;
        printf("   📋 u-blox详细信息:\n");
        printf("      消息类别: 0x%02X\n", header->messageClass);
        printf("      消息ID: 0x%02X\n", header->messageId);
        printf("      数据长度: %d字节\n", header->length);
        
        // 识别常见的u-blox消息类型
        if (header->messageClass == 0x01) {
            printf("      类型: NAV (导航) 消息\n");
            if (header->messageId == 0x07) {
                printf("      具体: NAV-PVT (位置速度时间)\n");
            }
        } else if (header->messageClass == 0x02) {
            printf("      类型: RXM (接收器管理) 消息\n");
        }
    }
}

// CRC错误回调
bool onMultiProtocolCrcError(MMP_PARSE_STATE *parse)
{
    (void)parse; // 避免未使用参数警告
    printf("❌ 检测到CRC错误，消息被丢弃\n");
    return false; // 不处理错误消息
}

// 调试输出回调
void debugOutput(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    printf("🔧 DEBUG: ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

// 错误输出回调
void errorOutput(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    printf("❌ ERROR: ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

//----------------------------------------
// 测试数据
//----------------------------------------

// SEMP协议测试数据
uint8_t sempTestData[] = {
    0xAA, 0x44, 0x18, 0x14, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0xC4, 0xD8, 0xDB, 0x6B
};

// NMEA协议测试数据
uint8_t nmeaTestData[] = 
    "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";

// u-blox协议测试数据 (NAV-PVT消息示例)
uint8_t ubloxTestData[] = {
    0xB5, 0x62, 0x01, 0x07, 0x5C, 0x00, // 头部：sync1, sync2, class, id, length(92字节)
    // 简化的92字节数据
    0x10, 0x27, 0x00, 0x00, 0xE7, 0x07, 0x0B, 0x1D, 0x0E, 0x23, 0x2E, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x39, 0x00, 0x00, 0x00, 0x03, 0x03, 0x00, 0x00,
    0x72, 0x29, 0x31, 0x17, 0x18, 0x4F, 0xC2, 0x0A, 0x69, 0xC7, 0x36, 0x02,
    0x5E, 0x8F, 0x00, 0x00, 0x2D, 0x13, 0x00, 0x00, 0xC6, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x87, 0x01, 0x00, 0x00, 0x93, 0x00, 0x00, 0x00, 0x3B, 0x01, 0x00, 0x00,
    0x18, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x9A, 0x0D // 校验和 (示例值)
};

// 混合协议测试数据（模拟真实环境）
typedef struct {
    uint8_t *data;
    size_t length;
    const char *description;
} TestPacket;

TestPacket mixedTestData[] = {
    {sempTestData, sizeof(sempTestData), "SEMP协议数据包"},
    {nmeaTestData, sizeof(nmeaTestData) - 1, "NMEA GPS语句"}, // -1去掉字符串终止符
    {ubloxTestData, sizeof(ubloxTestData), "u-blox导航消息"},
    // 可以添加更多测试数据...
};

//----------------------------------------
// 测试函数
//----------------------------------------

void testSingleProtocol(MMP_PARSE_STATE *parse, TestPacket *packet)
{
    printf("📦 测试数据包: %s\n", packet->description);
    printf("   数据长度: %zu字节\n", packet->length);
    printf("   原始数据: ");
    
    int displayLen = packet->length > 20 ? 20 : packet->length;
    for (int i = 0; i < displayLen; i++) {
        if (packet->data[i] >= 32 && packet->data[i] <= 126) {
            printf("%c", packet->data[i]);
        } else {
            printf(".");
        }
    }
    if (packet->length > 20) printf("...");
    printf("\n");
    
    printf("   十六进制: ");
    for (int i = 0; i < displayLen; i++) {
        printf("%02X ", packet->data[i]);
    }
    if (packet->length > 20) printf("...");
    printf("\n\n");
    
    // 逐字节处理
    for (size_t i = 0; i < packet->length; i++) {
        mmpProcessByte(parse, packet->data[i]);
    }
}

void testMixedProtocols(MMP_PARSE_STATE *parse)
{
    printf("🔀 混合协议测试 - 模拟真实通信环境\n");
    printf("===========================================\n\n");
    
    // 测试各种协议
    int packetCount = sizeof(mixedTestData) / sizeof(mixedTestData[0]);
    for (int i = 0; i < packetCount; i++) {
        testSingleProtocol(parse, &mixedTestData[i]);
        printf("-------------------------------------------\n\n");
    }
}

void testStreamData(MMP_PARSE_STATE *parse)
{
    printf("🌊 流式数据测试 - 连续数据流解析\n");
    printf("===========================================\n\n");
    
    // 创建一个包含多种协议的连续数据流
    uint8_t streamData[512];
    size_t offset = 0;
    
    // 添加SEMP数据
    memcpy(streamData + offset, sempTestData, sizeof(sempTestData));
    offset += sizeof(sempTestData);
    
    // 添加一些干扰字节
    streamData[offset++] = 0xFF;
    streamData[offset++] = 0x00;
    streamData[offset++] = 0x55;
    
    // 添加NMEA数据
    memcpy(streamData + offset, nmeaTestData, sizeof(nmeaTestData) - 1);
    offset += sizeof(nmeaTestData) - 1;
    
    // 添加更多干扰字节
    streamData[offset++] = 0xAB;
    streamData[offset++] = 0xCD;
    
    // 添加u-blox数据
    memcpy(streamData + offset, ubloxTestData, sizeof(ubloxTestData));
    offset += sizeof(ubloxTestData);
    
    printf("📊 流数据总长度: %zu字节\n", offset);
    printf("🔍 开始解析流数据...\n\n");
    
    // 逐字节处理整个流
    uint16_t processed = mmpProcessBuffer(parse, streamData, offset);
    
    printf("✅ 处理完成，共处理 %d 字节\n\n", processed);
}

//----------------------------------------
// 主程序
//----------------------------------------

int main(void)
{
    printf("🚀 多协议解析器演示程序\n");
    printf("==================================================\n\n");
    
    // 创建解析器和缓冲区
    static uint8_t messageBuffer[2048];
    static MMP_PARSE_STATE parser;
    
    // 初始化多协议解析器
    if (!mmpInitParser(&parser,
                       messageBuffer, sizeof(messageBuffer),
                       onMultiProtocolMessage,
                       onMultiProtocolCrcError,
                       "演示解析器",
                       errorOutput,
                       NULL)) { // 关闭调试输出以保持演示清晰
        printf("❌ 解析器初始化失败！\n");
        return -1;
    }
    
    printf("✅ 多协议解析器初始化成功\n");
    printf("📋 支持的协议:\n");
    for (int i = 0; i < MMP_PROTOCOL_COUNT; i++) {
        printf("   • %s\n", mmpGetProtocolName((MMP_PROTOCOL_TYPE)i));
    }
    printf("\n");
    
    // 测试各种协议
    testMixedProtocols(&parser);
    
    // 测试流式数据
    testStreamData(&parser);
    
    // 显示统计信息
    printf("📊 解析统计信息\n");
    printf("==================================================\n");
    mmpPrintStats(&parser);
    
    printf("\n🎉 演示完成！\n");
    printf("💡 提示: 在实际应用中，您可以:\n");
    printf("   • 在UART中断中调用 mmpProcessByte()\n");
    printf("   • 在DMA完成中断中调用 mmpProcessBuffer()\n");
    printf("   • 根据协议类型进行相应的业务处理\n");
    printf("   • 监控统计信息进行性能优化\n");
    
    return 0;
} 