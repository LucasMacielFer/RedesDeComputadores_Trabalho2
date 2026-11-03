#include "server/server.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <poll.h>
    #include <unistd.h>
#endif

namespace Server
{
    namespace
    {
        constexpr int KEYBOARD_POLL_MS = 100;

        enum class ReadResult
        {
            LINE,
            TIMEOUT,
            END_OF_INPUT
        };

        class LineReader
        {
        private:
            std::string pending;

        public:
            ReadResult next(std::string& line, int timeoutMs)
            {
            #ifdef _WIN32
                if (WaitForSingleObject(GetStdHandle(STD_INPUT_HANDLE), timeoutMs) != WAIT_OBJECT_0)
                    return ReadResult::TIMEOUT;

                if (!std::getline(std::cin, line))
                    return ReadResult::END_OF_INPUT;
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                return ReadResult::LINE;
            #else
                while (true)
                {
                    const size_t end = pending.find('\n');
                    if (end != std::string::npos)
                    {
                        line = pending.substr(0, end);
                        pending.erase(0, end + 1);
                        if (!line.empty() && line.back() == '\r')
                            line.pop_back();
                        return ReadResult::LINE;
                    }

                    pollfd pfd{STDIN_FILENO, POLLIN, 0};
                    if (::poll(&pfd, 1, timeoutMs) <= 0)
                        return ReadResult::TIMEOUT;

                    char buffer[512];
                    const ssize_t got = ::read(STDIN_FILENO, buffer, sizeof(buffer));
                    if (got <= 0)
                        return ReadResult::END_OF_INPUT;

                    pending.append(buffer, static_cast<size_t>(got));
                }
            #endif
            }
        };
    }

    Server::Server() :
        serializer(SERVER_BUCKET_PATH),
        state(INIT),
        listener4(false),
        listener6(true)
    {
        std::error_code ec;
        std::filesystem::create_directories(SERVER_BUCKET_PATH, ec);
    }

    Server::~Server()
    {
        stop();
    }

    void Server::start()
    {
        if (state != INIT)
            return;

        listener4.bind(SERVER_PORT_IPV4);
        listener4.listen(SOMAXCONN);

        listener6.bind(SERVER_PORT_IPV6);
        listener6.listen(SOMAXCONN);

        if (!listener4.isListening() && !listener6.isListening())
        {
            log("ERRO: Nao foi possivel escutar nas portas " + std::to_string(SERVER_PORT_IPV4) + " (IPv4) e " + std::to_string(SERVER_PORT_IPV6) + " (IPv6)");
            return;
        }

        if (!listener4.isListening())
            log("AVISO: IPv4 indisponivel, escutando apenas em IPv6.");
        else if (!listener6.isListening())
            log("AVISO: IPv6 indisponivel, escutando apenas em IPv4.");

        state = RUNNING;
        log("Servidor escutando nas portas " + std::to_string(SERVER_PORT_IPV4) + " (IPv4) e " + std::to_string(SERVER_PORT_IPV6) + " (IPv6). Digite uma mensagem e ENTER para enviar a todos os clientes.");

        keyboardThread = std::thread(&Server::keyboardLoop, this);

        acceptLoop();
        shutdown();
    }

    void Server::stop()
    {
        if (state == RUNNING)
            state = STOPPED;
    }

    void Server::acceptLoop()
    {
        const std::chrono::milliseconds timeout(200);

        while (state == RUNNING)
        {
            removeFinishedConnections();

            bool ready4 = false;
            bool ready6 = false;

            if (listener4.isListening() && listener6.isListening())
                Network::TcpSocket::waitForEither(listener4, listener6, timeout, ready4, ready6);
            else if (listener4.isListening())
                ready4 = listener4.waitForData(timeout);
            else
                ready6 = listener6.waitForData(timeout);

            if (ready4)
                acceptFrom(listener4, false);
            if (ready6)
                acceptFrom(listener6, true);
        }
    }

    void Server::acceptFrom(Network::TcpSocket& listener, bool ipv6)
    {
        std::lock_guard<std::mutex> lock(connectionsMutex);

        connections.emplace_back(ipv6, *this);
        ConnectionThread& connection = connections.back();

        if (!connection.accept(listener))
        {
            connections.pop_back();
            return;
        }

        log("Cliente conectado: " + connection.getLabel());
        connection.start();
    }

    void Server::keyboardLoop()
    {
        LineReader reader;
        std::string line;

        while (state == RUNNING)
        {
            const ReadResult result = reader.next(line, KEYBOARD_POLL_MS);

            if (result == ReadResult::END_OF_INPUT)
                break;
            if (result == ReadResult::TIMEOUT || line.empty())
                continue;

            const std::string text = "[Servidor] " + line;
            log(text);
            broadcast(text, nullptr);
        }
    }

    void Server::removeFinishedConnections()
    {
        std::lock_guard<std::mutex> lock(connectionsMutex);

        std::list<ConnectionThread>::iterator it = connections.begin();
        while (it != connections.end())
        {
            if (it->isFinished())
            {
                it->join();
                it = connections.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void Server::shutdown()
    {
        keyboardThread.join();

        {
            std::lock_guard<std::mutex> lock(connectionsMutex);
            for (ConnectionThread& connection : connections)
                connection.stop();
        }
        for (ConnectionThread& connection : connections)
            connection.join();

        {
            std::lock_guard<std::mutex> lock(connectionsMutex);
            connections.clear();
        }

        listener4.close();
        listener6.close();
        state = STOPPED;
        log("Servidor encerrado.");
    }

    void Server::log(const std::string& line)
    {
        std::lock_guard<std::mutex> lock(outputMutex);
        std::cout << line << std::endl;
    }

    void Server::broadcast(const std::string& text, const ConnectionThread* exclude)
    {
        std::lock_guard<std::mutex> lock(connectionsMutex);

        for (ConnectionThread& connection : connections)
        {
            if (&connection != exclude)
                connection.sendChat(text);
        }
    }

    const FileTransfer::FileSerializer& Server::fileSerializer() const
    {
        return serializer;
    }
}
