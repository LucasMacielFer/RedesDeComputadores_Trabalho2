#include "server/connectionThread.h"
#include "network/endpointUtils.h"
#include "server/server.h"
#include "protocol/framing.h"
#include <chrono>

namespace Server
{
    namespace
    {
        constexpr size_t FILE_CHUNK_SIZE = 8 * 1024;
    }

    ConnectionThread::ConnectionThread(bool ipv6, Server& server) :
        socket(ipv6),
        server(server),
        stopRequested(false),
        finished(false)
    {
    }

    ConnectionThread::~ConnectionThread()
    {
        stop();
        join();
    }

    bool ConnectionThread::accept(Network::TcpSocket& listener)
    {
        if (!listener.accept(socket))
            return false;

        label = Network::endpointToString(socket.getRemoteEndpoint());
        return true;
    }

    void ConnectionThread::start()
    {
        thread = std::thread(&ConnectionThread::run, this);
    }

    void ConnectionThread::stop()
    {
        stopRequested = true;
    }

    void ConnectionThread::join()
    {
        if (thread.joinable())
            thread.join();
    }

    bool ConnectionThread::isFinished() const
    {
        return finished;
    }

    const std::string& ConnectionThread::getLabel() const
    {
        return label;
    }

    // Varias threads enviam no mesmo socket (esta, broadcast do teclado e de outros clientes):
    // cada mensagem inteira e enviada sob o mutex, para os quadros nao se misturarem.
    bool ConnectionThread::send(Protocol::ApplicationCode code, const uint8_t* data, size_t length)
    {
        std::lock_guard<std::mutex> lock(sendMutex);
        return Protocol::sendMessage(socket, code, data, length);
    }

    bool ConnectionThread::sendChat(const std::string& text)
    {
        if (finished)
            return false;

        return send(Protocol::CHAT_BROADCAST, reinterpret_cast<const uint8_t*>(text.data()), text.size());
    }

    void ConnectionThread::run()
    {
        while (!stopRequested)
        {
            // Timeout curto para reagir ao stop() mesmo sem trafego.
            if (!socket.waitForData(std::chrono::milliseconds(200)))
                continue;

            Protocol::Message message;
            if (!Protocol::receiveMessage(socket, message))
                break;

            if (message.appCode == Protocol::REQUEST)
            {
                const std::string filename(message.payload.begin(), message.payload.end());
                if (!handleRequest(filename))
                    break;
            }
            else if (message.appCode == Protocol::CHAT_MSG)
            {
                const std::string line = "[" + label + "] " + std::string(message.payload.begin(), message.payload.end());
                server.log(line);
                server.broadcast(line, this);
            }
        }

        server.log("Cliente desconectado: " + label);
        finished = true;
    }

    // Retorna false se a conexao nao serve mais.
    bool ConnectionThread::handleRequest(const std::string& filename)
    {
        server.log("[" + label + "] solicitou o arquivo \"" + filename + "\"");

        Protocol::FileErrorReason reason = Protocol::NOT_FOUND;
        FileTransfer::FileTransferSession session;

        if (!server.fileSerializer().open(filename, session, reason))
        {
            server.log("[" + label + "] arquivo recusado: \"" + filename + "\"");
            const uint8_t code = reason;
            return send(Protocol::FILE_ERROR, &code, 1);
        }

        uint8_t size[8];
        for (int i = 0; i < 8; i++)
            size[i] = static_cast<uint8_t>(session.getFileSize() >> (56 - 8 * i));

        if (!send(Protocol::METADATA, size, sizeof(size)))
            return false;

        while (!session.isFinished())
        {
            if (stopRequested)
                return false;

            const std::vector<uint8_t> chunk = session.readNextChunk(FILE_CHUNK_SIZE);
            if (chunk.empty())
                break;

            if (!send(Protocol::DATA, chunk.data(), chunk.size()))
                return false;
        }

        // O hash e calculado durante a leitura e segue depois do ultimo DATA.
        // Se o arquivo foi truncado no meio do envio, o cliente detecta pelo tamanho e pede de novo.
        const FileTransfer::Sha256Digest digest = session.getDigest();
        if (!send(Protocol::FILE_HASH, digest.data(), digest.size()))
            return false;

        server.log("[" + label + "] arquivo enviado: \"" + filename + "\"");
        return true;
    }
}
