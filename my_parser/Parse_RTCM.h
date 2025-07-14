/**
 * @file Parse_RTCM.h
 * @brief RTCM协议解析器 - 头文件
 * @version 1.0
 * @date 2024-12
 */

#ifndef PARSE_RTCM_H
#define PARSE_RTCM_H

#include "Message_Parser.h"

#ifdef __cplusplus
extern "C" {
#endif

//----------------------------------------
// RTCM解析器前导函数
//----------------------------------------

/**
 * @brief 检查传入字节是否为RTCM消息的起始符 0xD3
 * 
 * @param parse 解析器状态结构体指针
 * @param data 传入的字节数据
 * @return 如果是起始符则返回true，否则返回false
 */
bool sempRtcmPreamble(SEMP_PARSE_STATE *parse, uint8_t data);

#ifdef __cplusplus
}
#endif

#endif // PARSE_RTCM_H 