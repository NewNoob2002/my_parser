# SEMP协议解析库 Makefile
# 适用于Linux/Windows/macOS

# 编译器设置
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
CPPFLAGS = -Wall -Wextra -std=c++11 -O2
SRCDIR = src
OBJDIR = build
BINDIR = bin

# 创建目录
$(shell mkdir -p $(OBJDIR) $(BINDIR))

# 源文件
SEMP_CORE = $(SRCDIR)/semp_parser.c
SEMP_HEADER = $(SRCDIR)/semp_parser.h
MULTIPROTO_CORE = $(SRCDIR)/multi_protocol_parser.c
MULTIPROTO_HEADER = $(SRCDIR)/multi_protocol_parser.h

# 目标程序
TARGETS = $(BINDIR)/demo $(BINDIR)/test_tool $(BINDIR)/example $(BINDIR)/multi_demo $(BINDIR)/multi_example

# 默认目标
all: $(TARGETS)

# 演示程序（简化版main）
$(BINDIR)/demo: $(SRCDIR)/main.cpp $(SEMP_CORE) $(SEMP_HEADER)
	@echo "编译演示程序..."
	$(CC) $(CPPFLAGS) -o $@ $(SRCDIR)/main.cpp $(SEMP_CORE)
	@echo "✅ 演示程序编译完成: $@"

# 详细测试工具
$(BINDIR)/test_tool: $(SRCDIR)/test_tool.c $(SEMP_CORE) $(SEMP_HEADER)
	@echo "编译测试工具..."
	$(CC) $(CFLAGS) -o $@ $(SRCDIR)/test_tool.c $(SEMP_CORE)
	@echo "✅ 测试工具编译完成: $@"

# 完整示例程序
$(BINDIR)/example: $(SRCDIR)/semp_example.c $(SEMP_CORE) $(SEMP_HEADER)
	@echo "编译示例程序..."
	$(CC) $(CFLAGS) -o $@ $(SRCDIR)/semp_example.c $(SEMP_CORE)
	@echo "✅ 示例程序编译完成: $@"

# 多协议演示程序
$(BINDIR)/multi_demo: $(SRCDIR)/multi_protocol_demo.c $(MULTIPROTO_CORE) $(MULTIPROTO_HEADER)
	@echo "编译多协议演示程序..."
	$(CC) $(CFLAGS) -o $@ $(SRCDIR)/multi_protocol_demo.c $(MULTIPROTO_CORE)
	@echo "✅ 多协议演示程序编译完成: $@"

# 多协议实际应用示例
$(BINDIR)/multi_example: $(SRCDIR)/multi_protocol_example.c $(MULTIPROTO_CORE) $(MULTIPROTO_HEADER)
	@echo "编译多协议应用示例..."
	$(CC) $(CFLAGS) -o $@ $(SRCDIR)/multi_protocol_example.c $(MULTIPROTO_CORE)
	@echo "✅ 多协议应用示例编译完成: $@"

# 静态库（可选）
$(BINDIR)/libsemp.a: $(OBJDIR)/semp_parser.o
	@echo "创建静态库..."
	ar rcs $@ $^
	@echo "✅ 静态库创建完成: $@"

$(OBJDIR)/semp_parser.o: $(SEMP_CORE) $(SEMP_HEADER)
	$(CC) $(CFLAGS) -c -o $@ $(SEMP_CORE)

# 清理
clean:
	@echo "清理编译文件..."
	rm -rf $(OBJDIR) $(BINDIR)
	rm -f *.o *.a semp_parser test_tool
	@echo "✅ 清理完成"

# 测试运行
test: $(BINDIR)/test_tool
	@echo "运行测试工具..."
	./$(BINDIR)/test_tool

# 演示运行
demo: $(BINDIR)/demo
	@echo "运行演示程序..."
	./$(BINDIR)/demo

# 安装头文件和库（可选）
install: $(BINDIR)/libsemp.a
	@echo "安装SEMP库..."
	mkdir -p /usr/local/include/semp
	mkdir -p /usr/local/lib
	cp $(SEMP_HEADER) /usr/local/include/semp/
	cp $(BINDIR)/libsemp.a /usr/local/lib/
	@echo "✅ 安装完成"

# 卸载
uninstall:
	@echo "卸载SEMP库..."
	rm -rf /usr/local/include/semp
	rm -f /usr/local/lib/libsemp.a
	@echo "✅ 卸载完成"

# 生成文档（需要doxygen）
docs:
	@if command -v doxygen >/dev/null 2>&1; then \
		echo "生成API文档..."; \
		doxygen; \
		echo "✅ 文档生成完成"; \
	else \
		echo "❌ 需要安装doxygen来生成文档"; \
	fi

# 代码检查（需要cppcheck）
check:
	@if command -v cppcheck >/dev/null 2>&1; then \
		echo "进行代码检查..."; \
		cppcheck --enable=all --std=c99 $(SRCDIR)/*.c $(SRCDIR)/*.h; \
		echo "✅ 代码检查完成"; \
	else \
		echo "❌ 需要安装cppcheck来进行代码检查"; \
	fi

# 帮助信息
help:
	@echo "SEMP协议解析库 - 可用命令："
	@echo ""
	@echo "  make all          - 编译所有程序"
	@echo "  make demo         - 编译并运行SEMP演示程序"
	@echo "  make test         - 编译并运行测试工具"
	@echo "  make example      - 编译SEMP示例程序"
	@echo "  make multi_demo   - 编译多协议演示程序"
	@echo "  make multi_example- 编译多协议应用示例"
	@echo "  make clean        - 清理编译文件"
	@echo "  make install      - 安装库和头文件"
	@echo "  make uninstall    - 卸载库和头文件"
	@echo "  make docs         - 生成API文档"
	@echo "  make check        - 代码质量检查"
	@echo "  make help         - 显示此帮助信息"
	@echo ""
	@echo "编译输出目录："
	@echo "  $(BINDIR)/       - 可执行文件"
	@echo "  $(OBJDIR)/       - 目标文件"

# 声明伪目标
.PHONY: all clean test demo install uninstall docs check help 