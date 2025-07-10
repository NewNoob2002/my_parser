/**
 * @file multi_protocol_parser.c
 * @brief 多协议解析器核心实现
 * @details 支持SEMP、NMEA、u-blox、RTCM等多种通信协议的统一解析框架
 */

#include "multi_protocol_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

//----------------------------------------
// CRC32查找表
//----------------------------------------

static const uint32_t mmp_crc32Table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
    0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
    0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
    0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
    0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
    0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
    0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
    0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
    0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
    0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
    0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
    0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
    0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
    0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
    0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
    0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
    0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
    0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693,
    0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

//----------------------------------------
// 协议名称表
//----------------------------------------

static const char* mmp_protocolNames[MMP_PROTOCOL_COUNT] = {
    "SEMP",
    "NMEA", 
    "u-blox",
    "RTCM",
    "SPARTN",
    "SBF",
    "Unicore-Hash",
    "Unicore-Binary"
};

//----------------------------------------
// 前向声明
//----------------------------------------

static bool mmpFirstByte(MMP_PARSE_STATE *parse, uint8_t data);

//----------------------------------------
// 实用工具函数
//----------------------------------------

// ASCII转半字节
int mmpAsciiToNibble(int data)
{
    // 转换为小写
    data |= 0x20;
    if ((data >= 'a') && (data <= 'f'))
        return data - 'a' + 10;
    if ((data >= '0') && (data <= '9'))
        return data - '0';
    return -1;
}

// 安全的printf包装
void mmpSafePrintf(MMP_PRINTF_CALLBACK callback, const char *format, ...)
{
    if (callback) {
        va_list args;
        va_start(args, format);
        
        // 创建一个足够大的缓冲区
        char buffer[512];
        vsnprintf(buffer, sizeof(buffer), format, args);
        callback("%s", buffer);
        
        va_end(args);
    }
}

//----------------------------------------
// SEMP协议解析器实现
//----------------------------------------

// SEMP CRC32计算
static uint32_t mmpSempComputeCrc(MMP_PARSE_STATE *parse, uint8_t data)
{
    uint32_t crc = parse->crc;
    crc = mmp_crc32Table[(crc ^ data) & 0xff] ^ (crc >> 8);
    return crc;
}

// SEMP读取CRC
static bool mmpSempReadCrc(MMP_PARSE_STATE *parse, uint8_t data)
{
    MMP_SCRATCH_PAD *scratchPad = (MMP_SCRATCH_PAD *)parse->scratchPad;

    // 检查是否读取完所有CRC字节
    if (--scratchPad->semp.bytesRemaining) {
        return true; // 还需要更多字节
    }

    // 提取接收到的CRC
    uint32_t crcRead = 0;
    if (parse->length >= 4) {
        crcRead = (uint32_t)parse->buffer[parse->length - 4] |
                  ((uint32_t)parse->buffer[parse->length - 3] << 8) |
                  ((uint32_t)parse->buffer[parse->length - 2] << 16) |
                  ((uint32_t)parse->buffer[parse->length - 1] << 24);
    }

    uint32_t crcComputed = scratchPad->semp.crc;
    
    // 验证CRC
    if (crcRead == crcComputed) {
        // CRC正确，调用消息结束回调
        parse->messagesProcessed[MMP_PROTOCOL_SEMP]++;
        if (parse->eomCallback) {
            parse->eomCallback(parse, MMP_PROTOCOL_SEMP);
        }
    } else {
        // CRC错误
        parse->crcErrors[MMP_PROTOCOL_SEMP]++;
        if ((!parse->badCrc) || (!parse->badCrc(parse))) {
            mmpSafePrintf(parse->printDebug,
                "MMP: %s SEMP CRC错误, 接收: %02X %02X %02X %02X, 计算: %02X %02X %02X %02X",
                parse->parserName ? parse->parserName : "Unknown",
                parse->buffer[parse->length - 4], parse->buffer[parse->length - 3],
                parse->buffer[parse->length - 2], parse->buffer[parse->length - 1],
                crcComputed & 0xff, (crcComputed >> 8) & 0xff,
                (crcComputed >> 16) & 0xff, (crcComputed >> 24) & 0xff);
        }
    }
    
    // 重置解析器状态
    parse->state = mmpFirstByte;
    return false;
}

// SEMP读取数据
static bool mmpSempReadData(MMP_PARSE_STATE *parse, uint8_t data)
{
    MMP_SCRATCH_PAD *scratchPad = (MMP_SCRATCH_PAD *)parse->scratchPad;

    // 检查是否读取完所有数据
    if (!--scratchPad->semp.bytesRemaining) {
        // 消息数据读取完成，准备读取CRC
        scratchPad->semp.bytesRemaining = 4;
        parse->crc ^= 0xFFFFFFFF;
        scratchPad->semp.crc = parse->crc;
        parse->state = mmpSempReadCrc;
    }
    return true;
}

// SEMP读取头部
static bool mmpSempReadHeader(MMP_PARSE_STATE *parse, uint8_t data)
{
    MMP_SCRATCH_PAD *scratchPad = (MMP_SCRATCH_PAD *)parse->scratchPad;

    if (parse->length >= sizeof(MMP_SEMP_HEADER)) {
        // 头部读取完成，准备读取消息数据
        MMP_SEMP_HEADER *header = (MMP_SEMP_HEADER *)parse->buffer;
        
        // 验证头部长度
        if (header->headerLength != 0x14) {
            mmpSafePrintf(parse->printDebug, "MMP: SEMP无效头部长度: 0x%02X", header->headerLength);
            parse->state = mmpFirstByte;
            return false;
        }
        
        scratchPad->semp.bytesRemaining = header->messageLength;
        
        // 如果没有数据部分，直接读取CRC
        if (scratchPad->semp.bytesRemaining == 0) {
            scratchPad->semp.bytesRemaining = 4;
            parse->crc ^= 0xFFFFFFFF;
            scratchPad->semp.crc = parse->crc;
            parse->state = mmpSempReadCrc;
        } else {
            parse->state = mmpSempReadData;
        }
    }
    return true;
}

// SEMP同步字节3
static bool mmpSempSync3(MMP_PARSE_STATE *parse, uint8_t data)
{
    if (data != 0x18) {
        return mmpFirstByte(parse, data);
    }
    parse->state = mmpSempReadHeader;
    return true;
}

// SEMP同步字节2
static bool mmpSempSync2(MMP_PARSE_STATE *parse, uint8_t data)
{
    if (data != 0x44) {
        return mmpFirstByte(parse, data);
    }
    parse->state = mmpSempSync3;
    return true;
}

// SEMP前导字节检测
bool mmpSempPreamble(MMP_PARSE_STATE *parse, uint8_t data)
{
    if (data != 0xAA) {
        return false;
    }

    // 初始化SEMP解析
    parse->crc = 0xFFFFFFFF;
    parse->computeCrc = mmpSempComputeCrc;
    parse->crc = parse->computeCrc(parse, data);
    parse->state = mmpSempSync2;
    return true;
}

//----------------------------------------
// NMEA协议解析器实现
//----------------------------------------

// NMEA校验和验证
static void mmpNmeaValidateChecksum(MMP_PARSE_STATE *parse)
{
    int checksum;
    MMP_SCRATCH_PAD *scratchPad = (MMP_SCRATCH_PAD *)parse->scratchPad;

    // 转换校验和字符为二进制
    checksum = mmpAsciiToNibble(parse->buffer[parse->length - 2]) << 4;
    checksum |= mmpAsciiToNibble(parse->buffer[parse->length - 1]);

    // 验证校验和
    if ((checksum == (int)(parse->crc & 0xFF)) || (parse->badCrc && (!parse->badCrc(parse)))) {
        // 添加回车换行
        parse->buffer[parse->length++] = '\r';
        parse->buffer[parse->length++] = '\n';
        parse->buffer[parse->length] = 0; // 零终止符

        // 处理NMEA语句
        parse->messagesProcessed[MMP_PROTOCOL_NMEA]++;
        if (parse->eomCallback) {
            parse->eomCallback(parse, MMP_PROTOCOL_NMEA);
        }
    } else {
        // 校验和错误
        parse->crcErrors[MMP_PROTOCOL_NMEA]++;
        mmpSafePrintf(parse->printDebug,
            "MMP: %s NMEA %s, 校验和错误, 接收: %c%c, 计算: %02X",
            parse->parserName ? parse->parserName : "Unknown",
            scratchPad->nmea.info.sentenceName,
            parse->buffer[parse->length - 2], parse->buffer[parse->length - 1],
            (uint8_t)(parse->crc & 0xFF));
    }
}

// NMEA行终止符处理
static bool mmpNmeaLineTermination(MMP_PARSE_STATE *parse, uint8_t data)
{
    // 不添加当前字符到长度
    parse->length -= 1;

    // 处理行终止符
    if (data == '\r' || data == '\n') {
        mmpNmeaValidateChecksum(parse);
        parse->state = mmpFirstByte;
        return true;
    }

    // 无效终止符
    mmpNmeaValidateChecksum(parse);
    return mmpFirstByte(parse, data);
}

// NMEA第二个校验和字节
static bool mmpNmeaChecksumByte2(MMP_PARSE_STATE *parse, uint8_t data)
{
    if (mmpAsciiToNibble(parse->buffer[parse->length - 1]) >= 0) {
        parse->state = mmpNmeaLineTermination;
        return true;
    }

    mmpSafePrintf(parse->printDebug, "MMP: %s NMEA无效的第二个校验和字符", 
                  parse->parserName ? parse->parserName : "Unknown");
    return mmpFirstByte(parse, data);
}

// NMEA第一个校验和字节
static bool mmpNmeaChecksumByte1(MMP_PARSE_STATE *parse, uint8_t data)
{
    if (mmpAsciiToNibble(parse->buffer[parse->length - 1]) >= 0) {
        parse->state = mmpNmeaChecksumByte2;
        return true;
    }

    mmpSafePrintf(parse->printDebug, "MMP: %s NMEA无效的第一个校验和字符",
                  parse->parserName ? parse->parserName : "Unknown");
    return mmpFirstByte(parse, data);
}

// NMEA查找星号
static bool mmpNmeaFindAsterisk(MMP_PARSE_STATE *parse, uint8_t data)
{
    if (data == '*') {
        parse->state = mmpNmeaChecksumByte1;
    } else {
        // 包含在校验和中
        parse->crc ^= data;

        // 检查缓冲区空间
        if ((uint32_t)(parse->length + 5) > parse->bufferLength) { // 5 = *, XX, \r, \n, \0
            mmpSafePrintf(parse->printDebug,
                "MMP: %s NMEA语句过长, 增加缓冲区大小 > %d",
                parse->parserName ? parse->parserName : "Unknown", parse->bufferLength);
            return mmpFirstByte(parse, data);
        }
    }
    return true;
}

// NMEA查找第一个逗号
static bool mmpNmeaFindFirstComma(MMP_PARSE_STATE *parse, uint8_t data)
{
    MMP_SCRATCH_PAD *scratchPad = (MMP_SCRATCH_PAD *)parse->scratchPad;
    parse->crc ^= data;
    
    if ((data != ',') || (scratchPad->nmea.info.sentenceNameLength == 0)) {
        // 验证字符有效性
        uint8_t upper = data & ~0x20;
        if (((upper < 'A') || (upper > 'Z')) && ((data < '0') || (data > '9'))) {
            mmpSafePrintf(parse->printDebug,
                "MMP: %s NMEA无效语句名字符 0x%02X",
                parse->parserName ? parse->parserName : "Unknown", data);
            return mmpFirstByte(parse, data);
        }

        // 名称过长检查
        if (scratchPad->nmea.info.sentenceNameLength >= (MMP_MAX_SENTENCE_NAME - 1)) {
            mmpSafePrintf(parse->printDebug,
                "MMP: %s NMEA语句名 > %d 字符",
                parse->parserName ? parse->parserName : "Unknown", MMP_MAX_SENTENCE_NAME - 1);
            return mmpFirstByte(parse, data);
        }

        // 保存语句名
        scratchPad->nmea.info.sentenceName[scratchPad->nmea.info.sentenceNameLength++] = data;
    } else {
        // 零终止语句名
        scratchPad->nmea.info.sentenceName[scratchPad->nmea.info.sentenceNameLength] = 0;
        parse->state = mmpNmeaFindAsterisk;
    }
    return true;
}

// NMEA前导字节检测
bool mmpNmeaPreamble(MMP_PARSE_STATE *parse, uint8_t data)
{
    if (data != '$') {
        return false;
    }
    
    MMP_SCRATCH_PAD *scratchPad = (MMP_SCRATCH_PAD *)parse->scratchPad;
    scratchPad->nmea.info.sentenceNameLength = 0;
    parse->crc = 0; // NMEA校验和从0开始
    parse->computeCrc = NULL; // NMEA使用简单异或
    parse->state = mmpNmeaFindFirstComma;
    return true;
}

//----------------------------------------
// 简化的u-blox协议解析器实现
//----------------------------------------

// u-blox校验和计算
static void mmpUbloxComputeChecksum(MMP_PARSE_STATE *parse, uint8_t data)
{
    MMP_SCRATCH_PAD *scratchPad = (MMP_SCRATCH_PAD *)parse->scratchPad;
    scratchPad->ublox.checksumA += data;
    scratchPad->ublox.checksumB += scratchPad->ublox.checksumA;
}

// u-blox校验和B
static bool mmpUbloxChecksumB(MMP_PARSE_STATE *parse, uint8_t data)
{
    MMP_SCRATCH_PAD *scratchPad = (MMP_SCRATCH_PAD *)parse->scratchPad;
    
    if (data == scratchPad->ublox.checksumB) {
        // 校验和正确
        parse->messagesProcessed[MMP_PROTOCOL_UBLOX]++;
        if (parse->eomCallback) {
            parse->eomCallback(parse, MMP_PROTOCOL_UBLOX);
        }
    } else {
        // 校验和错误
        parse->crcErrors[MMP_PROTOCOL_UBLOX]++;
        mmpSafePrintf(parse->printDebug,
            "MMP: %s u-blox校验和B错误, 接收: %02X, 计算: %02X",
            parse->parserName ? parse->parserName : "Unknown", data, scratchPad->ublox.checksumB);
    }
    
    parse->state = mmpFirstByte;
    return false;
}

// u-blox校验和A
static bool mmpUbloxChecksumA(MMP_PARSE_STATE *parse, uint8_t data)
{
    MMP_SCRATCH_PAD *scratchPad = (MMP_SCRATCH_PAD *)parse->scratchPad;
    
    if (data == scratchPad->ublox.checksumA) {
        parse->state = mmpUbloxChecksumB;
        return true;
    } else {
        // 校验和错误
        parse->crcErrors[MMP_PROTOCOL_UBLOX]++;
        mmpSafePrintf(parse->printDebug,
            "MMP: %s u-blox校验和A错误, 接收: %02X, 计算: %02X",
            parse->parserName ? parse->parserName : "Unknown", data, scratchPad->ublox.checksumA);
        parse->state = mmpFirstByte;
        return false;
    }
}

// u-blox读取数据
static bool mmpUbloxReadData(MMP_PARSE_STATE *parse, uint8_t data)
{
    MMP_SCRATCH_PAD *scratchPad = (MMP_SCRATCH_PAD *)parse->scratchPad;
    
    mmpUbloxComputeChecksum(parse, data);
    
    if (!--scratchPad->ublox.bytesRemaining) {
        parse->state = mmpUbloxChecksumA;
    }
    return true;
}

// u-blox读取长度
static bool mmpUbloxReadLength(MMP_PARSE_STATE *parse, uint8_t data)
{
    static uint8_t lengthBytes = 0;
    MMP_SCRATCH_PAD *scratchPad = (MMP_SCRATCH_PAD *)parse->scratchPad;
    
    mmpUbloxComputeChecksum(parse, data);
    
    if (lengthBytes == 0) {
        // 低字节
        scratchPad->ublox.bytesRemaining = data;
        lengthBytes = 1;
    } else {
        // 高字节
        scratchPad->ublox.bytesRemaining |= (data << 8);
        lengthBytes = 0;
        
        if (scratchPad->ublox.bytesRemaining == 0) {
            parse->state = mmpUbloxChecksumA;
        } else {
            parse->state = mmpUbloxReadData;
        }
    }
    return true;
}

// u-blox读取消息ID
static bool mmpUbloxReadMessageId(MMP_PARSE_STATE *parse, uint8_t data)
{
    MMP_SCRATCH_PAD *scratchPad = (MMP_SCRATCH_PAD *)parse->scratchPad;
    
    // 重置校验和并开始计算
    scratchPad->ublox.checksumA = 0;
    scratchPad->ublox.checksumB = 0;
    
    // 从类别开始计算校验和
    mmpUbloxComputeChecksum(parse, parse->buffer[parse->length - 2]); // 类别
    mmpUbloxComputeChecksum(parse, data); // 消息ID
    
    parse->state = mmpUbloxReadLength;
    return true;
}

// u-blox读取消息类别
static bool mmpUbloxReadClass(MMP_PARSE_STATE *parse, uint8_t data)
{
    parse->state = mmpUbloxReadMessageId;
    return true;
}

// u-blox同步字节2
static bool mmpUbloxSync2(MMP_PARSE_STATE *parse, uint8_t data)
{
    if (data != 0x62) {
        return mmpFirstByte(parse, data);
    }
    parse->state = mmpUbloxReadClass;
    return true;
}

// u-blox前导字节检测
bool mmpUbloxPreamble(MMP_PARSE_STATE *parse, uint8_t data)
{
    if (data != 0xB5) {
        return false;
    }
    
    parse->computeCrc = NULL; // u-blox使用自己的校验和
    parse->state = mmpUbloxSync2;
    return true;
}

//----------------------------------------
// RTCM协议解析器（简化实现）
//----------------------------------------

bool mmpRtcmPreamble(MMP_PARSE_STATE *parse, uint8_t data)
{
    if (data != 0xD3) {
        return false;
    }
    
    // RTCM的详细实现留作扩展
    mmpSafePrintf(parse->printDebug, "MMP: RTCM协议检测到但未完全实现");
    parse->state = mmpFirstByte;
    return false;
}

//----------------------------------------
// 核心框架实现
//----------------------------------------

// 解析器表
static const MMP_PARSE_ROUTINE mmp_parserTable[] = {
    mmpSempPreamble,    // SEMP协议
    mmpNmeaPreamble,    // NMEA协议  
    mmpUbloxPreamble,   // u-blox协议
    mmpRtcmPreamble,    // RTCM协议
    // 可以添加更多协议...
};

static const char* const mmp_parserNameTable[] = {
    "SEMP",
    "NMEA",
    "u-blox", 
    "RTCM",
    // 对应协议名称...
};

// 查找第一个字节
static bool mmpFirstByte(MMP_PARSE_STATE *parse, uint8_t data)
{
    if (!parse) return false;

    // 重置状态
    parse->crc = 0;
    parse->computeCrc = NULL;
    parse->length = 0;
    parse->type = MMP_PROTOCOL_COUNT; // 表示未识别
    parse->buffer[parse->length++] = data;
    parse->totalBytes++;

    // 遍历解析器表
    for (int index = 0; index < (int)(sizeof(mmp_parserTable)/sizeof(mmp_parserTable[0])); index++) {
        if (mmp_parserTable[index](parse, data)) {
            parse->type = index;
            return true;
        }
    }

    // 未找到匹配的协议前导
    parse->state = mmpFirstByte;
    return false;
}

//----------------------------------------
// 公共API实现
//----------------------------------------

bool mmpInitParser(MMP_PARSE_STATE *parse,
                   uint8_t *buffer,
                   uint16_t bufferLength,
                   MMP_EOM_CALLBACK eomCallback,
                   MMP_BAD_CRC_CALLBACK badCrcCallback,
                   const char *parserName,
                   MMP_PRINTF_CALLBACK printError,
                   MMP_PRINTF_CALLBACK printDebug)
{
    if (!parse || !buffer || !eomCallback || bufferLength < MMP_MINIMUM_BUFFER_LENGTH) {
        return false;
    }

    // 初始化解析器状态
    memset(parse, 0, sizeof(MMP_PARSE_STATE));
    
    // 分配临时数据区
    static MMP_SCRATCH_PAD scratchPad; // 使用静态分配
    parse->scratchPad = &scratchPad;
    
    // 设置配置
    parse->parsers = mmp_parserTable;
    parse->parserNames = mmp_parserNameTable;
    parse->parserCount = sizeof(mmp_parserTable)/sizeof(mmp_parserTable[0]);
    parse->parserName = parserName;
    
    // 设置回调
    parse->eomCallback = eomCallback;
    parse->badCrc = badCrcCallback;
    parse->printError = printError;
    parse->printDebug = printDebug;
    
    // 设置缓冲区
    parse->buffer = buffer;
    parse->bufferLength = bufferLength;
    
    // 设置初始状态
    parse->state = mmpFirstByte;
    parse->type = MMP_PROTOCOL_COUNT;
    
    return true;
}

bool mmpProcessByte(MMP_PARSE_STATE *parse, uint8_t data)
{
    if (!parse) return false;

    // 检查缓冲区空间
    if (parse->length >= parse->bufferLength) {
        mmpSafePrintf(parse->printError,
            "MMP %s: 消息过长, 增加缓冲区大小 > %d",
            parse->parserName ? parse->parserName : "Unknown", parse->bufferLength);
        return mmpFirstByte(parse, data);
    }

    // 保存数据字节
    parse->buffer[parse->length++] = data;
    parse->totalBytes++;

    // 计算CRC（如果需要）
    if (parse->computeCrc) {
        parse->crc = parse->computeCrc(parse, data);
    }

    // 调用状态处理函数
    return parse->state(parse, data);
}

uint16_t mmpProcessBuffer(MMP_PARSE_STATE *parse, uint8_t *data, uint16_t length)
{
    uint16_t processed = 0;

    if (!parse || !data) return 0;

    for (uint16_t i = 0; i < length; i++) {
        if (mmpProcessByte(parse, data[i])) {
            processed++;
        } else {
            // 消息处理完成或发生错误
            processed++;
            break;
        }
    }

    return processed;
}

MMP_PROTOCOL_TYPE mmpGetActiveProtocol(const MMP_PARSE_STATE *parse)
{
    if (!parse || parse->type >= MMP_PROTOCOL_COUNT) {
        return MMP_PROTOCOL_COUNT; // 未知协议
    }
    return (MMP_PROTOCOL_TYPE)parse->type;
}

const char* mmpGetProtocolName(MMP_PROTOCOL_TYPE protocolType)
{
    if (protocolType >= MMP_PROTOCOL_COUNT) {
        return "Unknown";
    }
    return mmp_protocolNames[protocolType];
}

uint16_t mmpGetProtocolStats(const MMP_PARSE_STATE *parse, 
                            MMP_PROTOCOL_STATS *stats, 
                            uint16_t maxCount)
{
    if (!parse || !stats) return 0;

    uint16_t count = 0;
    for (int i = 0; i < MMP_PROTOCOL_COUNT && count < maxCount; i++) {
        if (parse->messagesProcessed[i] > 0 || parse->crcErrors[i] > 0) {
            stats[count].protocolName = mmp_protocolNames[i];
            stats[count].messagesProcessed = parse->messagesProcessed[i];
            stats[count].crcErrors = parse->crcErrors[i];
            
            uint32_t total = stats[count].messagesProcessed + stats[count].crcErrors;
            stats[count].successRate = total > 0 ? 
                (float)stats[count].messagesProcessed / total * 100.0f : 0.0f;
            count++;
        }
    }
    return count;
}

void mmpResetStats(MMP_PARSE_STATE *parse)
{
    if (parse) {
        memset(parse->messagesProcessed, 0, sizeof(parse->messagesProcessed));
        memset(parse->crcErrors, 0, sizeof(parse->crcErrors));
        parse->totalBytes = 0;
    }
}

void mmpPrintStats(const MMP_PARSE_STATE *parse)
{
    if (!parse || !parse->printDebug) return;

    mmpSafePrintf(parse->printDebug, "=== 多协议解析器统计 ===");
    mmpSafePrintf(parse->printDebug, "总处理字节数: %u", parse->totalBytes);
    
    bool hasData = false;
    for (int i = 0; i < MMP_PROTOCOL_COUNT; i++) {
        if (parse->messagesProcessed[i] > 0 || parse->crcErrors[i] > 0) {
            uint32_t total = parse->messagesProcessed[i] + parse->crcErrors[i];
            float successRate = total > 0 ? 
                (float)parse->messagesProcessed[i] / total * 100.0f : 0.0f;
            
            mmpSafePrintf(parse->printDebug, "%s: 成功=%u, 错误=%u, 成功率=%.1f%%",
                mmp_protocolNames[i], parse->messagesProcessed[i], 
                parse->crcErrors[i], successRate);
            hasData = true;
        }
    }
    
    if (!hasData) {
        mmpSafePrintf(parse->printDebug, "暂无协议数据处理记录");
    }
    mmpSafePrintf(parse->printDebug, "=====================");
} 