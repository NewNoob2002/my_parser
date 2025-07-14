/**
 * @file Parse_UBLOX.h
 * @brief u-blox UBX协议解析器 - 头文件
 * @version 1.0
 * @date 2024-12
 */

#ifndef PARSE_UBLOX_H
#define PARSE_UBLOX_H

#include "Message_Parser.h"

#ifdef __cplusplus
extern "C" {
#endif

//----------------------------------------
// u-blox解析器前导函数
//----------------------------------------

/**
 * @brief 检查传入字节是否为u-blox UBX消息的第一个同步字符 0xB5
 * 
 * @param parse 解析器状态结构体指针
 * @param data 传入的字节数据
 * @return 如果是同步字符则返回true，否则返回false
 */
bool sempUbloxPreamble(SEMP_PARSE_STATE *parse, uint8_t data);

#ifdef __cplusplus
}
#endif

#endif // PARSE_UBLOX_H 