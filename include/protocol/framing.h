#pragma once
#include <cstddef>
#include <cstdint>
#include "network/tcpSocket.h"
#include "protocol/protocol.h"

namespace Protocol
{
    constexpr size_t CODE_SIZE = 1;
    constexpr size_t PAYLOAD_LENGTH_SIZE = 8;
    constexpr size_t HEADER_SIZE = CODE_SIZE + PAYLOAD_LENGTH_SIZE;

    bool sendMessage(Network::TcpSocket& socket, ApplicationCode code, const uint8_t* payload, size_t length);
    bool receiveMessage(Network::TcpSocket& socket, Message& message);
}
