#pragma once
#include <string>
#include <cstdint>
#include <memory>
#include "network/endpoint.h"
#include "network/tcpSocket.h"
#include "protocol/protocol.h"

namespace client
{
    enum ClientState
    {
        INIT,
        CONNECTED,
        DISCONNECTED
    };

    enum ClientMenu
    {
        WAITING_CONNECTION,
        MAIN,
        REQUEST_FILE,
        CHAT,
        EXIT
    };

    struct TransferInfo
    {
        Protocol::FileErrorReason errorReason = Protocol::NOT_FOUND;
        std::string digestHex;
    };

    const std::string MAIN_MENU_TEXT = "[1] Solicitar arquivo \n[2] Chat \n[3] Sair\n";
    const std::string MENU_TEXT_WAITING_CONNECTION = "[1] Conectar \n[2] Sair\n";
    const std::string MENU_TEXT_REQUEST_FILE = "Escreva o nome do arquivo que deseja solicitar: ";
    const std::string MENU_TEXT_CHAT = "Mensagem: ";

    const std::string ERROR_MESSAGE_CONNECTION_FAILED = "[SERVIDOR] Falha na conexão com o servidor\n";
    const std::string ERROR_MESSAGE_CONNECTION_LOST = "[SERVIDOR] Conexão com o servidor perdida\n";
    const std::string ERROR_MESSAGE_FILE_NOT_FOUND = "[SERVIDOR] Arquivo não encontrado\n";
    const std::string ERROR_MESSAGE_PERMISSION_DENIED = "[SERVIDOR] Permissão negada\n";
    const std::string ERROR_MESSAGE_PROTOCOL = "[SERVIDOR] Resposta inválida do servidor\n";

    const std::string DEFAULT_SERVER_IP = "127.0.0.1";
    const uint16_t DEFAULT_SERVER_PORT = 8080;
    const std::string DOWNLOAD_DIR = "downloads";

    // Quantas vezes o arquivo e solicitado de novo quando o SHA-256 nao confere.
    const int MAX_DOWNLOAD_ATTEMPTS = 3;

    class Client
    {
    private:
        ClientState state;
        ClientMenu menu;
        Network::Endpoint serverEndpoint;
        std::unique_ptr<Network::TcpSocket> socket;
        std::string notice;

        bool connectToServer(Network::Endpoint endpoint);
        void requestFile();
        void chat();
        void exit();
        void dropConnection(const std::string& reason);
        void displayMenu();
        void handleUserInput();

    public:
        Client();
        ~Client();

        void setServerEndpoint(const Network::Endpoint& endpoint);
        static bool makeEndpoint(const std::string& ip, uint16_t port, Network::Endpoint& out);

        void run();
    };
}
