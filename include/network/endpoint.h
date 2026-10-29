#pragma once
#include <cstdint>

namespace Network
{
    struct Endpoint
    {
        uint8_t ip[16];
        uint16_t port;
        bool isIpv6;
    };
}