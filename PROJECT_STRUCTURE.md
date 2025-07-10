# 通信协议解析库 - 项目结构

## 📁 目录结构

```
cProject/
├── src/                           # 源代码目录
│   ├── semp_parser.h              # SEMP协议解析器头文件
│   ├── semp_parser.c              # SEMP协议解析器实现
│   ├── multi_protocol_parser.h    # 多协议解析器头文件 ⭐ 新增
│   ├── multi_protocol_parser.c    # 多协议解析器实现 ⭐ 新增
│   ├── main.cpp                   # SEMP简单演示程序
│   ├── test_tool.c                # SEMP详细测试工具
│   ├── semp_example.c             # SEMP完整使用示例
│   ├── multi_protocol_demo.c      # 多协议演示程序 ⭐ 新增
│   └── multi_protocol_example.c   # 多协议嵌入式应用示例 ⭐ 新增
├── bin/                           # 编译输出目录
│   ├── demo                       # SEMP演示程序
│   ├── test_tool                  # SEMP测试工具
│   ├── example                    # SEMP示例程序
│   ├── multi_demo                 # 多协议演示程序 ⭐ 新增
│   └── multi_example              # 多协议应用示例 ⭐ 新增
├── lib/                           # 参考实现目录
│   ├── SparkFun_Extensible_Message_Parser.cpp  # 扩展消息解析器框架
│   ├── Parse_BT.cpp               # 蓝牙协议解析器
│   ├── Parse_NMEA.cpp             # NMEA协议解析器
│   ├── Parse_UBLOX.cpp            # u-blox协议解析器
│   └── ...                        # 其他协议参考实现
├── build/                         # 编译临时文件目录
├── Makefile                       # 构建脚本（已更新）
├── README.md                      # SEMP协议文档
├── MULTI_PROTOCOL_README.md       # 多协议解析器文档 ⭐ 新增
└── PROJECT_STRUCTURE.md           # 本文件
```

## 📋 文件分类说明

### 🔧 核心解析库

#### SEMP单协议解析器
| 文件 | 说明 | 用途 |
|------|------|------|
| `src/semp_parser.h` | SEMP解析器头文件 | SEMP协议API声明和结构定义 |
| `src/semp_parser.c` | SEMP解析器实现 | SEMP协议完整解析功能 |

#### 多协议解析框架 ⭐ 新增
| 文件 | 说明 | 用途 |
|------|------|------|
| `src/multi_protocol_parser.h` | 多协议解析器头文件 | 统一多协议API和协议枚举 |
| `src/multi_protocol_parser.c` | 多协议解析器实现 | 支持SEMP/NMEA/u-blox/RTCM等协议 |

### 🎯 应用程序

#### SEMP单协议应用
| 文件 | 说明 | 编译输出 | 功能 |
|------|------|----------|------|
| `src/main.cpp` | SEMP演示程序 | `bin/demo` | 基本功能验证 |
| `src/test_tool.c` | SEMP测试工具 | `bin/test_tool` | 数据包详细分析 |
| `src/semp_example.c` | SEMP完整示例 | `bin/example` | UART/DMA/蓝牙集成 |

#### 多协议应用 ⭐ 新增
| 文件 | 说明 | 编译输出 | 功能 |
|------|------|----------|------|
| `src/multi_protocol_demo.c` | 多协议演示 | `bin/multi_demo` | 协议识别和混合流处理 |
| `src/multi_protocol_example.c` | 嵌入式应用示例 | `bin/multi_example` | 中断/DMA/网络集成 |

### 📚 参考资料
| 目录/文件 | 说明 |
|-----------|------|
| `lib/SparkFun_*.cpp` | SparkFun扩展消息解析器框架 |
| `lib/Parse_*.cpp` | 各种协议解析器参考实现 |

### 📄 文档
| 文件 | 说明 |
|------|------|
| `README.md` | SEMP协议详细文档 |
| `MULTI_PROTOCOL_README.md` | 多协议解析器文档 ⭐ 新增 |
| `PROJECT_STRUCTURE.md` | 项目结构说明（本文件）|

## 🚀 快速开始

### 编译所有程序
```bash
make all
```

### SEMP单协议测试
```bash
make demo          # SEMP简单演示
make test          # SEMP详细测试
./bin/example      # SEMP完整示例
```

### 多协议测试 ⭐ 新功能
```bash
make multi_demo    # 多协议演示
./bin/multi_demo

make multi_example # 嵌入式应用示例
./bin/multi_example
```

### 清理和维护
```bash
make clean         # 清理编译文件
make help          # 查看所有可用命令
```

## 🌟 协议支持对比

| 特性 | SEMP单协议解析器 | 多协议解析器 |
|------|------------------|---------------|
| **支持协议** | SEMP | SEMP + NMEA + u-blox + RTCM |
| **协议识别** | 单一协议 | 自动识别多协议 |
| **内存占用** | ~3KB | ~4KB |
| **处理复杂度** | 简单 | 中等 |
| **适用场景** | SEMP专用设备 | 多协议网关/测试设备 |
| **扩展性** | 固定协议 | 易于添加新协议 |

## 🔧 在您的项目中使用

### 选择合适的解析器

#### 选择SEMP单协议解析器，如果：
- 只需要处理SEMP协议
- 追求最小内存占用
- 对性能要求极高
- 项目复杂度要求最小

#### 选择多协议解析器，如果：
- 需要处理多种协议
- 构建协议网关或转换设备
- 进行协议兼容性测试
- 未来可能支持更多协议

### 集成方法

#### 方法1: 直接包含源文件
```c
// SEMP单协议
#include "semp_parser.h"
// 编译时包含: semp_parser.c

// 或多协议
#include "multi_protocol_parser.h"
// 编译时包含: multi_protocol_parser.c
```

#### 方法2: 系统安装（可选）
```bash
sudo make install    # 安装库文件
sudo make uninstall  # 卸载
```

## 📊 性能和内存对比

### SEMP单协议解析器
| 组件 | 大小 | 说明 |
|------|------|------|
| 解析器状态 | ~1KB | SEMP_PARSE_STATE |
| 消息缓冲区 | 可配置 | 默认1KB |
| CRC查找表 | 1KB | 静态常量 |
| **总计** | **~3KB** | **最小配置** |

### 多协议解析器 ⭐ 新增
| 组件 | 大小 | 说明 |
|------|------|------|
| 解析器状态 | ~200字节 | MMP_PARSE_STATE |
| 临时数据区 | ~100字节 | 协议特定数据 |
| 消息缓冲区 | 可配置 | 默认1KB |
| CRC查找表 | 1KB | 静态常量 |
| 协议名称表 | ~100字节 | 静态字符串 |
| **总计** | **~2.5KB** | **基本配置** |

### 性能特点对比
| 特性 | SEMP单协议 | 多协议解析器 |
|------|------------|---------------|
| **处理速度** | 10-15指令/字节 | 15-25指令/字节 |
| **内存效率** | 极高 | 高 |
| **实时性** | 优秀 | 良好 |
| **错误恢复** | 自动重同步 | 自动重同步 + 统计 |

## 🎯 应用场景

### SEMP单协议解析器适用于：
- **专用SEMP设备**: 传感器节点、执行器
- **高性能要求**: 实时控制系统
- **资源受限**: 低成本MCU项目
- **简单应用**: 点对点通信

### 多协议解析器适用于：
- **GNSS接收机**: 处理多种卫星协议
- **物联网网关**: 统一多厂商设备协议
- **测试设备**: 协议兼容性验证
- **数据采集**: 多源数据融合
- **协议转换**: 不同协议间的桥接

## 🔧 定制和扩展

### SEMP单协议定制
```c
// 调整缓冲区大小
#define SEMP_MAX_BUFFER_SIZE 512

// 启用/禁用调试
parse->printDebug = debugFunction;  // 或NULL
```

### 多协议解析器扩展 ⭐ 新功能
```c
// 添加新协议支持
typedef enum {
    MMP_PROTOCOL_SEMP = 0,
    MMP_PROTOCOL_NMEA,
    MMP_PROTOCOL_UBLOX,
    MMP_PROTOCOL_YOUR_NEW_PROTOCOL,  // 添加新协议
    MMP_PROTOCOL_COUNT
} MMP_PROTOCOL_TYPE;

// 实现新协议前导检测
bool mmpYourProtocolPreamble(MMP_PARSE_STATE *parse, uint8_t data);
```

## 📈 迁移指南

### 从SEMP单协议迁移到多协议

1. **更换头文件**
```c
// 原来
#include "semp_parser.h"

// 改为
#include "multi_protocol_parser.h"
```

2. **更新初始化代码**
```c
// 原来
sempInitParser(&parse, buffer, size, callback, ...);

// 改为
mmpInitParser(&parse, buffer, size, callback, ...);
```

3. **更新回调函数**
```c
// 新增协议类型参数
void onMessage(MMP_PARSE_STATE *parse, uint16_t protocolType) {
    switch (protocolType) {
        case MMP_PROTOCOL_SEMP:
            // 原有SEMP处理逻辑
            break;
        case MMP_PROTOCOL_NMEA:
            // 新增NMEA处理
            break;
        // ...
    }
}
```

## 📞 技术支持

### 文档资源
- **SEMP协议**: 查看 `README.md`
- **多协议框架**: 查看 `MULTI_PROTOCOL_README.md`
- **示例代码**: 参考 `src/*_example.c`

### 调试工具
- **SEMP数据包分析**: `./bin/test_tool`
- **多协议演示**: `./bin/multi_demo`
- **嵌入式场景**: `./bin/multi_example`

### 性能分析
```bash
# 查看统计信息
./bin/multi_demo    # 包含协议解析统计
./bin/multi_example # 包含性能建议
```

---

**最后更新**: 2024年12月  
**新增功能**: 多协议解析器框架 ⭐  
**兼容性**: 保持SEMP单协议解析器完整向后兼容 