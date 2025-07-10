#
# Makefile for Multi-Protocol Message Parser
# 支持单协议SEMP解析器和模块化多协议解析器
#

# 编译器设置
CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99 -Isrc/
LDFLAGS = 

# 目录设置
SRC_DIR = src
BIN_DIR = bin

# 确保目录存在
$(shell mkdir -p $(BIN_DIR))

#------------------------------------------------
# 源文件定义
#------------------------------------------------

# 原始SEMP解析器源文件
SEMP_SOURCES = $(SRC_DIR)/semp_parser.c

# 新的模块化消息解析器源文件
MESSAGE_PARSER_CORE = $(SRC_DIR)/Message_Parser.c
MESSAGE_PARSER_PROTOCOLS = $(SRC_DIR)/Parse_BT.c \
                          $(SRC_DIR)/Parse_NMEA.c \
                          $(SRC_DIR)/Parse_UBLOX.c \
                          $(SRC_DIR)/Parse_RTCM.c \
                          $(SRC_DIR)/Parse_Unicore_Binary.c \
                          $(SRC_DIR)/Parse_Unicore_Hash.c

MESSAGE_PARSER_SOURCES = $(MESSAGE_PARSER_CORE) $(MESSAGE_PARSER_PROTOCOLS)

# 示例和演示程序源文件
DEMO_SOURCES = $(SRC_DIR)/main.cpp
TEST_SOURCES = $(SRC_DIR)/test_tool.c
SEMP_EXAMPLE_SOURCES = $(SRC_DIR)/semp_example.c
MESSAGE_PARSER_DEMO_SOURCES = $(SRC_DIR)/message_parser_demo.c
MESSAGE_PARSER_EXAMPLE_SOURCES = $(SRC_DIR)/message_parser_example.c

#------------------------------------------------
# 目标文件定义
#------------------------------------------------

SEMP_OBJECTS = $(SEMP_SOURCES:.c=.o)
MESSAGE_PARSER_OBJECTS = $(MESSAGE_PARSER_SOURCES:.c=.o)

#------------------------------------------------
# 编译目标
#------------------------------------------------

.PHONY: all clean help install check docs
.PHONY: semp-demo semp-test semp-example 
.PHONY: mp-demo mp-example
.PHONY: demo test example multi_demo multi_example

# 默认目标 - 编译所有程序
all: semp-demo semp-test semp-example mp-demo mp-example

#------------------------------------------------
# SEMP协议解析器目标（保持向后兼容）
#------------------------------------------------

# SEMP演示程序（原main.cpp）
semp-demo: $(BIN_DIR)/semp_demo
demo: semp-demo

$(BIN_DIR)/semp_demo: $(SRC_DIR)/main.cpp $(SEMP_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# SEMP测试工具
semp-test: $(BIN_DIR)/semp_test
test: semp-test

$(BIN_DIR)/semp_test: $(SRC_DIR)/test_tool.c $(SEMP_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# SEMP示例程序
semp-example: $(BIN_DIR)/semp_example
example: semp-example

$(BIN_DIR)/semp_example: $(SRC_DIR)/semp_example.c $(SEMP_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

#------------------------------------------------
# 模块化消息解析器目标
#------------------------------------------------

# 消息解析器演示程序
mp-demo: $(BIN_DIR)/message_parser_demo
multi_demo: mp-demo

$(BIN_DIR)/message_parser_demo: $(SRC_DIR)/message_parser_demo.c $(MESSAGE_PARSER_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# 消息解析器示例程序
mp-example: $(BIN_DIR)/message_parser_example
multi_example: mp-example

$(BIN_DIR)/message_parser_example: $(SRC_DIR)/message_parser_example.c $(MESSAGE_PARSER_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

#------------------------------------------------
# 对象文件编译规则
#------------------------------------------------

# C文件编译规则
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# C++文件编译规则  
$(SRC_DIR)/main.o: $(SRC_DIR)/main.cpp
	$(CC) $(CFLAGS) -c -o $@ $<

#------------------------------------------------
# 实用工具目标
#------------------------------------------------

# 清理编译文件
clean:
	@echo "清理编译文件..."
	@rm -f $(SRC_DIR)/*.o
	@rm -f $(BIN_DIR)/*
	@echo "清理完成！"

# 安装（复制到系统目录，需要root权限）
install: all
	@echo "安装解析器工具..."
	@sudo cp $(BIN_DIR)/* /usr/local/bin/
	@echo "安装完成！"

# 代码检查
check:
	@echo "运行代码检查..."
	@cppcheck --enable=all --suppress=missingIncludeSystem $(SRC_DIR)/ 2>/dev/null || echo "cppcheck未安装，跳过静态检查"
	@echo "编译测试..."
	@$(CC) $(CFLAGS) -fsyntax-only $(SRC_DIR)/*.c
	@echo "代码检查完成！"

# 生成文档（需要doxygen）
docs:
	@echo "生成API文档..."
	@doxygen Doxyfile 2>/dev/null || echo "doxygen未安装，请手动查看头文件中的文档"
	@echo "文档生成完成！"

#------------------------------------------------
# 帮助信息
#------------------------------------------------

help:
	@echo ""
	@echo "=================================="
	@echo "  消息解析器项目编译系统帮助"
	@echo "=================================="
	@echo ""
	@echo "主要目标："
	@echo "  all              - 编译所有程序"
	@echo "  clean            - 清理所有编译文件"
	@echo "  help             - 显示此帮助信息"
	@echo ""
	@echo "SEMP协议解析器（单协议）："
	@echo "  semp-demo        - SEMP协议演示程序"
	@echo "  semp-test        - SEMP协议测试工具"
	@echo "  semp-example     - SEMP协议使用示例"
	@echo ""
	@echo "模块化消息解析器（多协议）："
	@echo "  mp-demo          - 多协议演示程序"
	@echo "  mp-example       - 多协议使用示例"
	@echo ""
	@echo "兼容性别名："
	@echo "  demo             - 等同于 semp-demo"
	@echo "  test             - 等同于 semp-test"
	@echo "  example          - 等同于 semp-example"
	@echo "  multi_demo       - 等同于 mp-demo"
	@echo "  multi_example    - 等同于 mp-example"
	@echo ""
	@echo "实用工具："
	@echo "  install          - 安装到系统（需要sudo）"
	@echo "  check            - 运行代码检查"
	@echo "  docs             - 生成API文档"
	@echo ""
	@echo "支持的协议："
	@echo "  - BT/SEMP        - 蓝牙/SEMP协议（0xAA 0x44 0x18）"
	@echo "  - NMEA           - NMEA GPS协议（$）"
	@echo "  - u-blox         - u-blox二进制协议（0xB5 0x62）"
	@echo "  - RTCM           - RTCM差分GPS协议（0xD3）"
	@echo "  - Unicore-Bin    - 中海达二进制协议（0xAA 0x44 0x12）"
	@echo "  - Unicore-Hash   - 中海达Hash协议（#）"
	@echo ""
	@echo "使用示例："
	@echo "  make all         - 编译所有程序"
	@echo "  make mp-demo     - 编译多协议演示程序"
	@echo "  make semp-test   - 编译SEMP测试工具"
	@echo "  make clean       - 清理编译文件"
	@echo ""
	@echo "运行示例："
	@echo "  ./bin/semp_demo                  - 运行SEMP演示"
	@echo "  ./bin/message_parser_demo        - 运行多协议演示"
	@echo "  ./bin/semp_test [hex_data]       - 测试SEMP数据包"
	@echo ""
	@echo "项目结构："
	@echo "  src/             - 源代码目录"
	@echo "  bin/             - 编译输出目录"
	@echo "  lib/             - 参考实现库"
	@echo "  README.md        - SEMP协议文档"
	@echo "  MULTI_PROTOCOL_README.md - 多协议文档"
	@echo "  PROJECT_STRUCTURE.md     - 项目结构说明"
	@echo ""

#------------------------------------------------
# 依赖关系
#------------------------------------------------

# SEMP解析器依赖
$(SRC_DIR)/main.o: $(SRC_DIR)/semp_parser.h
$(SRC_DIR)/test_tool.o: $(SRC_DIR)/semp_parser.h
$(SRC_DIR)/semp_example.o: $(SRC_DIR)/semp_parser.h
$(SRC_DIR)/semp_parser.o: $(SRC_DIR)/semp_parser.h

# 消息解析器核心依赖
$(SRC_DIR)/Message_Parser.o: $(SRC_DIR)/Message_Parser.h

# 协议解析器依赖
$(SRC_DIR)/Parse_BT.o: $(SRC_DIR)/Message_Parser.h
$(SRC_DIR)/Parse_NMEA.o: $(SRC_DIR)/Message_Parser.h
$(SRC_DIR)/Parse_UBLOX.o: $(SRC_DIR)/Message_Parser.h
$(SRC_DIR)/Parse_RTCM.o: $(SRC_DIR)/Message_Parser.h
$(SRC_DIR)/Parse_Unicore_Binary.o: $(SRC_DIR)/Message_Parser.h
$(SRC_DIR)/Parse_Unicore_Hash.o: $(SRC_DIR)/Message_Parser.h

# 示例程序依赖
$(SRC_DIR)/message_parser_demo.o: $(SRC_DIR)/Message_Parser.h
$(SRC_DIR)/message_parser_example.o: $(SRC_DIR)/Message_Parser.h 