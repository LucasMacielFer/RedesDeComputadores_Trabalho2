#include "protocol/framing.h"
#include <exception>
#include <iostream>

namespace Protocol
{
    namespace
    {
        bool receiveExact(Network::TcpSocket& socket, uint8_t* buffer, size_t length)
        {
            size_t total = 0;
            while (total < length)
            {
                size_t received = 0;
                if (!socket.recv(buffer + total, length - total, received))
                    return false;

                total += received;
            }
            return true;
        }
    }

    bool sendMessage(Network::TcpSocket& socket, ApplicationCode code, const uint8_t* payload, size_t length)
    {
        std::vector<uint8_t> frame(HEADER_SIZE + length);
        frame[0] = code;
        for (int i = 0; i < 8; i++)
            frame[1 + i] = static_cast<uint8_t>(static_cast<uint64_t>(length) >> (56 - 8 * i));

        if (length > 0)
            memcpy(frame.data() + HEADER_SIZE, payload, length);

        return socket.send(frame.data(), frame.size());
    }

    bool receiveMessage(Network::TcpSocket& socket, Message& message)
    {
        uint8_t header[HEADER_SIZE];
        if (!receiveExact(socket, header, HEADER_SIZE))
            return false;

        if (header[0] < REQUEST || header[0] > FILE_HASH)
        {
            std::cerr << "ERRO: Codigo de mensagem desconhecido." << std::endl;
            return false;
        }

        uint64_t length = 0;
        for (int i = 0; i < 8; i++)
            length = (length << 8) | header[1 + i];

        message.appCode = static_cast<ApplicationCode>(header[0]);
        message.payloadLength = length;

        try
        {
            message.payload.assign(static_cast<size_t>(length), 0);
        }
        catch (const std::exception&)
        {
            std::cerr << "ERRO: Nao foi possivel alocar o payload da mensagem." << std::endl;
            return false;
        }

        if (length > 0 && !receiveExact(socket, message.payload.data(), message.payload.size()))
            return false;

        return true;
    }
}
