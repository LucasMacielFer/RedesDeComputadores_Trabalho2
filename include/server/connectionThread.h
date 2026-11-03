#pragma once
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include "network/tcpSocket.h"
#include "protocol/protocol.h"

namespace Server
{
    class Server;

    class ConnectionThread
    {
    private:
        Network::TcpSocket socket;
        std::mutex sendMutex;
        Server& server;
        std::string label;

        std::thread thread;
        std::atomic<bool> stopRequested;
        std::atomic<bool> finished;

        void run();
        bool send(Protocol::ApplicationCode code, const uint8_t* data, size_t length);
        bool handleRequest(const std::string& filename);

    public:
        ConnectionThread(bool ipv6, Server& server);
        ~ConnectionThread();

        ConnectionThread(const ConnectionThread&) = delete;
        ConnectionThread& operator=(const ConnectionThread&) = delete;

        bool accept(Network::TcpSocket& listener);

        void start();
        void stop();
        void join();
        bool isFinished() const;

        bool sendChat(const std::string& text);

        const std::string& getLabel() const;
    };
}
