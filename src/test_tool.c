#include "semp_parser.h"
#include <stdio.h>
#include <string.h>

// 详细的消息解析回调
void detailedEomCallback(SEMP_PARSE_STATE *parse, uint8_t messageType)
{
    printf("=== 数据包解析成功 ===\n");
    printf("总长度: %d字节\n", parse->length);
    
    SEMP_MESSAGE_HEADER *header = (SEMP_MESSAGE_HEADER *)parse->buffer;
    
    // 头部信息
    printf("\n--- 协议头部 (20字节) ---\n");
    printf("同步字节: 0x%02X 0x%02X 0x%02X %s\n", 
           header->syncA, header->syncB, header->syncC,
           (header->syncA == 0xAA && header->syncB == 0x44 && header->syncC == 0x18) ? "✓" : "✗");
    printf("头部长度: 0x%02X (%d字节) %s\n", 
           header->headerLength, header->headerLength,
           header->headerLength == 0x14 ? "✓" : "✗");
    printf("消息ID: 0x%04X (%d)\n", header->messageId, header->messageId);
    printf("保留字段1: 0x%04X\n", header->RESERVED1);
    printf("保留时间戳: 0x%08X (%u)\n", header->RESERVED_time, header->RESERVED_time);
    printf("消息长度: %d字节\n", header->messageLength);
    printf("保留字段2: 0x%04X\n", header->RESERVED2);
    printf("发送者: 0x%02X (%d)\n", header->sender, header->sender);
    printf("消息类型: 0x%02X (%d)\n", header->messageType, header->messageType);
    printf("协议版本: 0x%02X (%d)\n", header->Protocol, header->Protocol);
    printf("消息间隔: %d\n", header->MsgInterval);
    
    // 数据部分
    if (header->messageLength > 0) {
        printf("\n--- 数据部分 (%d字节) ---\n", header->messageLength);
        uint8_t *data = parse->buffer + sizeof(SEMP_MESSAGE_HEADER);
        
        // 十六进制显示
        printf("十六进制: ");
        for (uint16_t i = 0; i < header->messageLength; i++) {
            printf("%02X ", data[i]);
            if ((i + 1) % 16 == 0) printf("\n           ");
        }
        printf("\n");
        
        // ASCII显示（如果可打印）
        printf("ASCII字符: ");
        for (uint16_t i = 0; i < header->messageLength; i++) {
            if (data[i] >= 32 && data[i] <= 126) {
                printf("%c", data[i]);
            } else {
                printf(".");
            }
        }
        printf("\n");
        
        // 不同数据类型解析
        if (header->messageLength >= 1) {
            printf("8位值: %d (0x%02X)\n", data[0], data[0]);
        }
        if (header->messageLength >= 2) {
            uint16_t val16_le = (data[1] << 8) | data[0];
            uint16_t val16_be = (data[0] << 8) | data[1];
            printf("16位值 (小端): %d (0x%04X)\n", val16_le, val16_le);
            printf("16位值 (大端): %d (0x%04X)\n", val16_be, val16_be);
        }
        if (header->messageLength >= 4) {
            uint32_t val32_le = (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];
            uint32_t val32_be = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
            printf("32位值 (小端): %u (0x%08X)\n", val32_le, val32_le);
            printf("32位值 (大端): %u (0x%08X)\n", val32_be, val32_be);
            
            // 尝试解析为浮点数
            float float_val = *(float*)data;
            printf("浮点数: %.6f\n", float_val);
        }
    } else {
        printf("\n--- 无数据部分 ---\n");
    }
    
    // CRC信息
    printf("\n--- CRC校验信息 ---\n");
    if (parse->length >= 4) {
        uint8_t *crcBytes = parse->buffer + parse->length - 4;
        printf("CRC字节: %02X %02X %02X %02X\n", 
               crcBytes[0], crcBytes[1], crcBytes[2], crcBytes[3]);
        
        uint32_t receivedCrc = (uint32_t)crcBytes[0] |
                              ((uint32_t)crcBytes[1] << 8) |
                              ((uint32_t)crcBytes[2] << 16) |
                              ((uint32_t)crcBytes[3] << 24);
        printf("CRC值: 0x%08X (%u)\n", receivedCrc, receivedCrc);
    }
    printf("CRC状态: ✅ 校验通过\n");
    
    // 原始数据包
    printf("\n--- 完整数据包 ---\n");
    for (int i = 0; i < parse->length; i++) {
        printf("%02X ", parse->buffer[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    if (parse->length % 16 != 0) printf("\n");
    
    printf("========================\n\n");
}

// CRC错误回调
bool crcErrorCallback(SEMP_PARSE_STATE *parse)
{
    printf("❌ CRC校验失败！\n");
    printf("数据包可能在传输过程中损坏\n");
    return false;
}

// 测试指定的数据包
void testDataPacket(uint8_t *data, int length, const char *description)
{
    printf("🔍 测试数据包: %s\n", description);
    printf("数据长度: %d字节\n", length);
    printf("原始数据: ");
    for (int i = 0; i < length; i++) {
        printf("%02X ", data[i]);
        if ((i + 1) % 16 == 0 && i < length - 1) printf("\n          ");
    }
    printf("\n\n");
    
    // 创建解析器
    static uint8_t messageBuffer[SEMP_MAX_BUFFER_SIZE];
    static SEMP_PARSE_STATE parser;
    
    sempInitParser(&parser, messageBuffer, sizeof(messageBuffer),
                   detailedEomCallback, crcErrorCallback, "测试解析器");
    
    // 处理数据
    sempProcessBuffer(&parser, data, length);
}

int main(void)
{
    printf("=== SEMP协议数据包测试工具 ===\n\n");
    
    // 测试用户提供的数据包
    uint8_t userData[] = {
        0xAA, 0x44, 0x18, 0x14, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x01, 0x00, 0xC4, 0xD8, 0xDB, 0x6B
    };
    
    testDataPacket(userData, sizeof(userData), "用户提供的数据包");
    
    // 可以在这里添加更多测试案例
    
    printf("测试完成！\n");
    return 0;
} 