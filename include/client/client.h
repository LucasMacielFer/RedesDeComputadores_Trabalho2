#pragma once
#include <string>
#include <cstdint>
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

    enum TransferResult
    {
        OK,
        HASH_MISMATCH,
        SERVER_ERROR,
        CONNECTION_LOST,
        PROTOCOL_ERROR
    };

    const std::string MAIN_MENU_TEXT = "[1] Solicitar arquivo \n[2] Chat \n[3] Sair\n";
    const std::string MENU_TEXT_WAITING_CONNECTION = "[1] Conectar \n[2] Sair\n";
    const std::string MENU_TEXT_REQUEST_FILE = "Escreva o nome do arquivo que deseja solicitar: ";
    const std::string MENU_TEXT_CHAT = "Mensagem: ";
    const std::string MENU_TEXT_SERVER_IP = "IP do servidor: ";
    const std::string MENU_TEXT_SERVER_PORT = "Porta do servidor: ";

    const std::string ERROR_MESSAGE_INVALID_IP = "[ERRO] IP inválido\n";
    const std::string ERROR_MESSAGE_INVALID_PORT = "[ERRO] Porta inválida (use um número de 1 a 65535)\n";
    const std::string ERROR_MESSAGE_CONNECTION_FAILED = "[SERVIDOR] Falha na conexão com o servidor\n";
    const std::string ERROR_MESSAGE_CONNECTION_LOST = "[SERVIDOR] Conexão com o servidor perdida\n";
    const std::string ERROR_MESSAGE_FILE_NOT_FOUND = "[SERVIDOR] Arquivo não encontrado\n";
    const std::string ERROR_MESSAGE_PERMISSION_DENIED = "[SERVIDOR] Permissão negada\n";
    const std::string ERROR_MESSAGE_PROTOCOL = "[SERVIDOR] Resposta inválida do servidor\n";

    const std::string DOWNLOAD_DIR = "received_files";

    const int MAX_DOWNLOAD_ATTEMPTS = 3;

    class Client
    {
    private:
        ClientState state;
        ClientMenu menu;
        Network::TcpSocket socket;
        std::string notice;

        bool askServerEndpoint(Network::Endpoint& endpoint);
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

        static bool makeEndpoint(const std::string& ip, uint16_t port, Network::Endpoint& out);
        void run();
    };
}
