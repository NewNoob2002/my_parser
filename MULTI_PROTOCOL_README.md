# 多协议解析器 (Multi-Protocol Parser)

## 概述

多协议解析器是一个强大的通信协议解析框架，能够在单一数据流中同时识别和解析多种不同的通信协议。该框架基于我们之前开发的SEMP协议解析器扩展而来，采用统一的状态机架构，支持实时协议自动识别和并发解析。

## 主要特性

### 🚀 协议支持
- **SEMP协议**: 自定义的高效二进制协议（0xAA 0x44 0x18前导）
- **NMEA协议**: 标准GPS导航数据协议（$开头的ASCII协议）
- **u-blox协议**: u-blox GNSS模块二进制协议（0xB5 0x62前导）
- **RTCM协议**: 差分GPS校正数据协议（0xD3前导）
- **扩展支持**: 框架设计支持轻松添加新协议

### ⚡ 核心特性
- **实时协议识别**: 自动检测数据流中的协议类型
- **状态机解析**: 高效的字节级流式解析
- **内存优化**: 最小化内存使用，适合嵌入式系统
- **中断友好**: 支持UART中断、DMA等多种数据源
- **统计监控**: 内置协议解析统计和错误监控
- **错误恢复**: 自动错误恢复和重新同步

### 🛠️ 应用场景
- **GNSS接收机**: 同时处理多种卫星导航协议
- **物联网设备**: 统一处理不同厂商的通信协议  
- **数据采集系统**: 多源数据融合和解析
- **网关设备**: 协议转换和桥接
- **测试设备**: 协议兼容性测试和验证

## 快速开始

### 编译和运行

```bash
# 编译所有程序
make all

# 运行多协议演示程序
make multi_demo
./bin/multi_demo

# 运行嵌入式应用示例
make multi_example  
./bin/multi_example

# 查看帮助信息
make help
```

### 基本使用

```c
#include "multi_protocol_parser.h"

// 1. 创建解析器实例
static MMP_PARSE_STATE parser;
static uint8_t buffer[1024];

// 2. 定义消息处理回调
void onMessage(MMP_PARSE_STATE *parse, uint16_t protocolType) {
    switch (protocolType) {
        case MMP_PROTOCOL_SEMP:
            // 处理SEMP协议消息
            processSempMessage(parse->buffer, parse->length);
            break;
        case MMP_PROTOCOL_NMEA:
            // 处理NMEA GPS消息
            processNmeaMessage(parse->buffer, parse->length);
            break;
        // ... 其他协议
    }
}

// 3. 初始化解析器
mmpInitParser(&parser, buffer, sizeof(buffer), 
              onMessage, NULL, "MyParser", NULL, NULL);

// 4. 处理接收数据
void processIncomingData(uint8_t *data, uint16_t length) {
    for (uint16_t i = 0; i < length; i++) {
        mmpProcessByte(&parser, data[i]);
    }
}
```

## 嵌入式系统集成

### UART中断处理

```c
// UART接收中断
void UART_IRQHandler(void) {
    if (UART->SR & UART_FLAG_RXNE) {
        uint8_t data = UART->DR;
        mmpProcessByte(&g_parser, data);
    }
}
```

### DMA批量处理

```c
// DMA传输完成中断
void DMA_IRQHandler(void) {
    if (DMA->ISR & DMA_FLAG_TC) {
        uint16_t count = DMA_BUFFER_SIZE - DMA->CNDTR;
        mmpProcessBuffer(&g_parser, dma_buffer, count);
        // 重启DMA传输
        DMA->CNDTR = DMA_BUFFER_SIZE;
        DMA->CCR |= DMA_CCR_EN;
    }
}
```

### 任务调度集成

```c
// FreeRTOS任务示例
void vParserTask(void *pvParameters) {
    while (1) {
        // 处理接收队列中的数据
        uint8_t rxData[64];
        uint16_t length = receiveFromQueue(rxData, sizeof(rxData));
        if (length > 0) {
            mmpProcessBuffer(&g_parser, rxData, length);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

## 协议详细说明

### SEMP协议格式
```
同步头: 0xAA 0x44 0x18
头长度: 0x14 (20字节)
消息结构: [同步头3字节] + [头长度1字节] + [消息头16字节] + [数据N字节] + [CRC32 4字节]
```

### NMEA协议格式
```
格式: $TALKER,field1,field2,...,fieldN*checksum\r\n
示例: $GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47
```

### u-blox协议格式  
```
同步头: 0xB5 0x62
消息结构: [同步头2字节] + [类别1字节] + [ID1字节] + [长度2字节] + [数据N字节] + [校验2字节]
```

## 性能特征

### 内存使用
- **解析器状态**: ~200字节
- **缓冲区**: 用户可配置（建议512-2048字节）
- **临时数据**: ~100字节
- **总计**: < 3KB（基本配置）

### 处理性能
- **处理速度**: 10-20条CPU指令/字节
- **实时性**: 支持115200波特率以上UART
- **吞吐量**: > 100KB/s（在100MHz ARM Cortex-M4上）

### 错误处理
- **CRC验证**: 所有支持CRC的协议
- **超时保护**: 防止不完整消息阻塞
- **自动恢复**: 错误后自动重新同步
- **统计监控**: 成功率、错误率实时监控

## 添加新协议

框架支持轻松添加新的协议支持：

1. **定义协议枚举**
```c
typedef enum {
    MMP_PROTOCOL_SEMP = 0,
    MMP_PROTOCOL_NMEA,
    MMP_PROTOCOL_UBLOX,
    MMP_PROTOCOL_YOUR_PROTOCOL,  // 添加新协议
    MMP_PROTOCOL_COUNT
} MMP_PROTOCOL_TYPE;
```

2. **实现前导检测函数**
```c
bool mmpYourProtocolPreamble(MMP_PARSE_STATE *parse, uint8_t data) {
    if (data != YOUR_SYNC_BYTE) {
        return false;
    }
    // 初始化协议特定解析状态
    parse->state = yourProtocolNextState;
    return true;
}
```

3. **实现状态机函数**
```c
static bool yourProtocolReadHeader(MMP_PARSE_STATE *parse, uint8_t data) {
    // 处理头部数据
    return true;
}
```

4. **注册到解析器表**
```c
static const MMP_PARSE_ROUTINE mmp_parserTable[] = {
    mmpSempPreamble,
    mmpNmeaPreamble,
    mmpUbloxPreamble,
    mmpYourProtocolPreamble,  // 添加到表中
};
```

## 调试和诊断

### 统计信息查看
```c
// 打印解析统计
mmpPrintStats(&parser);

// 获取详细统计
MMP_PROTOCOL_STATS stats[MMP_PROTOCOL_COUNT];
uint16_t count = mmpGetProtocolStats(&parser, stats, MMP_PROTOCOL_COUNT);
for (int i = 0; i < count; i++) {
    printf("%s: 成功=%u, 错误=%u, 成功率=%.1f%%\n",
           stats[i].protocolName, stats[i].messagesProcessed,
           stats[i].crcErrors, stats[i].successRate);
}
```

### 调试输出
```c
// 启用调试输出
void debugPrint(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

// 初始化时传入调试回调
mmpInitParser(&parser, buffer, sizeof(buffer),
              onMessage, onCrcError, "DebugParser", 
              errorPrint, debugPrint);
```

## 最佳实践

### 1. 中断处理优化
- 在中断中只做数据缓存，避免复杂解析
- 使用DMA减少中断频率
- 在主循环或低优先级任务中进行解析

### 2. 内存管理
- 根据实际协议需求调整缓冲区大小
- 使用多个小缓冲区避免单点阻塞
- 及时处理完成的消息释放内存

### 3. 错误处理
- 实现消息超时机制
- 监控CRC错误率进行链路质量评估
- 提供协议重置和恢复机制

### 4. 性能优化
- 根据协议频率调整解析优先级
- 使用协议过滤减少不必要的处理
- 定期清理统计信息避免溢出

## 常见问题

### Q: 如何处理协议冲突？
A: 框架通过前导字节的唯一性来区分协议。确保新添加的协议具有独特的前导模式。

### Q: 支持的最大消息长度是多少？
A: 受限于初始化时指定的缓冲区大小。建议根据实际协议需求设置合适大小。

### Q: 如何优化实时性能？
A: 使用DMA批量传输，在低优先级任务中解析，避免在中断中进行复杂处理。

### Q: 支持线程安全吗？
A: 单个解析器实例不是线程安全的。多线程环境下请使用独立的解析器实例或添加互斥保护。

## 技术支持

如需技术支持或添加新协议，请参考：
- 源码中的详细注释
- `src/multi_protocol_demo.c` - 基础演示
- `src/multi_protocol_example.c` - 嵌入式应用示例
- 现有协议实现作为参考模板

---

**版本**: v1.0  
**更新日期**: 2024年12月  
**兼容性**: C99, ARM Cortex-M, x86/x64, RISC-V 