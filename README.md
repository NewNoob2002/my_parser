# SEMP 通信协议解析库

一个高效的通信协议解析库，专为32位单片机设计，支持 **20字节头部 + 可变长度数据 + 4字节CRC** 的协议格式。

## 协议格式

```
|<------------- 20字节头部 ------------------> |<----- 消息数据 ----->|<- 4字节CRC ->|
|                                            |                    |            |
+----------+--------+----------------+---------+----------+-------------+
| 同步字节   | 头部长度  | 消息ID和类型    | 消息       |   CRC-32Q   | 
|  3字节    |  1字节   |    16字节      |  n字节     |   4字节     |
| AA 44 18 |   14     |   (详见下表)   |           |             |
+----------+--------+----------------+---------+----------+-------------+
|                                                         |
|<------------------------ CRC覆盖范围 ------------------->|
```

### 头部结构 (20字节)
| 偏移 | 字段 | 大小 | 描述 |
|------|------|------|------|
| 0 | syncA | 1字节 | 同步字节1 (0xAA) |
| 1 | syncB | 1字节 | 同步字节2 (0x44) |
| 2 | syncC | 1字节 | 同步字节3 (0x18) |
| 3 | headerLength | 1字节 | 头部长度 (0x14) |
| 4-5 | messageId | 2字节 | 消息ID |
| 6-7 | RESERVED1 | 2字节 | 保留字段 |
| 8-11 | RESERVED_time | 4字节 | 保留时间戳 |
| 12-13 | messageLength | 2字节 | 消息数据长度 |
| 14-15 | RESERVED2 | 2字节 | 保留字段 |
| 16 | sender | 1字节 | 发送者ID |
| 17 | messageType | 1字节 | 消息类型 |
| 18 | Protocol | 1字节 | 协议版本 |
| 19 | MsgInterval | 1字节 | 消息间隔 |

## 特性

- ✅ **状态机解析**：逐字节解析，适合流式数据
- ✅ **CRC32校验**：完整的数据完整性检查
- ✅ **内存高效**：针对单片机优化的内存使用
- ✅ **中断友好**：适合在UART/SPI/I2C中断中使用
- ✅ **错误恢复**：自动重新同步，处理数据流中的错误
- ✅ **回调机制**：灵活的消息处理和错误处理
- ✅ **多场景支持**：UART、DMA、蓝牙、WiFi、文件流等

## 快速开始

### 1. 基本使用

```c
#include "semp_parser.h"

// 定义缓冲区和解析器
static uint8_t messageBuffer[SEMP_MAX_BUFFER_SIZE];
static SEMP_PARSE_STATE parser;

// 消息处理回调
void onMessageReceived(SEMP_PARSE_STATE *parse, uint8_t messageType)
{
    SEMP_MESSAGE_HEADER *header = (SEMP_MESSAGE_HEADER *)parse->buffer;
    printf("收到消息，类型: 0x%02X, ID: 0x%04X\n", 
           messageType, header->messageId);
}

// 初始化
sempInitParser(&parser, messageBuffer, sizeof(messageBuffer),
               onMessageReceived, NULL, "主解析器");

// 处理接收数据
uint8_t receivedByte = UART_ReadByte();
sempProcessByte(&parser, receivedByte);
```

### 2. UART中断处理

```c
void UART_IRQHandler(void)
{
    if (UART_GetFlagStatus(UART1, UART_FLAG_RXNE)) {
        uint8_t data = UART_ReceiveData(UART1);
        sempProcessByte(&parser, data);
        UART_ClearFlag(UART1, UART_FLAG_RXNE);
    }
}
```

### 3. DMA批量处理

```c
void DMA_IRQHandler(void)
{
    if (DMA_GetFlagStatus(DMA1_Stream0, DMA_FLAG_TCIF0)) {
        uint16_t length = sizeof(dmaBuffer) - DMA_GetCurrDataCounter(DMA1_Stream0);
        sempProcessBuffer(&parser, dmaBuffer, length);
        
        // 重启DMA接收
        DMA_SetCurrDataCounter(DMA1_Stream0, sizeof(dmaBuffer));
        DMA_Cmd(DMA1_Stream0, ENABLE);
        DMA_ClearFlag(DMA1_Stream0, DMA_FLAG_TCIF0);
    }
}
```

## API 参考

### 核心函数

#### `sempInitParser()`
```c
void sempInitParser(SEMP_PARSE_STATE *parse, 
                   uint8_t *buffer, 
                   uint16_t maxLength,
                   sempEomCallback eomCallback,
                   sempBadCrcCallback badCrcCallback,
                   const char *parserName);
```
初始化SEMP解析器。

**参数:**
- `parse`: 解析器状态结构体指针
- `buffer`: 消息缓冲区
- `maxLength`: 缓冲区最大长度  
- `eomCallback`: 消息完成回调函数
- `badCrcCallback`: CRC错误回调函数（可为NULL）
- `parserName`: 解析器名称（用于调试）

#### `sempProcessByte()`
```c
bool sempProcessByte(SEMP_PARSE_STATE *parse, uint8_t data);
```
处理单个接收字节。

**返回值:**
- `true`: 继续接收字节
- `false`: 消息处理完成或发生错误

#### `sempProcessBuffer()`
```c
uint16_t sempProcessBuffer(SEMP_PARSE_STATE *parse, uint8_t *data, uint16_t length);
```
批量处理数据缓冲区。

**返回值:** 已处理的字节数

### 回调函数类型

#### 消息完成回调
```c
typedef void (*sempEomCallback)(SEMP_PARSE_STATE *parse, uint8_t messageType);
```

#### CRC错误回调
```c
typedef bool (*sempBadCrcCallback)(SEMP_PARSE_STATE *parse);
```

## 消息类型示例

### 控制命令 (messageType = 0x01)
```c
void handleControlCommand(SEMP_MESSAGE_HEADER *header, uint8_t *fullMessage)
{
    uint8_t *cmdData = fullMessage + sizeof(SEMP_MESSAGE_HEADER);
    
    switch (header->messageId) {
        case 0x0001: // LED控制
            GPIO_WriteBit(GPIOC, GPIO_Pin_13, cmdData[0] ? Bit_SET : Bit_RESET);
            break;
        case 0x0002: // 电机控制
            uint16_t speed = (cmdData[1] << 8) | cmdData[0];
            setMotorSpeed(speed);
            break;
    }
}
```

### 数据传输 (messageType = 0x02)
```c
void handleDataTransfer(SEMP_MESSAGE_HEADER *header, uint8_t *fullMessage)
{
    uint8_t *data = fullMessage + sizeof(SEMP_MESSAGE_HEADER);
    
    switch (header->messageId) {
        case 0x1001: // 传感器数据
            float temperature = *(float*)data;
            saveTemperatureData(temperature);
            break;
        case 0x1002: // 配置数据
            writeConfigData(data, header->messageLength);
            break;
    }
}
```

## 性能优化建议

### 内存优化
- 根据实际需求调整 `SEMP_MAX_BUFFER_SIZE`
- 使用静态分配避免堆碎片
- 考虑使用环形缓冲区处理大量数据

### 性能优化
- 在中断中只进行字节解析，消息处理放在主循环
- 使用DMA减少CPU占用
- 考虑实现硬件CRC计算（如果MCU支持）

### 错误处理
- 实现消息超时机制
- 添加错误统计和重传机制
- 考虑添加消息序号检测重复包

## 移植指南

### STM32平台
```c
// UART接收中断
void USART1_IRQHandler(void)
{
    if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
        uint8_t data = USART_ReceiveData(USART1);
        sempProcessByte(&parser, data);
    }
}
```

### ESP32平台
```c
// UART事件任务
void uart_event_task(void *pvParameters)
{
    uart_event_t event;
    uint8_t data[128];
    
    while(1) {
        if(xQueueReceive(uart_queue, (void*)&event, portMAX_DELAY)) {
            if(event.type == UART_DATA) {
                int len = uart_read_bytes(UART_NUM_1, data, event.size, 100);
                sempProcessBuffer(&parser, data, len);
            }
        }
    }
}
```

### Arduino平台
```c
void setup() {
    Serial.begin(115200);
    sempInitParser(&parser, messageBuffer, sizeof(messageBuffer),
                   onMessageReceived, NULL, "Arduino解析器");
}

void loop() {
    while(Serial.available()) {
        uint8_t data = Serial.read();
        sempProcessByte(&parser, data);
    }
}
```

## 文件结构

```
src/
├── semp_parser.h      # 头文件
├── main.cpp           # 完整实现和测试代码
├── semp_example.c     # 使用示例
└── README.md          # 本文档
```

## 编译和测试

```bash
# 编译测试程序
gcc -o semp_parser src/main.cpp

# 运行测试
./semp_parser
```

**输出示例:**
```
开始测试SEMP协议解析器...
消息解析完成: 类型=0x02, 长度=28字节
消息ID: 0x0001
发送者: 0x01
协议版本: 0x01
测试完成
```

## 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](LICENSE) 文件。

## 贡献

欢迎提交 Issue 和 Pull Request！

## 联系方式

如有问题或建议，请通过以下方式联系：
- 创建 Issue
- 发送邮件至项目维护者

---

**注意**: 本库专为32位单片机优化，在实际产品中请根据具体需求调整缓冲区大小和错误处理机制。 