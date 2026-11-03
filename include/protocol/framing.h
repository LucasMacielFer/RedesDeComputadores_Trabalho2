#pragma once
#include <cstddef>
#include <cstdint>
#include "network/tcpSocket.h"
#include "protocol/protocol.h"

namespace Protocol
{
    constexpr size_t HEADER_SIZE = 9;

    bool sendMessage(Network::TcpSocket& socket, ApplicationCode code, const uint8_t* payload, size_t length);
    bool receiveMessage(Network::TcpSocket& socket, Message& message);
}
