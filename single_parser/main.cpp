/**
 * @file main.cpp
 * @brief SEMP协议解析库 - 简单演示程序
 * @author SEMP Team
 * @date 2024
 */

#include "semp_parser.h"
#include <stdio.h>

// 简单的消息处理回调
void simpleMessageCallback(SEMP_PARSE_STATE *parse, uint8_t messageType)
{
    SEMP_MESSAGE_HEADER *header = (SEMP_MESSAGE_HEADER *)parse->buffer;
    
    printf("✅ 消息解析成功!\n");
    printf("   消息ID: 0x%04X\n", header->messageId);
    printf("   消息类型: 0x%02X\n", messageType);
    printf("   数据长度: %d字节\n", header->messageLength);
    printf("   总长度: %d字节\n", parse->length);
    printf("   CRC校验: 通过\n\n");
}

// CRC错误处理回调
bool simpleCrcErrorCallback(SEMP_PARSE_STATE *parse)
{
    (void)parse; // 消除未使用参数警告
    printf("❌ CRC校验失败，消息被丢弃\n\n");
    return false;
}

int main(void)
{
    printf("=== SEMP协议解析库演示程序 ===\n\n");
    
    // 创建解析器和缓冲区
    static uint8_t messageBuffer[SEMP_MAX_BUFFER_SIZE];
    static SEMP_PARSE_STATE parser;
    
    // 初始化解析器
    sempInitParser(&parser, 
                   messageBuffer, 
                   sizeof(messageBuffer),
                   simpleMessageCallback,
                   simpleCrcErrorCallback,
                   "演示解析器");
    
    printf("📡 解析器初始化完成\n\n");
    
    // 测试数据包（用户提供的数据）
    uint8_t testPacket[] = {
        0xAA, 0x44, 0x18, 0x14, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x01, 0x00, 0xC4, 0xD8, 0xDB, 0x6B
    };
    
    printf("🔍 处理测试数据包 (%zu字节)...\n", sizeof(testPacket));
    
    // 模拟逐字节接收（类似UART中断）
    for (size_t i = 0; i < sizeof(testPacket); i++) {
        sempProcessByte(&parser, testPacket[i]);
    }
    
    printf("✨ 演示完成！\n\n");
    
    printf("📚 更多功能请查看:\n");
    printf("   - test_tool: 详细数据包分析工具\n");
    printf("   - semp_example.c: 完整使用示例\n");
    printf("   - README.md: 完整文档\n");
    
    return 0;
}
