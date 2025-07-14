/**
 * @file stress_test.c
 * @brief 解析器压力测试程序
 * @details 从文件读取混合协议数据流，测试解析器的稳定性和准确性
 * @version 1.0
 * @date 2024-12
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../Message_Parser.h"
#include "../Parse_NMEA.h"
#include "../Parse_RTCM.h"
#include "../Parse_UBLOX.h"
#include "../Parse_Unicore_Binary.h"
#include "../Parse_Unicore_Hash.h"
#include "../Parse_BT.h"

#define MAX_PROTOCOL_TYPES 16

//----------------------------------------
// 测试状态
//----------------------------------------
typedef struct {
    long total_bytes_processed;
    long success_counts[MAX_PROTOCOL_TYPES];
} StressTestState;

StressTestState g_stress_state;

//----------------------------------------
// 回调函数
//----------------------------------------
void stressEomCallback(SEMP_PARSE_STATE *parse, uint16_t type) {
    if (type < MAX_PROTOCOL_TYPES) {
        g_stress_state.success_counts[type]++;
    }
}

void stressPrintError(const char *format, ...) {
    // 在压力测试中，我们通常不希望看到满屏的错误信息，除非需要调试
}

//----------------------------------------
// 主函数
//----------------------------------------
int main() {
    printf("=================================\n");
    printf("  解析器压力测试 v1.0\n");
    printf("=================================\n");
    
    // 1. 定义协议解析器表
    const SEMP_PARSE_ROUTINE parsersTable[] = {
        sempNmeaPreamble,
        sempRtcmPreamble,
        sempUbloxPreamble,
        sempUnicoreBinaryPreamble,
        sempUnicoreHashPreamble,
        sempBtPreamble
    };
    const char *parserNamesTable[] = {
        "NMEA", "RTCM3", "u-blox", "Unicore-BIN", "Unicore-HASH", "Custom-BIN"
    };
    const uint8_t parserCount = sizeof(parsersTable) / sizeof(parsersTable[0]);
    if (parserCount > MAX_PROTOCOL_TYPES) {
        printf("错误: 协议数量超过了测试状态数组的最大容量!\n");
        return -1;
    }

    // 2. 初始化主解析器
    SEMP_PARSE_STATE *parser = sempBeginParser(
        "StressTestParser",
        parsersTable, parserCount,
        parserNamesTable, parserCount,
        1024, // scratchPadBytes
        2048, // bufferLength
        stressEomCallback,
        stressPrintError,
        NULL, // printDebug
        NULL // badCrcCallback
    );

    if (!parser) {
        printf("解析器初始化失败!\n");
        return -1;
    }

    // 3. 打开并读取测试数据文件
    FILE *fp = fopen("custom_parser/demo/mixed_data.bin", "rb");
    if (!fp) {
        perror("错误: 无法打开 'mixed_data.bin'");
        sempStopParser(&parser);
        return -1;
    }

    printf("正在处理 'mixed_data.bin'...\n");

    int byte;
    while ((byte = fgetc(fp)) != EOF) {
        sempParseNextByte(parser, (uint8_t)byte);
        g_stress_state.total_bytes_processed++;
    }
    fclose(fp);

    // 4. 打印总结
    printf("\n--- 压力测试总结 ---\n");
    printf("总处理字节数: %ld\n", g_stress_state.total_bytes_processed);
    printf("成功解析的消息统计:\n");
    
    bool any_success = false;
    for (uint8_t i = 0; i < parserCount; i++) {
        if (g_stress_state.success_counts[i] > 0) {
            printf("  - %-15s: %ld 条\n", parserNamesTable[i], g_stress_state.success_counts[i]);
            any_success = true;
        }
    }
    if (!any_success) {
        printf("  (未成功解析任何消息)\n");
    }
    printf("=======================\n");

    // 5. 清理
    sempStopParser(&parser);

    return 0;
} 