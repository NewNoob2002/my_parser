/**
 * @file multi_protocol_example.c
 * @brief 多协议解析器实际应用示例
 * @details 展示在嵌入式系统中如何使用多协议解析器处理UART、DMA、WiFi等通信数据
 */

#include "multi_protocol_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

//----------------------------------------
// 模拟硬件接口
//----------------------------------------

// 模拟UART寄存器
typedef struct {
    volatile uint8_t DATA;
    volatile uint8_t STATUS;
    volatile uint8_t CTRL;
} UART_TypeDef;

// 模拟DMA寄存器
typedef struct {
    volatile uint8_t *SRC_ADDR;
    volatile uint8_t *DST_ADDR;
    volatile uint16_t COUNT;
    volatile uint8_t CTRL;
    volatile uint8_t STATUS;
} DMA_TypeDef;

// 模拟外设
static UART_TypeDef UART1 = {0};
static DMA_TypeDef DMA1 = {0};

//----------------------------------------
// 全局变量
//----------------------------------------

// 多协议解析器实例
static MMP_PARSE_STATE g_multiParser;
static uint8_t g_parseBuffer[1024];

// 接收缓冲区
static uint8_t g_uartRxBuffer[256];
static volatile uint16_t g_uartRxHead = 0;
static volatile uint16_t g_uartRxTail = 0;

// 解析结果缓冲区
static uint8_t g_messageBuffers[4][512];  // 4个消息缓冲区
static volatile uint8_t g_currentBuffer = 0;

// 统计信息
static uint32_t g_processedBytes = 0;
static uint32_t g_messagesReceived = 0;

//----------------------------------------
// 协议处理函数
//----------------------------------------

// SEMP协议消息处理
void processSempMessage(uint8_t *buffer, uint16_t length)
{
    printf("📡 SEMP消息处理\n");
    
    if (length >= sizeof(MMP_SEMP_HEADER)) {
        MMP_SEMP_HEADER *header = (MMP_SEMP_HEADER *)buffer;
        
        printf("   消息ID: 0x%04X\n", header->messageId);
        printf("   消息类型: 0x%02X\n", header->messageType);
        printf("   数据长度: %d字节\n", header->messageLength);
        
        // 根据消息类型进行特定处理
        switch (header->messageType) {
            case 0x01: // 心跳消息
                printf("   ❤️  心跳消息 - 设备正常运行\n");
                break;
            case 0x02: // 传感器数据
                printf("   📊 传感器数据消息\n");
                if (header->messageLength >= 2) {
                    uint16_t sensorValue = *(uint16_t*)(buffer + sizeof(MMP_SEMP_HEADER));
                    printf("      传感器值: %d\n", sensorValue);
                }
                break;
            case 0x03: // 控制命令
                printf("   🎮 控制命令消息\n");
                break;
            default:
                printf("   ❓ 未知消息类型: 0x%02X\n", header->messageType);
                break;
        }
    }
}

// NMEA协议消息处理
void processNmeaMessage(uint8_t *buffer, uint16_t length)
{
    printf("🗺️  NMEA GPS消息处理\n");
    
    // 确保字符串正确终止
    char sentence[256];
    int copyLen = length > 255 ? 255 : length;
    memcpy(sentence, buffer, copyLen);
    sentence[copyLen] = '\0';
    
    printf("   GPS语句: %s", sentence);
    
    // 简单解析NMEA语句
    if (strncmp((char*)buffer, "$GPGGA", 6) == 0) {
        printf("   🎯 GPS固定数据 - 解析位置信息\n");
        // 这里可以添加具体的GPGGA解析代码
    } else if (strncmp((char*)buffer, "$GPRMC", 6) == 0) {
        printf("   🧭 推荐最小定位信息 - 解析航向和速度\n");
        // 这里可以添加具体的GPRMC解析代码
    } else if (strncmp((char*)buffer, "$GPGSV", 6) == 0) {
        printf("   🛰️  可见卫星信息 - 解析卫星状态\n");
        // 这里可以添加具体的GPGSV解析代码
    }
}

// u-blox协议消息处理
void processUbloxMessage(uint8_t *buffer, uint16_t length)
{
    printf("📍 u-blox导航消息处理\n");
    
    if (length >= sizeof(MMP_UBLOX_HEADER)) {
        MMP_UBLOX_HEADER *header = (MMP_UBLOX_HEADER *)buffer;
        
        printf("   消息类别: 0x%02X, ID: 0x%02X\n", header->messageClass, header->messageId);
        printf("   数据长度: %d字节\n", header->length);
        
        // 解析具体的u-blox消息
        if (header->messageClass == 0x01 && header->messageId == 0x07) {
            printf("   🌍 NAV-PVT: 位置速度时间解决方案\n");
            // 可以添加PVT数据解析
        } else if (header->messageClass == 0x01 && header->messageId == 0x35) {
            printf("   📊 NAV-SAT: 卫星信息\n");
        } else if (header->messageClass == 0x05) {
            printf("   ✅ ACK: 确认消息\n");
        }
    }
}

//----------------------------------------
// 回调函数实现
//----------------------------------------

// 多协议消息结束回调
void onMessageComplete(MMP_PARSE_STATE *parse, uint16_t protocolType)
{
    g_messagesReceived++;
    
    // 复制消息到处理缓冲区
    uint8_t *targetBuffer = g_messageBuffers[g_currentBuffer];
    memcpy(targetBuffer, parse->buffer, parse->length);
    
    // 根据协议类型调用相应处理函数
    switch (protocolType) {
        case MMP_PROTOCOL_SEMP:
            processSempMessage(targetBuffer, parse->length);
            break;
            
        case MMP_PROTOCOL_NMEA:
            processNmeaMessage(targetBuffer, parse->length);
            break;
            
        case MMP_PROTOCOL_UBLOX:
            processUbloxMessage(targetBuffer, parse->length);
            break;
            
        default:
            printf("❓ 未处理的协议类型: %d\n", protocolType);
            break;
    }
    
    // 切换到下一个缓冲区
    g_currentBuffer = (g_currentBuffer + 1) % 4;
    
    printf("   📈 统计: 已处理%u条消息, %u字节\n\n", g_messagesReceived, g_processedBytes);
}

// CRC错误处理回调
bool onCrcError(MMP_PARSE_STATE *parse)
{
    printf("❌ CRC错误: 长度%d字节, 协议%s\n", 
           parse->length, mmpGetProtocolName((MMP_PROTOCOL_TYPE)parse->type));
    return false; // 丢弃错误的消息
}

// 调试输出回调
void debugPrint(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    printf("🔧 [DEBUG] ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

// 错误输出回调
void errorPrint(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    printf("❌ [ERROR] ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

//----------------------------------------
// UART中断处理
//----------------------------------------

// 模拟UART接收中断
void UART1_IRQHandler(void)
{
    // 检查接收数据寄存器非空
    if (UART1.STATUS & 0x01) {
        uint8_t receivedByte = UART1.DATA;
        g_processedBytes++;
        
        // 直接在中断中处理字节（适用于低频数据）
        mmpProcessByte(&g_multiParser, receivedByte);
        
        // 或者存储到缓冲区稍后处理（适用于高频数据）
        /*
        uint16_t nextHead = (g_uartRxHead + 1) % sizeof(g_uartRxBuffer);
        if (nextHead != g_uartRxTail) {
            g_uartRxBuffer[g_uartRxHead] = receivedByte;
            g_uartRxHead = nextHead;
        }
        */
    }
}

//----------------------------------------
// DMA处理
//----------------------------------------

// DMA接收完成中断
void DMA1_IRQHandler(void)
{
    // 检查传输完成标志
    if (DMA1.STATUS & 0x01) {
        uint16_t receivedCount = DMA1.COUNT;
        
        printf("📦 DMA接收完成: %d字节\n", receivedCount);
        
        // 批量处理接收到的数据
        uint16_t processed = mmpProcessBuffer(&g_multiParser, g_uartRxBuffer, receivedCount);
        
        printf("   处理了 %d/%d 字节\n", processed, receivedCount);
        
        // 重新启动DMA接收
        DMA1.DST_ADDR = g_uartRxBuffer;
        DMA1.COUNT = sizeof(g_uartRxBuffer);
        DMA1.CTRL |= 0x01; // 启动DMA
        
        // 清除中断标志
        DMA1.STATUS &= ~0x01;
    }
}

//----------------------------------------
// 网络接口处理
//----------------------------------------

// 模拟WiFi/以太网数据包处理
void processNetworkPacket(uint8_t *packet, uint16_t length)
{
    printf("🌐 网络数据包处理: %d字节\n", length);
    
    // 在网络数据中查找协议消息
    for (uint16_t i = 0; i < length; i++) {
        mmpProcessByte(&g_multiParser, packet[i]);
    }
}

// 模拟蓝牙数据处理
void processBluetoothData(uint8_t *data, uint16_t length)
{
    printf("📱 蓝牙数据处理: %d字节\n", length);
    
    // 批量处理蓝牙数据
    uint16_t processed = mmpProcessBuffer(&g_multiParser, data, length);
    printf("   蓝牙数据解析: %d/%d字节\n", processed, length);
}

//----------------------------------------
// 任务调度处理
//----------------------------------------

// 后台任务：处理缓冲区中的数据
void backgroundProcessTask(void)
{
    // 处理UART接收缓冲区
    while (g_uartRxTail != g_uartRxHead) {
        uint8_t data = g_uartRxBuffer[g_uartRxTail];
        g_uartRxTail = (g_uartRxTail + 1) % sizeof(g_uartRxBuffer);
        
        mmpProcessByte(&g_multiParser, data);
    }
}

// 统计任务：定期打印统计信息
void statisticsTask(void)
{
    printf("\n📊 === 多协议解析器统计报告 ===\n");
    mmpPrintStats(&g_multiParser);
    printf("总接收字节: %u\n", g_processedBytes);
    printf("总消息数: %u\n", g_messagesReceived);
    printf("=============================\n\n");
}

//----------------------------------------
// 初始化函数
//----------------------------------------

bool initMultiProtocolSystem(void)
{
    printf("🚀 初始化多协议通信系统...\n");
    
    // 初始化多协议解析器
    if (!mmpInitParser(&g_multiParser,
                       g_parseBuffer, sizeof(g_parseBuffer),
                       onMessageComplete,
                       onCrcError,
                       "嵌入式多协议解析器",
                       errorPrint,
                       NULL)) { // 生产环境中通常关闭调试输出
        printf("❌ 多协议解析器初始化失败!\n");
        return false;
    }
    
    // 初始化UART（模拟）
    UART1.CTRL = 0x01; // 启用UART
    printf("✅ UART1初始化完成\n");
    
    // 初始化DMA（模拟）
    DMA1.DST_ADDR = g_uartRxBuffer;
    DMA1.COUNT = sizeof(g_uartRxBuffer);
    DMA1.CTRL = 0x01; // 启用DMA
    printf("✅ DMA1初始化完成\n");
    
    printf("✅ 多协议通信系统初始化成功!\n");
    printf("📋 支持的协议: ");
    for (int i = 0; i < MMP_PROTOCOL_COUNT; i++) {
        printf("%s ", mmpGetProtocolName((MMP_PROTOCOL_TYPE)i));
    }
    printf("\n\n");
    
    return true;
}

//----------------------------------------
// 测试函数
//----------------------------------------

void testEmbeddedScenarios(void)
{
    printf("🧪 嵌入式应用场景测试\n");
    printf("=====================================\n\n");
    
    // 场景1: UART串口数据接收
    printf("📡 场景1: UART串口数据接收\n");
    uint8_t uartData[] = {0xAA, 0x44, 0x18, 0x14, 0x01, 0x00, 0x00, 0x00, 
                          0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 
                          0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0xC4, 0xD8, 0xDB, 0x6B};
    
    for (size_t i = 0; i < sizeof(uartData); i++) {
        UART1.DATA = uartData[i];
        UART1.STATUS |= 0x01; // 设置数据就绪标志
        UART1_IRQHandler(); // 模拟中断
    }
    printf("\n");
    
    // 场景2: DMA批量数据接收
    printf("📦 场景2: DMA批量数据接收\n");
    uint8_t dmaData[] = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    memcpy(g_uartRxBuffer, dmaData, sizeof(dmaData) - 1);
    DMA1.COUNT = sizeof(dmaData) - 1;
    DMA1.STATUS |= 0x01; // 设置传输完成标志
    DMA1_IRQHandler(); // 模拟DMA中断
    printf("\n");
    
    // 场景3: 网络数据包处理
    printf("🌐 场景3: 网络数据包处理\n");
    uint8_t networkPacket[] = {0xB5, 0x62, 0x01, 0x07, 0x04, 0x00, 0x12, 0x34, 0x56, 0x78, 0x99, 0x0D};
    processNetworkPacket(networkPacket, sizeof(networkPacket));
    printf("\n");
    
    // 场景4: 蓝牙数据处理
    printf("📱 场景4: 蓝牙数据处理\n");
    uint8_t bluetoothData[] = "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\r\n";
    processBluetoothData(bluetoothData, sizeof(bluetoothData) - 1);
    printf("\n");
    
    // 场景5: 混合数据流处理
    printf("🔄 场景5: 混合数据流处理\n");
    uint8_t mixedStream[200];
    size_t offset = 0;
    
    // 添加SEMP数据
    memcpy(mixedStream + offset, uartData, sizeof(uartData));
    offset += sizeof(uartData);
    
    // 添加噪声
    mixedStream[offset++] = 0xFF;
    mixedStream[offset++] = 0x00;
    
    // 添加NMEA数据
    memcpy(mixedStream + offset, dmaData, sizeof(dmaData) - 1);
    offset += sizeof(dmaData) - 1;
    
    processNetworkPacket(mixedStream, offset);
    printf("\n");
}

//----------------------------------------
// 主程序
//----------------------------------------

int main(void)
{
    printf("🔧 嵌入式多协议解析器应用示例\n");
    printf("================================================\n\n");
    
    // 初始化系统
    if (!initMultiProtocolSystem()) {
        return -1;
    }
    
    // 运行测试场景
    testEmbeddedScenarios();
    
    // 显示最终统计
    statisticsTask();
    
    printf("💡 实际使用建议:\n");
    printf("================\n");
    printf("1. 🎯 中断优化:\n");
    printf("   - 在UART中断中只存储数据到缓冲区\n");
    printf("   - 在主循环或低优先级任务中进行解析\n");
    printf("   - 使用DMA减少CPU占用\n\n");
    
    printf("2. 📊 内存优化:\n");
    printf("   - 根据实际需求调整缓冲区大小\n");
    printf("   - 使用多个小缓冲区避免阻塞\n");
    printf("   - 及时处理完成的消息释放空间\n\n");
    
    printf("3. ⚡ 性能优化:\n");
    printf("   - 优先处理高频协议\n");
    printf("   - 使用协议优先级队列\n");
    printf("   - 监控统计信息进行调优\n\n");
    
    printf("4. 🛡️  错误处理:\n");
    printf("   - 实现超时机制防止死锁\n");
    printf("   - 记录错误统计便于调试\n");
    printf("   - 提供协议重置恢复机制\n\n");
    
    printf("🎉 示例程序完成!\n");
    
    return 0;
} 