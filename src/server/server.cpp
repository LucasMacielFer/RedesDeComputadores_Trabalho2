#include "server/server.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <system_error>
#include <thread>

namespace Server
{
    Server::Server() :
        serializer(SERVER_BUCKET_PATH),
        running(false),
        listener4(false),
        listener6(true)
    {
        std::error_code ec;
        std::filesystem::create_directories(SERVER_BUCKET_PATH, ec);
    }

    void Server::start()
    {
        listener4.bind(SERVER_PORT_IPV4);
        listener4.listen(SOMAXCONN);

        listener6.bind(SERVER_PORT_IPV6);
        listener6.listen(SOMAXCONN);

        if (!listener4.isListening())
            listener4.close();
        if (!listener6.isListening())
            listener6.close();

        if (!listener4.isListening() && !listener6.isListening())
        {
            log("ERRO: Nao foi possivel escutar nas portas " + std::to_string(SERVER_PORT_IPV4) + " (IPv4) e " + std::to_string(SERVER_PORT_IPV6) + " (IPv6)");
            return;
        }

        if (!listener4.isListening())
            log("AVISO: IPv4 indisponivel, escutando apenas em IPv6.");
        else if (!listener6.isListening())
            log("AVISO: IPv6 indisponivel, escutando apenas em IPv4.");

        running = true;
        log("Servidor escutando nas portas " + std::to_string(SERVER_PORT_IPV4) + " (IPv4) e " + std::to_string(SERVER_PORT_IPV6) + " (IPv6). Digite uma mensagem e ENTER para enviar a todos os clientes.");

        std::thread(&Server::keyboardLoop, this).detach();

        acceptLoop();
        shutdown();
    }

    void Server::stop()
    {
        running = false;
    }

    void Server::acceptLoop()
    {
        while (running)
        {
            removeFinishedConnections();

            bool ready4 = false;
            bool ready6 = false;
            Network::TcpSocket::waitForEither(listener4, listener6, std::chrono::milliseconds(200), ready4, ready6);

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
        std::string line;

        while (running && std::getline(std::cin, line))
        {
            if (line.empty())
                continue;

            const std::string text = "[Servidor] " + line;
            log(text);
            broadcast(text);
        }
    }

    void Server::removeFinishedConnections()
    {
        std::lock_guard<std::mutex> lock(connectionsMutex);

        std::list<ConnectionThread>::iterator it = connections.begin();
        while (it != connections.end())
        {
            if (it->isFinished())
                it = connections.erase(it);
            else
                ++it;
        }
    }

    void Server::shutdown()
    {
        std::lock_guard<std::mutex> lock(connectionsMutex);

        for (ConnectionThread& connection : connections)
            connection.stop();
        connections.clear();

        listener4.close();
        listener6.close();
        log("Servidor encerrado.");
    }

    void Server::log(const std::string& line)
    {
        std::lock_guard<std::mutex> lock(outputMutex);
        std::cout << line << std::endl;
    }

    void Server::broadcast(const std::string& text)
    {
        std::lock_guard<std::mutex> lock(connectionsMutex);

        for (ConnectionThread& connection : connections)
            connection.sendChat(text);
    }

    const FileTransfer::FileSerializer& Server::fileSerializer() const
    {
        return serializer;
    }
}
