#include "network/tcpSocket.h"

#ifdef _WIN32
    #define SOCKET_IS_INVALID(s) ((s) == INVALID_SOCKET)
    #define SOCKET_INVALID_VALUE INVALID_SOCKET
    #define SEND_FLAGS 0
    using SockLen = int;
#else
    #define SOCKET_IS_INVALID(s) ((s) < 0)
    #define SOCKET_INVALID_VALUE (-1)
    #define SEND_FLAGS MSG_NOSIGNAL
    using SockLen = socklen_t;
#endif

namespace Network
{
    namespace
    {
        void fillEndpoint(const sockaddr_storage& addr, Endpoint& endpoint)
        {
            memset(endpoint.ip, 0, 16);
            if (addr.ss_family == AF_INET6)
            {
                const auto* addr6 = reinterpret_cast<const sockaddr_in6*>(&addr);
                memcpy(endpoint.ip, &addr6->sin6_addr, 16);
                endpoint.port = ntohs(addr6->sin6_port);
                endpoint.isIpv6 = true;
            }
            else
            {
                const auto* addr4 = reinterpret_cast<const sockaddr_in*>(&addr);
                memcpy(endpoint.ip, &addr4->sin_addr, 4);
                endpoint.port = ntohs(addr4->sin_port);
                endpoint.isIpv6 = false;
            }
        }
    }

    int TcpSocket::instanceCount = 0;

    TcpSocket::TcpSocket(bool ipv6)
    {
        localEndpoint = {};
        localEndpoint.isIpv6 = ipv6;
        remoteEndpoint = {};
        socketIsBound = false;
        socketIsListening = false;
        socketIsConnected = false;
        sock = SOCKET_INVALID_VALUE;

        if (instanceCount == 0)
        {
    #ifdef _WIN32
            WSADATA wsaData;
            WSAStartup(MAKEWORD(2, 2), &wsaData);
    #endif
        }
        // Contado mesmo em caso de falha, pois o destrutor sempre decrementa.
        instanceCount++;

        sock = ::socket(ipv6 ? AF_INET6 : AF_INET, SOCK_STREAM, IPPROTO_TCP);

        if (SOCKET_IS_INVALID(sock))
        {
            std::cerr << "ERRO: Erro ao criar socket" << std::endl;
        }
    }

    TcpSocket::~TcpSocket()
    {
        close();

        instanceCount--;
        if (instanceCount == 0)
        {
    #ifdef _WIN32
            WSACleanup();
    #endif
        }
    }

    void TcpSocket::bind(uint16_t port)
    {
        if (SOCKET_IS_INVALID(sock))
        {
            std::cerr << "ERRO: Socket invalido." << std::endl;
            return;
        }

        // Permite reutilizar a porta logo apos fechar o servidor (TIME_WAIT).
        int reuse = 1;
        ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

        int result;

        if (localEndpoint.isIpv6)
        {
            sockaddr_in6 addr{};
            addr.sin6_family = AF_INET6;
            addr.sin6_port = htons(port);
            addr.sin6_addr = in6addr_any;

            result = ::bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        }
        else
        {
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.s_addr = INADDR_ANY;

            result = ::bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        }

        if (result != 0)
        {
            std::cerr << "ERRO: Erro ao dar bind (" << (localEndpoint.isIpv6 ? "IPv6" : "IPv4") << ")" << std::endl;
            return;
        }

        localEndpoint.port = port;
        socketIsBound = true;
    }

    bool TcpSocket::listen(int backlog)
    {
        if (!socketIsBound)
        {
            std::cerr << "ERRO: Socket sem bind" << std::endl;
            return false;
        }

        if (::listen(sock, backlog) != 0)
        {
            std::cerr << "ERRO: Erro ao colocar socket em escuta" << std::endl;
            return false;
        }

        socketIsListening = true;
        return true;
    }

    bool TcpSocket::accept(TcpSocket& clientSocket)
    {
        if (!socketIsListening)
        {
            std::cerr << "ERRO: Socket nao esta em escuta" << std::endl;
            return false;
        }

        sockaddr_storage fromAddr{};
        SockLen fromLen = sizeof(fromAddr);

        auto newSock = ::accept(sock, reinterpret_cast<sockaddr*>(&fromAddr), &fromLen);
        if (SOCKET_IS_INVALID(newSock))
        {
            std::cerr << "ERRO: Erro ao aceitar conexao" << std::endl;
            return false;
        }

        // Descarta o descritor que o cliente criou no construtor e assume o aceito.
        clientSocket.close();
        clientSocket.sock = newSock;
        clientSocket.localEndpoint.isIpv6 = (fromAddr.ss_family == AF_INET6);
        fillEndpoint(fromAddr, clientSocket.remoteEndpoint);
        clientSocket.socketIsConnected = true;
        clientSocket.updateLocalPort();

        return true;
    }

    bool TcpSocket::connect(const Endpoint& destEndpoint)
    {
        if (SOCKET_IS_INVALID(sock))
        {
            std::cerr << "ERRO: Socket invalido." << std::endl;
            return false;
        }

        if (destEndpoint.isIpv6 != localEndpoint.isIpv6)
        {
            std::cerr << "ERRO: Tipo de IP do destino nao corresponde ao tipo de IP do socket." << std::endl;
            return false;
        }

        int result;

        if (localEndpoint.isIpv6)
        {
            sockaddr_in6 addr{};
            addr.sin6_family = AF_INET6;
            addr.sin6_port = htons(destEndpoint.port);
            memcpy(&addr.sin6_addr, destEndpoint.ip, 16);

            result = ::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        }
        else
        {
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(destEndpoint.port);
            memcpy(&addr.sin_addr, destEndpoint.ip, 4);

            result = ::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        }

        if (result != 0)
        {
            std::cerr << "ERRO: Erro ao conectar" << std::endl;
            return false;
        }

        remoteEndpoint = destEndpoint;
        socketIsConnected = true;
        updateLocalPort();

        return true;
    }

    bool TcpSocket::isBound() const
    {
        return socketIsBound;
    }

    bool TcpSocket::isListening() const
    {
        return socketIsListening;
    }

    bool TcpSocket::isConnected() const
    {
        return socketIsConnected;
    }

    const Endpoint& TcpSocket::getRemoteEndpoint() const
    {
        return remoteEndpoint;
    }

    bool TcpSocket::send(const uint8_t* data, size_t length)
    {
        if (!socketIsConnected)
        {
            std::cerr << "ERRO: Socket nao conectado." << std::endl;
            return false;
        }

        size_t totalSent = 0;
        while (totalSent < length)
        {
            const int result = ::send(
                sock,
                reinterpret_cast<const char*>(data + totalSent),
                static_cast<int>(length - totalSent),
                SEND_FLAGS
            );

            if (result < 0)
            {
                std::cerr << "ERRO: Erro ao enviar dados." << std::endl;
                socketIsConnected = false;
                return false;
            }

            totalSent += static_cast<size_t>(result);
        }

        return true;
    }

    bool TcpSocket::recv(uint8_t* buffer, size_t bufferSize, size_t& receivedLength)
    {
        receivedLength = 0;

        if (!socketIsConnected)
        {
            std::cerr << "ERRO: Socket nao conectado." << std::endl;
            return false;
        }

        const int result = ::recv(sock, reinterpret_cast<char*>(buffer), static_cast<int>(bufferSize), 0);
        if (result < 0)
        {
            std::cerr << "ERRO: Erro ao receber dados" << std::endl;
            socketIsConnected = false;
            return false;
        }

        if (result == 0)
        {
            // O par encerrou a conexao.
            socketIsConnected = false;
            return false;
        }

        receivedLength = static_cast<size_t>(result);
        return true;
    }

    bool TcpSocket::waitForData(std::chrono::milliseconds timeout) const
    {
        if (SOCKET_IS_INVALID(sock))
            return false;

        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(sock, &readSet);

        timeval tv{};
        tv.tv_sec = static_cast<long>(timeout.count() / 1000);
        tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);

    #ifdef _WIN32
        const int result = ::select(0, &readSet, nullptr, nullptr, &tv);
    #else
        const int result = ::select(sock + 1, &readSet, nullptr, nullptr, &tv);
    #endif

        return result > 0 && FD_ISSET(sock, &readSet);
    }

    void TcpSocket::waitForEither(const TcpSocket& first, const TcpSocket& second, std::chrono::milliseconds timeout, bool& firstReady, bool& secondReady)
    {
        firstReady = false;
        secondReady = false;

        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(first.sock, &readSet);
        FD_SET(second.sock, &readSet);

        timeval tv{};
        tv.tv_sec = static_cast<long>(timeout.count() / 1000);
        tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);

    #ifdef _WIN32
        const int result = ::select(0, &readSet, nullptr, nullptr, &tv);
    #else
        const int maxFd = first.sock > second.sock ? first.sock : second.sock;
        const int result = ::select(maxFd + 1, &readSet, nullptr, nullptr, &tv);
    #endif

        if (result <= 0)
            return;

        firstReady = FD_ISSET(first.sock, &readSet);
        secondReady = FD_ISSET(second.sock, &readSet);
    }

    void TcpSocket::close()
    {
        if (!SOCKET_IS_INVALID(sock))
        {
    #ifdef _WIN32
            closesocket(sock);
    #else
            ::close(sock);
    #endif
            sock = SOCKET_INVALID_VALUE;
        }
        socketIsBound = false;
        socketIsListening = false;
        socketIsConnected = false;
    }

    void TcpSocket::updateLocalPort()
    {
        sockaddr_storage addr{};
        SockLen addrLen = sizeof(addr);

        if (::getsockname(sock, reinterpret_cast<sockaddr*>(&addr), &addrLen) != 0)
        {
            std::cerr << "ERRO: Erro ao obter endereco local." << std::endl;
            return;
        }

        fillEndpoint(addr, localEndpoint);
        socketIsBound = true;
    }
}
