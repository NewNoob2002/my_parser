/**
 * @file Parse_Unicore_Binary.h
 * @brief 和芯星通Unicore二进制协议解析器 - 头文件
 * @version 1.0
 * @date 2024-12
 */

#ifndef PARSE_UNICORE_BINARY_H
#define PARSE_UNICORE_BINARY_H

#include "Message_Parser.h"

#ifdef __cplusplus
extern "C" {
#endif

//----------------------------------------
// 和芯星通二进制协议解析器前导函数
//----------------------------------------

/**
 * @brief 检查传入字节是否为和芯星通二进制消息的第一个同步字符 0xAA
 * 
 * @param parse 解析器状态结构体指针
 * @param data 传入的字节数据
 * @return 如果是同步字符则返回true，否则返回false
 */
bool sempUnicoreBinaryPreamble(SEMP_PARSE_STATE *parse, uint8_t data);

#ifdef __cplusplus
}
#endif

#endif // PARSE_UNICORE_BINARY_H 