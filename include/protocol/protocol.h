#pragma once
#include <cstdint>
#include <vector>

namespace Protocol
{
    enum ApplicationCode : uint8_t
    {
        REQUEST = 0x01,
        METADATA = 0x02,
        DATA = 0x03,
        FILE_ERROR = 0x04,
        CHAT_MSG = 0x05,
        CHAT_BROADCAST = 0x06,
        FILE_HASH = 0x07
    };

    enum FileErrorReason : uint8_t
    {
        NOT_FOUND = 1,
        PERMISSION_DENIED = 2
    };

    struct Message
    {
        ApplicationCode appCode;
        uint64_t payloadLength;
        std::vector<uint8_t> payload;
    };
}