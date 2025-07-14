/**
 * @file Message_Parser.c
 * @brief 统一消息解析器框架 - 主体解析器实现
 * @details 管理多种协议解析器，提供统一的API接口和协议识别机制
 * @version 1.0
 * @date 2024-12
 */

#include "Message_Parser.h"

//----------------------------------------
// 协议信息函数
//----------------------------------------

const char* semp_getProtocolName(const SEMP_PARSE_STATE *parse, uint16_t protocolIndex)
{
    if (!parse || protocolIndex >= parse->parsers_count) {
        return "Unknown";
    }
    return parse->parserNames_table[protocolIndex];
}

const char* semp_getProtocolDescription(uint16_t protocolIndex)
{
    // This function is no longer supported as the description has been removed.
    (void)protocolIndex;
    return "Description Removed";
}

//----------------------------------------
// 主状态机
//----------------------------------------
SEMP_PARSE_STATE * sempBeginParser(
    const char *parserName, \
    const SEMP_PARSE_ROUTINE *parsersTable, \
    uint8_t parsersCount, \
    const char * const *parserNamesTable, \
    uint8_t parserNamesCount, \
    uint16_t scratchPadBytes, \
    uint16_t bufferLength, \
    SEMP_EOM_CALLBACK eomCallback, \
    SEMP_PRINTF_CALLBACK printError, \
    SEMP_PRINTF_CALLBACK printDebug, \
    SEMP_BAD_CRC_CALLBACK badCrcCallback)
{
    SEMP_PARSE_STATE *parse = NULL;

    do
    {
        // Validate the parse type names table
        if (parsersCount != parserNamesCount)
        {
            sempPrintln(printError, "SEMP: Please fix parserTable and parserNameTable parserCount != parserNameCount");
            break;
        }

        // Validate the parserTable address is not nullptr
        if (!parsersTable)
        {
            sempPrintln(printError, "SEMP: Please specify a parserTable data structure");
            break;
        }

        // Validate the parserNameTable address is not nullptr
        if (!parserNamesTable)
        {
            sempPrintln(printError, "SEMP: Please specify a parserNameTable data structure");
            break;
        }

        // Validate the end-of-message callback routine address is not nullptr
        if (!eomCallback)
        {
            sempPrintln(printError, "SEMP: Please specify an eomCallback routine");
            break;
        }

        // Verify the parser name
        if ((!parserName) || (!strlen(parserName)))
        {
            sempPrintln(printError, "SEMP: Please provide a name for the parser");
            break;
        }

        // Verify that there is at least one parser in the table
        if (!parsersCount)
        {
            sempPrintln(printError, "SEMP: Please provide at least one parser in parserTable");
            break;
        }

        // Validate the parser address is not nullptr
        parse = sempAllocateParseStructure(printDebug, scratchPadBytes, bufferLength);
        if (!parse)
        {
            sempPrintln(printError, "SEMP: Failed to allocate the parse structure");
            break;
        }

        // Initialize the parser
        parse->printError = printError;
        parse->parsers_table = parsersTable;
        parse->parsers_count = parsersCount;
        parse->parserNames_table = parserNamesTable;
        parse->state = sempFirstByte;
        parse->eomCallback = eomCallback;
        parse->parserName = parserName;
        parse->badCrc = badCrcCallback;

        // Display the parser configuration
        sempPrintParserConfiguration(parse, parse->printDebug);
    } while (0);

    // Return the parse structure address
    return parse;
}

bool sempFirstByte(SEMP_PARSE_STATE *parse, uint8_t data)
{
    uint8_t index;
    SEMP_PARSE_ROUTINE parseRoutine;

    if (parse)
    {
        // Add this byte to the buffer
        parse->crc = 0;
        parse->computeCrc = nullptr;
        parse->msg_length = 0;
        parse->parser_type = parse->parsers_count;
        parse->buffer[parse->msg_length++] = data;

        // Walk through the parse table
        for (index = 0; index < parse->parsers_count; index++)
        {
            parseRoutine = parse->parsers_table[index];
            if (parseRoutine(parse, data))
            {
                parse->parser_type = index;
                return true;
            }
        }

        // Preamble byte not found, continue searching for a preamble byte
        parse->state = sempFirstByte;
    }
    return false;
}

// Parse the next byte
void sempParseNextByte(SEMP_PARSE_STATE *parse, uint8_t data)
{
    if (parse)
    {
        // Verify that enough space exists in the buffer
        if (parse->msg_length >= parse->buffer_length)
        {
            // Message too long
            sempPrintf(parse->printError, "SEMP %s: Message too long, increase the buffer size > %d\r\n",
                       parse->parserName,
                       parse->buffer_length);

            // Start searching for a preamble byte
            sempFirstByte(parse, data);
            return;
        }

        // Save the data byte
        parse->buffer[parse->msg_length++] = data;

        // Compute the CRC value for the message
        if (parse->computeCrc)
            parse->crc = parse->computeCrc(parse, data);

        // Update the parser state based on the incoming byte
        parse->state(parse, data);
    }
}

// Shutdown the parser
void sempStopParser(SEMP_PARSE_STATE **parse)
{
    // Free the parse structure if it was specified
    if (parse && *parse)
    {
        semp_util_free(*parse);
        *parse = nullptr;
    }
}

//----------------------------------------
// 工具函数实现
//----------------------------------------
// Allocate the parse structure
/**
 * @brief 分配解析结构体
 * @param printDebug 调试输出回调函数
 * @param scratchPadBytes 解析器scratch区域大小
 * @param bufferLength 缓冲区大小
 * @return 解析结构体指针
 */
SEMP_PARSE_STATE * sempAllocateParseStructure(
    SEMP_PRINTF_CALLBACK printDebug,
    uint16_t scratchPadBytes,
    uint16_t bufferLength
    )
{
    int length;
    SEMP_PARSE_STATE *parse = NULL;
    int parseBytes;

    // Print the scratchPad area size
    sempPrintf(printDebug, "scratchPadBytes: 0x%04x (%d) bytes",
               scratchPadBytes, scratchPadBytes);

    // Align the scratch patch area
    if (scratchPadBytes < SEMP_ALIGN(scratchPadBytes))
    {
        scratchPadBytes = SEMP_ALIGN(scratchPadBytes);
        sempPrintf(printDebug,
                   "scratchPadBytes: 0x%04x (%d) bytes after alignment",
                   scratchPadBytes, scratchPadBytes);
    }

    // Determine the minimum length for the scratch pad
    length = SEMP_ALIGN(sizeof(SEMP_SCRATCH_PAD));
    if (scratchPadBytes < length)
    {
        scratchPadBytes = length;
        sempPrintf(printDebug,
                   "scratchPadBytes: 0x%04x (%d) bytes after mimimum size adjustment",
                   scratchPadBytes, scratchPadBytes);
    }
    parseBytes = SEMP_ALIGN(sizeof(SEMP_PARSE_STATE));
    sempPrintf(printDebug, "parseBytes: 0x%04x (%d)", parseBytes, parseBytes);

    // Verify the minimum bufferLength
    if (bufferLength < SEMP_MINIMUM_BUFFER_LENGTH)
    {
        sempPrintf(printDebug,
                   "SEMP: Increasing bufferLength from %ld to %d bytes, minimum size adjustment",
                   bufferLength, SEMP_MINIMUM_BUFFER_LENGTH);
        bufferLength = SEMP_MINIMUM_BUFFER_LENGTH;
    }

    // Allocate the parser
    length = parseBytes + scratchPadBytes;
    parse = (SEMP_PARSE_STATE *)semp_util_malloc(length + bufferLength);
    sempPrintf(printDebug, "parse: %p", (void *)parse);

    // Initialize the parse structure
    if (parse)
    {
        // Zero the parse structure
        memset(parse, 0, length);

        // Set the scratch pad area address
        parse->scratchPad = ((uint8_t *)parse) + parseBytes;
        parse->printDebug = printDebug;
        sempPrintf(parse->printDebug, "parse->scratchPad: %p", parse->scratchPad);

        // Set the buffer address and length
        parse->buffer_length = bufferLength;
        parse->buffer = ((uint8_t *)parse->scratchPad + scratchPadBytes);
        sempPrintf(parse->printDebug, "parse->buffer: %p", parse->buffer);
    }
    return parse;
}

void * semp_util_malloc(size_t size)
{
    return malloc(size);
}

void semp_util_free(void *ptr)
{
    if (ptr) {
        free(ptr);
    }
}

// Translate the type value into an ASCII type name
const char * sempGetTypeName(SEMP_PARSE_STATE *parse, uint16_t type)
{
    const char *name = "Unknown parser";

    if (parse)
    {
        if (type == parse->parsers_count)
            name = "No active parser, scanning for preamble";
        else if (parse->parserNames_table && (type < parse->parsers_count))
            name = parse->parserNames_table[type];
    }
    return name;
}
// Print the parser's configuration
void sempPrintParserConfiguration(SEMP_PARSE_STATE *parse, SEMP_PRINTF_CALLBACK print)
{
    if (print && parse)
    {
        sempPrintln(print, "SparkFun Extensible Message Parser");
        sempPrintf(print, "    Name: %p (%s)", parse->parserName, parse->parserName);
        sempPrintf(print, "    parsers: %p", (void *)parse->parsers_table);
        sempPrintf(print, "    parserNames: %p", (void *)parse->parserNames_table);
        sempPrintf(print, "    parserCount: %d", parse->parsers_count);
        sempPrintf(print, "    printError: %p", parse->printError);
        sempPrintf(print, "    printDebug: %p", parse->printDebug);
        sempPrintf(print, "    Scratch Pad: %p (%ld bytes)",
                   (void *)parse->scratchPad, parse->buffer - (uint8_t *)parse->scratchPad);
        sempPrintf(print, "    computeCrc: %p", (void *)parse->computeCrc);
        sempPrintf(print, "    crc: 0x%08x", parse->crc);
        sempPrintf(print, "    State: %p%s", (void *)parse->state,
                   (parse->state == sempFirstByte) ? " (sempFirstByte)" : "");
        sempPrintf(print, "    EomCallback: %p", (void *)parse->eomCallback);
        sempPrintf(print, "    Buffer: %p (%d bytes)",
                   (void *)parse->buffer, parse->buffer_length);
        sempPrintf(print, "    length: %d message bytes", parse->msg_length);
        sempPrintf(print, "    type: %d (%s)", parse->parser_type, sempGetTypeName(parse, parse->parser_type));
    }
}
/**
 * @brief 将ASCII字符转换为半字节
 * @param data 输入字符
 * @return 转换后的半字节值
 */
int semp_util_asciiToNibble(int data)
{
    // 转换为小写
    data |= 0x20;
    if ((data >= 'a') && (data <= 'f'))
        return data - 'a' + 10;
    if ((data >= '0') && (data <= '9'))
        return data - '0';
    return -1;
}

/**
 * @brief 安全打印函数
 * @param callback 回调函数
 * @param format 格式字符串
 * @param ... 可变参数
 */
void sempPrintf(SEMP_PRINTF_CALLBACK callback, const char *format, ...)
{
    if (callback) {
        va_list args;
        va_start(args, format);
        va_list args2;

        va_copy(args2, args);
        char buf[vsnprintf(NULL, 0, format, args) + sizeof("\r\n")];

        vsnprintf(buf, sizeof buf, format, args2);

        // Add CR+LF
        buf[sizeof(buf) - 3] = '\r';
        buf[sizeof(buf) - 2] = '\n';
        buf[sizeof(buf) - 1] = '\0';

        callback("%s", buf);

        va_end(args);
        va_end(args2);
    }
}

void sempPrintln(SEMP_PRINTF_CALLBACK print, const char *string)
{
    if (print) {
        print("%s\r\n", string);
    }
}
/**
 * @brief 禁用调试输出
 * @param parse 解析结构体
 */
void sempDisableDebugOutput(SEMP_PARSE_STATE *parse)
{
    if (parse) {
        parse->printDebug = NULL;
    }
}

/**
 * @brief 启用调试输出
 * @param parse 解析结构体
 * @param print 输出回调函数
 */
void sempEnableDebugOutput(SEMP_PARSE_STATE *parse, SEMP_PRINTF_CALLBACK print)
{
    if (parse) {
        parse->printDebug = print;
    }
}

/**
 * @brief 禁用错误输出
 * @param parse 解析结构体
 */
void sempDisableErrorOutput(SEMP_PARSE_STATE *parse)
{
    if (parse) {
        parse->printError = NULL;
    }
}

/**
 * @brief 启用错误输出
 * @param parse 解析结构体
 * @param print 输出回调函数
 */
void sempEnableErrorOutput(SEMP_PARSE_STATE *parse, SEMP_PRINTF_CALLBACK print)
{
    if (parse) {
        parse->printError = print;
    }
}

/**
 * @brief 将十六进制数据转换为字符串
 * @param data 输入数据
 * @param length 数据长度
 * @param output 输出字符串
 * @param outputSize 输出字符串大小
 * @return 转换后的字符串长度
 */
int semp_util_hexToString(const uint8_t *data, uint16_t length, char *output, uint16_t outputSize)
{
    if (!data || !output || outputSize < 3) return 0;
    
    int pos = 0;
    for (uint16_t i = 0; i < length && pos < (outputSize - 3); i++) {
        pos += snprintf(output + pos, outputSize - pos, "%02X ", data[i]);
    }
    
    if (pos > 0 && output[pos - 1] == ' ') {
        output[pos - 1] = '\0'; // 移除最后一个空格
        pos--;
    }
    
    return pos;
}

/**
 * @brief 计算校验和
 * @param data 输入数据
 * @param length 数据长度
 * @return 校验和值
 */
uint8_t semp_util_calculateChecksum(const uint8_t *data, uint16_t length)
{
    uint8_t checksum = 0;
    for (uint16_t i = 0; i < length; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

/**
 * @brief 解析分隔符字段
 * @param sentence 输入字符串
 * @param fields_void 输出字段
 * @param maxFields 最大字段数
 * @param fieldSize 字段大小
 * @param delimiter 分隔符
 * @param terminator 终止符
 * @return 解析后的字段数
 */
int semp_util_parse_delimited_fields(const char *sentence, void *fields_void, int maxFields, int fieldSize, char delimiter, char terminator)
{
    char (*fields)[fieldSize] = fields_void;
    if (!sentence || !fields || maxFields <= 0) {
        return 0;
    }

    int fieldCount = 0;
    int fieldPos = 0;
    const char *p = sentence;

    // Skip leading non-field characters if necessary (like '$' in NMEA)
    if (*p < 32) p++; 
    
    fields[fieldCount][0] = '\0';

    while(*p && *p != terminator && fieldCount < maxFields) {
        if (*p == delimiter) {
            fields[fieldCount][fieldPos] = '\0';
            fieldCount++;
            if(fieldCount < maxFields) {
                fields[fieldCount][0] = '\0';
                fieldPos = 0;
            }
        } else {
            if (fieldPos < (fieldSize - 1)) {
                fields[fieldCount][fieldPos++] = *p;
            }
        }
        p++;
    }

    if (fieldPos > 0) {
        fields[fieldCount][fieldPos] = '\0';
        fieldCount++;
    }

    return fieldCount;
}

//----------------------------------------
// 内部常量
//----------------------------------------

// CRC32查找表
const unsigned long semp_crc32Table[256] = {
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