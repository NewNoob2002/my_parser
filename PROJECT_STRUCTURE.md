# SEMP协议解析库 - 项目结构

## 📁 目录结构

```
cProject/
├── src/                    # 源代码目录
│   ├── semp_parser.h      # 解析库头文件
│   ├── semp_parser.c      # 解析库实现
│   ├── main.cpp           # 简单演示程序
│   ├── test_tool.c        # 详细测试工具
│   └── semp_example.c     # 完整使用示例
├── bin/                   # 编译输出目录
│   ├── demo              # 演示程序
│   ├── test_tool         # 测试工具
│   └── example           # 示例程序
├── build/                 # 编译临时文件目录
├── Makefile              # 构建脚本
├── README.md             # 项目文档
└── PROJECT_STRUCTURE.md  # 本文件
```

## 📋 文件说明

### 核心库文件

| 文件 | 说明 | 用途 |
|------|------|------|
| `src/semp_parser.h` | 解析库头文件 | 包含所有API声明和数据结构定义 |
| `src/semp_parser.c` | 解析库实现 | 完整的协议解析功能实现 |

### 示例和测试程序

| 文件 | 说明 | 编译输出 |
|------|------|----------|
| `src/main.cpp` | 简单演示程序 | `bin/demo` |
| `src/test_tool.c` | 详细测试工具 | `bin/test_tool` |
| `src/semp_example.c` | 完整使用示例 | `bin/example` |

### 构建和文档

| 文件 | 说明 |
|------|------|
| `Makefile` | 构建脚本，支持编译、测试、清理等 |
| `README.md` | 完整的项目文档和API手册 |

## 🚀 快速开始

### 编译所有程序
```bash
make all
```

### 运行演示
```bash
make demo    # 简单演示
make test    # 详细测试
./bin/example  # 完整示例
```

### 清理编译文件
```bash
make clean
```

## 🔧 在您的项目中使用

### 方法1: 直接包含源文件
```c
// 在您的项目中包含这两个文件：
#include "semp_parser.h"
// 编译时链接: semp_parser.c
```

### 方法2: 静态库（可选）
```bash
make libsemp.a  # 创建静态库
# 然后在您的项目中链接 libsemp.a
```

### 方法3: 系统安装（可选）
```bash
sudo make install    # 安装到系统
sudo make uninstall  # 卸载
```

## 📊 内存占用

| 组件 | 大小 | 说明 |
|------|------|------|
| 解析器状态结构 | ~1KB | SEMP_PARSE_STATE |
| 消息缓冲区 | 可配置 | 默认1KB，可调整 |
| CRC查找表 | 1KB | 编译时常量 |
| **总计** | **~3KB** | **典型配置** |

## 🎯 使用场景

### 单片机项目
- 复制 `semp_parser.h` 和 `semp_parser.c` 到您的项目
- 根据需要调整 `SEMP_MAX_BUFFER_SIZE`
- 参考 `semp_example.c` 实现回调函数

### PC/Linux项目
- 使用Makefile编译
- 可选择静态库或直接包含源文件
- 参考 `main.cpp` 或 `test_tool.c`

### 嵌入式RTOS
- 适配中断处理函数
- 使用静态内存分配
- 参考示例中的UART/DMA处理代码

## 🔧 定制选项

### 缓冲区大小
```c
// 在semp_parser.h中修改
#define SEMP_MAX_BUFFER_SIZE 512  // 根据需要调整
```

### 调试输出
```c
// 启用/禁用调试输出
parse->printDebug = yourDebugFunction;  // 或NULL禁用
```

### 平台适配
- 修改数据类型定义（如果需要）
- 调整内存分配策略
- 适配特定硬件CRC功能

## 📈 性能特点

- **处理速度**: 每字节约10-20条CPU指令
- **内存效率**: 最小3KB RAM占用
- **实时性**: 适合中断环境逐字节处理
- **错误恢复**: 自动重新同步，抗干扰强

## 🤝 贡献指南

1. Fork此项目
2. 创建功能分支
3. 提交更改
4. 发起Pull Request

## 📞 技术支持

如有问题或建议：
- 查看README.md文档
- 运行测试工具分析数据包
- 参考示例代码
- 创建Issue反馈

---

**最后更新**: 2024年 