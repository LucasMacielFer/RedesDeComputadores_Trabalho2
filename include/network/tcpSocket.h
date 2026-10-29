#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <chrono>
#include "network/endpoint.h"

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#elif __unix__
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <sys/select.h>
    #include <unistd.h>
#else
    #error "Sistema operacional desconhecido. Este projeto suporta somente Windows e Unix."
#endif

namespace Network
{
    class TcpSocket
    {
    private:
        static int instanceCount;

    #ifdef _WIN32
        SOCKET sock;
    #else
        int sock;
    #endif
        Endpoint localEndpoint;
        Endpoint remoteEndpoint;
        bool socketIsBound;
        bool socketIsListening;
        bool socketIsConnected;

    public:
        TcpSocket(bool ipv6);
        ~TcpSocket();

        TcpSocket(const TcpSocket&) = delete;
        TcpSocket& operator=(const TcpSocket&) = delete;

        void bind(uint16_t port);
        bool listen(int backlog);
        // Aceita uma conexao pendente; retorna false em caso de falha.
        bool accept(TcpSocket& clientSocket);
        bool connect(const Endpoint& destEndpoint);

        bool isBound() const;
        bool isListening() const;
        bool isConnected() const;
        const Endpoint& getRemoteEndpoint() const;

        // Envia todos os bytes (repete ate completar). Retorna false em erro.
        bool send(const uint8_t* data, size_t length);
        // Recebe ate bufferSize bytes. Retorna false em erro ou se o par fechou a conexao (receivedLength == 0).
        bool recv(uint8_t* buffer, size_t bufferSize, size_t& receivedLength);
        bool waitForData(std::chrono::milliseconds timeout) const;

        static void waitForEither(const TcpSocket& first, const TcpSocket& second, std::chrono::milliseconds timeout, bool& firstReady, bool& secondReady);

        void close();

    private:
        void updateLocalPort();
    };
}