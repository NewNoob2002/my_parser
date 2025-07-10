/**
 * @file semp_example.c
 * @brief SEMP协议解析库使用示例
 * @details 展示如何在32位单片机项目中集成和使用SEMP协议解析库
 * 
 * 适用场景：
 * - UART/SPI/I2C通信接收
 * - 蓝牙/WiFi数据包解析
 * - 文件数据流处理
 */

#include "semp_parser.h"
#include <stdio.h>

// 函数前向声明
void handleControlCommand(SEMP_MESSAGE_HEADER *header, uint8_t *fullMessage);
void handleDataTransfer(SEMP_MESSAGE_HEADER *header, uint8_t *fullMessage);
void handleStatusQuery(SEMP_MESSAGE_HEADER *header, uint8_t *fullMessage);

// ===== 单片机典型应用示例 =====

// 全局解析器实例（适合中断环境）
static SEMP_PARSE_STATE g_sempParser;
static uint8_t g_messageBuffer[SEMP_MAX_BUFFER_SIZE];

// 消息处理回调函数
void onMessageReceived(SEMP_PARSE_STATE *parse, uint8_t messageType)
{
    SEMP_MESSAGE_HEADER *header = (SEMP_MESSAGE_HEADER *)parse->buffer;
    
    printf("=== 收到完整消息 ===\n");
    printf("消息类型: 0x%02X\n", messageType);
    printf("消息ID: 0x%04X\n", header->messageId);
    printf("发送者: 0x%02X\n", header->sender);
    printf("协议版本: 0x%02X\n", header->Protocol);
    printf("总长度: %d字节\n", parse->length);
    
    // 提取数据部分（跳过20字节头部）
    if (parse->length > sizeof(SEMP_MESSAGE_HEADER)) {
        uint8_t *messageData = parse->buffer + sizeof(SEMP_MESSAGE_HEADER);
        uint16_t dataLength = parse->length - sizeof(SEMP_MESSAGE_HEADER) - SEMP_CRC_SIZE;
        
        printf("数据长度: %d字节\n", dataLength);
        printf("数据内容: ");
        for (uint16_t i = 0; i < dataLength && i < 16; i++) {
            printf("%02X ", messageData[i]);
        }
        if (dataLength > 16) printf("...");
        printf("\n");
    }
    
    // 根据消息类型执行相应处理
    switch (messageType) {
        case 0x01: // 控制命令
            handleControlCommand(header, parse->buffer);
            break;
        case 0x02: // 数据传输
            handleDataTransfer(header, parse->buffer);
            break;
        case 0x03: // 状态查询
            handleStatusQuery(header, parse->buffer);
            break;
        default:
            printf("未知消息类型: 0x%02X\n", messageType);
            break;
    }
    printf("==================\n\n");
}

// CRC错误处理回调（可选）
bool onCrcError(SEMP_PARSE_STATE *parse)
{
    (void)parse; // 消除未使用参数警告
    printf("CRC校验失败，消息被丢弃\n");
    // 可以在这里记录错误统计、触发重传等
    return false; // 返回false表示丢弃消息
}

// 初始化SEMP解析器
void sempInit(void)
{
    sempInitParser(&g_sempParser,
                   g_messageBuffer,
                   sizeof(g_messageBuffer),
                   onMessageReceived,
                   onCrcError,
                   "主解析器");
    
    printf("SEMP协议解析器初始化完成\n");
}

// ===== UART接收中断处理示例 =====
// 注意: 以下代码仅为示例，实际使用时需要根据具体MCU调整
/*
void UART_IRQHandler(void)
{
    if (UART_GetFlagStatus(UART1, UART_FLAG_RXNE)) {
        uint8_t receivedByte = UART_ReceiveData(UART1);
        
        // 将接收到的字节传递给解析器
        sempProcessByte(&g_sempParser, receivedByte);
        
        UART_ClearFlag(UART1, UART_FLAG_RXNE);
    }
}
*/

// ===== DMA接收完成中断处理示例 =====
// 注意: 以下代码仅为示例，实际使用时需要根据具体MCU调整
/*
static uint8_t dmaBuffer[256];

void DMA_IRQHandler(void)
{
    if (DMA_GetFlagStatus(DMA1_Stream0, DMA_FLAG_TCIF0)) {
        uint16_t receivedLength = sizeof(dmaBuffer) - DMA_GetCurrDataCounter(DMA1_Stream0);
        
        // 处理DMA接收的数据缓冲区
        sempProcessBuffer(&g_sempParser, dmaBuffer, receivedLength);
        
        // 重新启动DMA接收
        DMA_SetCurrDataCounter(DMA1_Stream0, sizeof(dmaBuffer));
        DMA_Cmd(DMA1_Stream0, ENABLE);
        
        DMA_ClearFlag(DMA1_Stream0, DMA_FLAG_TCIF0);
    }
}
*/

// ===== 蓝牙/WiFi数据包处理示例 =====
void processBluetoothPacket(uint8_t *packet, uint16_t length)
{
    // 逐字节处理接收到的蓝牙数据包
    for (uint16_t i = 0; i < length; i++) {
        sempProcessByte(&g_sempParser, packet[i]);
    }
}

// ===== 文件流处理示例 =====
void processFileStream(FILE *file)
{
    uint8_t buffer[512];
    size_t bytesRead;
    
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        sempProcessBuffer(&g_sempParser, buffer, (uint16_t)bytesRead);
    }
}

// ===== 消息类型处理函数示例 =====

void handleControlCommand(SEMP_MESSAGE_HEADER *header, uint8_t *fullMessage)
{
    printf("处理控制命令 (ID: 0x%04X)\n", header->messageId);
    
    // 提取命令数据
    uint8_t *cmdData = fullMessage + sizeof(SEMP_MESSAGE_HEADER);
    uint16_t cmdLength = header->messageLength;
    
    // 根据消息ID执行相应操作
    switch (header->messageId) {
        case 0x0001: // LED控制
            if (cmdLength >= 1) {
                uint8_t ledState = cmdData[0];
                printf("设置LED状态: %s\n", ledState ? "开启" : "关闭");
                // 控制LED硬件
                //GPIO_WriteBit(GPIOC, GPIO_Pin_13, ledState ? Bit_SET : Bit_RESET);
            }
            break;
            
        case 0x0002: // 电机控制
            if (cmdLength >= 2) {
                uint16_t motorSpeed = (cmdData[1] << 8) | cmdData[0];
                printf("设置电机速度: %d RPM\n", motorSpeed);
                // 控制电机硬件
                //setMotorSpeed(motorSpeed);
            }
            break;
            
        default:
            printf("未知控制命令ID: 0x%04X\n", header->messageId);
            break;
    }
}

void handleDataTransfer(SEMP_MESSAGE_HEADER *header, uint8_t *fullMessage)
{
    printf("处理数据传输 (ID: 0x%04X)\n", header->messageId);
    
    uint8_t *data = fullMessage + sizeof(SEMP_MESSAGE_HEADER);
    uint16_t dataLength = header->messageLength;
    
    // 数据存储或转发处理
    switch (header->messageId) {
        case 0x1001: // 传感器数据
            if (dataLength >= 4) {
                float temperature = *(float*)data;
                printf("接收到温度数据: %.2f°C\n", temperature);
                // 存储到EEPROM或发送到云端
                //saveTemperatureData(temperature);
            }
            break;
            
        case 0x1002: // 配置数据
            printf("接收到配置数据，长度: %d字节\n", dataLength);
            // 写入配置存储器
            //writeConfigData(data, dataLength);
            break;
            
        default:
            printf("未知数据传输ID: 0x%04X\n", header->messageId);
            break;
    }
}

void handleStatusQuery(SEMP_MESSAGE_HEADER *header, uint8_t *fullMessage)
{
    (void)fullMessage; // 消除未使用参数警告
    printf("处理状态查询 (ID: 0x%04X)\n", header->messageId);
    
    // 根据查询类型返回相应状态
    switch (header->messageId) {
        case 0x2001: // 系统状态查询
            printf("返回系统状态信息\n");
            // 发送状态响应
            //sendSystemStatus();
            break;
            
        case 0x2002: // 传感器状态查询
            printf("返回传感器状态信息\n");
            // 发送传感器数据
            //sendSensorStatus();
            break;
            
        default:
            printf("未知状态查询ID: 0x%04X\n", header->messageId);
            break;
    }
}

// ===== 测试函数 =====

void testSempParser(void)
{
    printf("开始测试SEMP协议解析器...\n\n");
    
    // 初始化解析器
    sempInit();
    
    // 测试数据1：LED控制命令
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
    
    printf("测试消息1 - LED控制命令:\n");
    sempProcessBuffer(&g_sempParser, testMessage1, sizeof(testMessage1));
    
    printf("测试消息2 - 温度传感器数据:\n");
    sempProcessBuffer(&g_sempParser, testMessage2, sizeof(testMessage2));
    
    printf("测试完成\n");
}

// ===== 性能优化建议 =====

/*
 * 针对32位单片机的优化建议：
 * 
 * 1. 内存优化：
 *    - 根据实际需求调整 SEMP_MAX_BUFFER_SIZE
 *    - 使用静态分配避免堆碎片
 *    - 考虑使用环形缓冲区处理大量数据
 * 
 * 2. 性能优化：
 *    - 在中断中只进行字节解析，消息处理放在主循环
 *    - 使用DMA减少CPU占用
 *    - 考虑实现硬件CRC计算（如果MCU支持）
 * 
 * 3. 错误处理：
 *    - 实现消息超时机制
 *    - 添加错误统计和重传机制
 *    - 考虑添加消息序号检测重复包
 * 
 * 4. 移植性：
 *    - 避免使用printf在实际产品中（使用串口输出或日志系统）
 *    - 适配不同编译器的数据类型定义
 *    - 考虑大小端字节序兼容性
 */

// ===== 主函数 - 演示完整功能 =====
int main(void)
{
    printf("=== SEMP协议解析库完整示例 ===\n\n");
    
    // 初始化解析器
    sempInit();
    
    printf("📖 此示例展示了SEMP解析库在实际项目中的使用方法\n");
    printf("📋 包含的功能:\n");
    printf("   • UART中断处理 (注释状态)\n");
    printf("   • DMA批量处理 (注释状态)\n");
    printf("   • 蓝牙/WiFi数据包处理\n");
    printf("   • 文件流处理\n");
    printf("   • 完整的消息类型处理框架\n");
    printf("   • 性能优化建议\n\n");
    
    printf("🔧 在实际单片机项目中使用时:\n");
    printf("   1. 取消注释相关中断处理函数\n");
    printf("   2. 根据具体MCU调整硬件相关代码\n");
    printf("   3. 实现具体的消息处理逻辑\n");
    printf("   4. 配置相应的回调函数\n\n");
    
    // 模拟蓝牙数据包处理
    uint8_t bluetoothPacket[] = {
        0xAA, 0x44, 0x18, 0x14, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x00,
        0x01, 0x8E, 0x42, 0x7A, 0x75
    };
    
    printf("🧪 模拟蓝牙数据包处理...\n");
    processBluetoothPacket(bluetoothPacket, sizeof(bluetoothPacket));
    
    printf("✨ 示例程序运行完成！\n");
    printf("📚 更多详细信息请查看源代码注释\n");
    
    return 0;
} 