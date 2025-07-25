#include <stdio.h>
#include "SparkFun_Extensible_Message_Parser.h"


uint32_t sempBluetoothComputeCrc(SEMP_PARSE_STATE *parse, uint8_t data)
{
    uint32_t crc;

    crc = parse->crc;
    crc = semp_crc32Table[(crc ^ data) & 0xff] ^ (crc >> 8);
    return crc;
}
//    |<------------- 20 bytes ------------------> |<----- MsgData ----->|<- 4 bytes ->|
//    |                                            |                    |      |
//    +----------+--------+----------------+---------+----------+-------------+
//    | Synchron   |  HeaderLen  | Message ID&Type | Message     |   CRC-32Q   | 
//    |  24 bits   |   8 bits    |    128 bits     |  n-bits     |   32 bits   |
//    | 0xAA 44 18 |   0x14      |   (in 16bytes)  |             |             |
//    +----------+--------+----------------+---------+----------+-------------+
//    |                                                         |
//    |<------------------------ CRC ------------------------->|

// Read the CRC
bool sempBluetoothReadCrc(SEMP_PARSE_STATE *parse, uint8_t data)
{
    SEMP_SCRATCH_PAD *scratchPad = (SEMP_SCRATCH_PAD *)parse->scratchPad;

    // Determine if the entire message was read
    if (--scratchPad->bluetooth.bytesRemaining)
        // Need more bytes
        return true;

    // uint32_t crcRead = parse->crc;
    
    // uint32_t crcComputed = scratchPad->bluetooth.crc;
    // Call the end-of-message routine with this message
    if ((!parse->crc) || (parse->badCrc && (!parse->badCrc(parse))))
        parse->eomCallback(parse, parse->type); // Pass parser array index
    else
    {
        sempPrintf(parse->printDebug,
                   "SEMP: %s Unicore, bad CRC, "
                   "received %02x %02x %02x %02x, computed: %02x %02x %02x %02x",
                   parse->parserName,
                   parse->buffer[parse->length - 4],
                   parse->buffer[parse->length - 3],
                   parse->buffer[parse->length - 2],
                   parse->buffer[parse->length - 1],
                   scratchPad->bluetooth.crc & 0xff,
                   (scratchPad->bluetooth.crc >> 8) & 0xff,
                   (scratchPad->bluetooth.crc >> 16) & 0xff,
                   (scratchPad->bluetooth.crc >> 24) & 0xff);
    }
    parse->state = sempFirstByte;
    return false;
}

bool sempBluetoothReadData(SEMP_PARSE_STATE *parse, uint8_t data)
{
    SEMP_SCRATCH_PAD *scratchPad = (SEMP_SCRATCH_PAD *)parse->scratchPad;

    // Determine if the entire message was read
    if (!--scratchPad->bluetooth.bytesRemaining)
    {
        // The message data is complete, read the CRC
        scratchPad->bluetooth.bytesRemaining = 4;
        parse->crc ^= 0xFFFFFFFF;
        scratchPad->bluetooth.crc = parse->crc;
        parse->state = sempBluetoothReadCrc;
    }
    return true;
}
//  Header
//
//    Bytes     Field Description
//    -----     ----------------------------------------
//      1       Header Length
//      2       Message ID
//      2       Reserved
//      4       ReservedTime
//      2       Message Length
//      2       Reserved
//      1       Sender
//      1       Message Type
//      1       protocol version
//      1       Reserved


bool sempBluetoothReadHeader(SEMP_PARSE_STATE *parse, uint8_t data)
{
    SEMP_SCRATCH_PAD *scratchPad = (SEMP_SCRATCH_PAD *)parse->scratchPad;

    if (parse->length >= sizeof(SEMP_Bluetooth_HEADER))
    {
        // The header is complete, read the message data next
        SEMP_Bluetooth_HEADER *header = (SEMP_Bluetooth_HEADER *)parse->buffer;
        scratchPad->bluetooth.bytesRemaining = header->messageLength;
        parse->state = sempBluetoothReadData;
    }
    return true;
}

bool sempBluetoothSync3(SEMP_PARSE_STATE *parse, uint8_t data)
{
    // Verify sync byte 2
    if (data != 0x18)
        // Invalid sync byte, start searching for a preamble byte
        return sempFirstByte(parse, data);

    // Look for the last sync byte
    parse->state = sempBluetoothReadHeader;
    return true;
}

bool sempBluetoothSync2(SEMP_PARSE_STATE *parse, uint8_t data)
{
    // Verify sync byte 2
    if (data != 0x44)
        // Invalid sync byte, start searching for a preamble byte
        return sempFirstByte(parse, data);

    // Look for the last sync byte
    parse->state = sempBluetoothSync3;
    return true;
}

bool sempBluetoothPreamble(SEMP_PARSE_STATE *parse, uint8_t data)
{
    // Determine if this is the beginning of a Unicore message
    if (data != 0xAA)
        return false;

    // Look for the second sync byte
    parse->crc = 0xFFFFFFFF;
    parse->computeCrc = sempBluetoothComputeCrc;
    parse->crc = parse->computeCrc(parse, data);
    parse->state = sempBluetoothSync2;
    return true;
}