#pragma once
#include <atomic>
#include <cstdint>
#include <list>
#include <mutex>
#include <string>
#include <thread>
#include "file/fileSerializer.h"
#include "network/tcpSocket.h"
#include "server/connectionThread.h"

#define SERVER_BUCKET_PATH "server_bucket"

namespace Server
{
    enum ServerThreadState
    {
        INIT,
        RUNNING,
        STOPPED
    };


    class Server
    {
    private:
        FileTransfer::FileSerializer serializer;
        std::atomic<ServerThreadState> state;

        Network::TcpSocket listener4;
        Network::TcpSocket listener6;
        std::thread keyboardThread;

        std::mutex connectionsMutex;
        std::list<ConnectionThread> connections;
        std::mutex outputMutex;

        void acceptLoop();
        void acceptFrom(Network::TcpSocket& listener, bool ipv6);
        void keyboardLoop();
        void removeFinishedConnections();
        void shutdown();

    public:
        Server();
        ~Server();

        void start();
        void stop();

        void log(const std::string& line);
        void broadcast(const std::string& text, const ConnectionThread* exclude);
        const FileTransfer::FileSerializer& fileSerializer() const;
    };
}
