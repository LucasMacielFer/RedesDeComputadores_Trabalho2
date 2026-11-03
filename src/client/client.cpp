#include "client/client.h"
#include "file/fileAssembler.h"
#include "protocol/framing.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <conio.h>
    #include <windows.h>
#else
    #include <poll.h>
    #include <signal.h>
    #include <termios.h>
    #include <unistd.h>
#endif

namespace client
{
    namespace
    {
        constexpr int KEY_NONE = -1;
        constexpr int KEY_EOF = -2;
        constexpr int KEY_ESC = 27;

        constexpr int TRANSFER_IDLE_TIMEOUT_SECONDS = 30;
        constexpr size_t MAX_CHAT_MESSAGE_SIZE = 1024;

    #ifndef _WIN32
        termios savedTermios;
        bool termiosSaved = false;

        void onSigint(int)
        {
            if (termiosSaved)
                tcsetattr(STDIN_FILENO, TCSANOW, &savedTermios);
            _exit(130);
        }
    #endif

        class TerminalMode
        {
        public:
            TerminalMode()
            {
            #ifdef _WIN32
                SetConsoleOutputCP(CP_UTF8);
                HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
                DWORD mode = 0;
                if (GetConsoleMode(out, &mode))
                    SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
            #else
                if (tcgetattr(STDIN_FILENO, &savedTermios) == 0)
                {
                    termios raw = savedTermios;
                    raw.c_lflag &= ~static_cast<tcflag_t>(ICANON | ECHO);
                    raw.c_cc[VMIN] = 1;
                    raw.c_cc[VTIME] = 0;
                    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
                    termiosSaved = true;
                    signal(SIGINT, onSigint);
                }
            #endif
            }

            ~TerminalMode()
            {
            #ifndef _WIN32
                if (termiosSaved)
                {
                    tcsetattr(STDIN_FILENO, TCSANOW, &savedTermios);
                    termiosSaved = false;
                }
            #endif
            }
        };

        void clearScreen()
        {
            std::cout << "\033[2J\033[H" << std::flush;
        }

        int readKey(int timeoutMs)
        {
        #ifdef _WIN32
            int waited = 0;
            while (!_kbhit())
            {
                if (timeoutMs >= 0 && waited >= timeoutMs)
                    return KEY_NONE;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                waited += 10;
            }

            int c = _getch();
            if (c == 0 || c == 0xE0)
            {
                _getch();
                return KEY_NONE;
            }
            return c;
        #else
            pollfd pfd{STDIN_FILENO, POLLIN, 0};
            if (::poll(&pfd, 1, timeoutMs) <= 0)
                return KEY_NONE;

            unsigned char c;
            if (::read(STDIN_FILENO, &c, 1) <= 0)
                return KEY_EOF;

            if (c != KEY_ESC)
                return c;

            if (::poll(&pfd, 1, 25) <= 0)
                return KEY_ESC;

            while (::poll(&pfd, 1, 5) > 0)
            {
                if (::read(STDIN_FILENO, &c, 1) <= 0)
                    break;
            }
            return KEY_NONE;
        #endif
        }

        bool isBackspace(int key)
        {
            return key == 127 || key == 8;
        }

        bool isPrintable(int key)
        {
            return key >= 0x20 && key != 127 && key <= 0xFF;
        }

        bool eraseLastChar(std::string& text)
        {
            if (text.empty())
                return false;

            while (!text.empty() && (static_cast<unsigned char>(text.back()) & 0xC0) == 0x80)
                text.pop_back();

            if (!text.empty())
                text.pop_back();

            return true;
        }

        std::string readLine(bool& cancelled)
        {
            std::string line;
            cancelled = false;

            while (true)
            {
                const int key = readKey(-1);
                if (key == KEY_NONE)
                    continue;

                if (key == KEY_ESC || key == KEY_EOF)
                {
                    cancelled = true;
                    return "";
                }

                if (key == '\r' || key == '\n')
                {
                    std::cout << '\n' << std::flush;
                    return line;
                }

                if (isBackspace(key))
                {
                    if (eraseLastChar(line))
                        std::cout << "\b \b" << std::flush;
                }
                else if (isPrintable(key))
                {
                    line.push_back(static_cast<char>(key));
                    std::cout << static_cast<char>(key) << std::flush;
                }
            }
        }

        std::string toHex(const uint8_t* data, size_t length)
        {
            std::ostringstream out;
            out << std::hex << std::setfill('0');
            for (size_t i = 0; i < length; i++)
                out << std::setw(2) << static_cast<int>(data[i]);
            return out.str();
        }

        std::string endpointToString(const Network::Endpoint& endpoint)
        {
            char buffer[INET6_ADDRSTRLEN] = {};
            inet_ntop(endpoint.isIpv6 ? AF_INET6 : AF_INET, endpoint.ip, buffer, sizeof(buffer));

            std::ostringstream out;
            if (endpoint.isIpv6)
                out << '[' << buffer << ']';
            else
                out << buffer;
            out << ':' << endpoint.port;
            return out.str();
        }

        enum class TransferResult
        {
            OK,
            HASH_MISMATCH,
            SERVER_ERROR,
            CONNECTION_LOST,
            PROTOCOL_ERROR
        };

        void printProgress(uint64_t received, uint64_t total)
        {
            const int percent = total == 0 ? 100 : static_cast<int>(received * 100 / total);
            std::cout << "\r  Recebendo: " << std::setw(3) << percent << "% (" << received << " / " << total << " bytes)" << std::flush;
        }

        TransferResult downloadFile(Network::TcpSocket& socket, const std::string& filename, FileTransfer::FileAssembler& assembler, TransferInfo& info)
        {
            if (!Protocol::sendMessage(socket, Protocol::REQUEST, reinterpret_cast<const uint8_t*>(filename.data()), filename.size()))
                return TransferResult::CONNECTION_LOST;

            int lastPercent = -1;
            int idleSeconds = 0;

            try
            {
                while (true)
                {
                    if (!socket.waitForData(std::chrono::seconds(1)))
                    {
                        if (++idleSeconds >= TRANSFER_IDLE_TIMEOUT_SECONDS)
                            return TransferResult::CONNECTION_LOST;
                        continue;
                    }
                    idleSeconds = 0;

                    Protocol::Message message;
                    if (!Protocol::receiveMessage(socket, message))
                        return TransferResult::CONNECTION_LOST;

                    switch (message.appCode)
                    {
                    case Protocol::FILE_ERROR:
                        if (message.payload.size() != 1)
                            return TransferResult::PROTOCOL_ERROR;

                        info.errorReason = static_cast<Protocol::FileErrorReason>(message.payload[0]);
                        return TransferResult::SERVER_ERROR;

                    case Protocol::METADATA:
                    {
                        if (message.payload.size() != 8 || assembler.hasMetadata())
                            return TransferResult::PROTOCOL_ERROR;

                        uint64_t size = 0;
                        for (uint8_t b : message.payload)
                            size = (size << 8) | b;

                        assembler.setMetadata(size);
                        std::cout << "  Metadados recebidos: " << size << " bytes\n" << std::flush;
                        break;
                    }

                    case Protocol::DATA:
                    {
                        if (!assembler.hasMetadata())
                            return TransferResult::PROTOCOL_ERROR;

                        assembler.appendChunk(message.payload);
                        if (assembler.getBytesReceived() > assembler.getExpectedSize())
                            return TransferResult::PROTOCOL_ERROR;

                        const uint64_t total = assembler.getExpectedSize();
                        const int percent = total == 0 ? 100 : static_cast<int>(assembler.getBytesReceived() * 100 / total);
                        if (percent != lastPercent)
                        {
                            lastPercent = percent;
                            printProgress(assembler.getBytesReceived(), total);
                        }
                        break;
                    }

                    case Protocol::FILE_HASH:
                    {
                        if (message.payload.size() != 32 || !assembler.hasMetadata())
                            return TransferResult::PROTOCOL_ERROR;

                        FileTransfer::Sha256Digest digest;
                        std::copy(message.payload.begin(), message.payload.end(), digest.begin());
                        assembler.setExpectedDigest(digest);

                        if (lastPercent != 100)
                            printProgress(assembler.getBytesReceived(), assembler.getExpectedSize());
                        std::cout << '\n';

                        info.digestHex = toHex(digest.data(), digest.size());
                        return assembler.verifyIntegrity() ? TransferResult::OK : TransferResult::HASH_MISMATCH;
                    }

                    default:
                        // Mensagens de chat que chegam durante a transferencia sao ignoradas.
                        break;
                    }
                }
            }
            catch (const std::exception&)
            {
                // Ex.: std::bad_alloc ao reservar memoria para um tamanho absurdo nos metadados.
                return TransferResult::PROTOCOL_ERROR;
            }
        }

        std::string errorMessageFor(Protocol::FileErrorReason reason)
        {
            return reason == Protocol::PERMISSION_DENIED ? ERROR_MESSAGE_PERMISSION_DENIED : ERROR_MESSAGE_FILE_NOT_FOUND;
        }

        // ---------------------------------------------------------------- chat

        // Estado compartilhado entre a thread do teclado e a thread que recebe do servidor.
        struct ChatSession
        {
            std::atomic<bool> stop{false};
            std::atomic<bool> connectionLost{false};
            std::mutex outputMutex;
            std::string input;

            // Chamar com outputMutex travado: redesenha a linha de digitacao.
            void redrawPrompt() const
            {
                std::cout << "\r\033[2K" << MENU_TEXT_CHAT << input << std::flush;
            }

            // Mostra uma linha acima da linha de digitacao.
            void printLine(const std::string& line)
            {
                std::lock_guard<std::mutex> lock(outputMutex);
                std::cout << "\r\033[2K" << line << '\n';
                redrawPrompt();
            }

            void fail()
            {
                connectionLost = true;
                stop = true;
            }
        };

        // Corpo da thread do teclado: edita a mensagem, envia no ENTER e sai no ESC.
        void chatKeyboardLoop(Network::TcpSocket& socket, ChatSession& chat)
        {
            while (!chat.stop)
            {
                const int key = readKey(50);
                if (key == KEY_NONE)
                    continue;

                if (key == KEY_ESC || key == KEY_EOF)
                {
                    chat.stop = true;
                }
                else if (key == '\r' || key == '\n')
                {
                    std::string text;
                    {
                        std::lock_guard<std::mutex> lock(chat.outputMutex);
                        text.swap(chat.input);
                    }

                    if (text.empty())
                        continue;

                    if (!Protocol::sendMessage(socket, Protocol::CHAT_MSG, reinterpret_cast<const uint8_t*>(text.data()), text.size()))
                    {
                        chat.fail();
                        return;
                    }

                    chat.printLine("[Você] " + text);
                }
                else if (isBackspace(key))
                {
                    std::lock_guard<std::mutex> lock(chat.outputMutex);
                    if (eraseLastChar(chat.input))
                        chat.redrawPrompt();
                }
                else if (isPrintable(key))
                {
                    std::lock_guard<std::mutex> lock(chat.outputMutex);
                    if (chat.input.size() < MAX_CHAT_MESSAGE_SIZE)
                    {
                        chat.input.push_back(static_cast<char>(key));
                        chat.redrawPrompt();
                    }
                }
            }
        }
    }

    Client::Client() :
        state(INIT),
        menu(WAITING_CONNECTION),
        serverEndpoint{}
    {
        makeEndpoint(DEFAULT_SERVER_IP, DEFAULT_SERVER_PORT, serverEndpoint);
    }

    Client::~Client()
    {
    }

    void Client::setServerEndpoint(const Network::Endpoint& endpoint)
    {
        serverEndpoint = endpoint;
    }

    bool Client::makeEndpoint(const std::string& ip, uint16_t port, Network::Endpoint& out)
    {
        out = {};

        if (inet_pton(AF_INET, ip.c_str(), out.ip) == 1)
            out.isIpv6 = false;
        else if (inet_pton(AF_INET6, ip.c_str(), out.ip) == 1)
            out.isIpv6 = true;
        else
            return false;

        out.port = port;
        return true;
    }

    void Client::run()
    {
        TerminalMode terminal;

        while (menu != EXIT)
        {
            displayMenu();
            handleUserInput();
        }

        clearScreen();
    }

    void Client::displayMenu()
    {
        clearScreen();

        if (!notice.empty())
        {
            std::cout << notice << '\n';
            notice.clear();
        }

        if (menu == WAITING_CONNECTION)
            std::cout << MENU_TEXT_WAITING_CONNECTION << std::flush;
        else if (menu == MAIN)
            std::cout << MAIN_MENU_TEXT << std::flush;
    }

    void Client::handleUserInput()
    {
        const int key = readKey(-1);

        if (menu == WAITING_CONNECTION)
        {
            if (key == '1')
            {
                clearScreen();
                std::cout << "Conectando a " << endpointToString(serverEndpoint) << "...\n" << std::flush;

                if (connectToServer(serverEndpoint))
                    menu = MAIN;
                else
                    notice = ERROR_MESSAGE_CONNECTION_FAILED;
            }
            else if (key == '2' || key == KEY_EOF)
            {
                exit();
            }
        }
        else if (menu == MAIN)
        {
            if (key == '1')
                requestFile();
            else if (key == '2')
                chat();
            else if (key == '3' || key == KEY_EOF)
                exit();
        }
    }

    bool Client::connectToServer(Network::Endpoint endpoint)
    {
        socket = std::make_unique<Network::TcpSocket>(endpoint.isIpv6);

        if (!socket->connect(endpoint))
        {
            socket.reset();
            state = DISCONNECTED;
            return false;
        }

        state = CONNECTED;
        return true;
    }

    void Client::dropConnection(const std::string& reason)
    {
        socket.reset();
        state = DISCONNECTED;
        menu = WAITING_CONNECTION;
        notice = reason;
    }

    void Client::exit()
    {
        socket.reset();
        menu = EXIT;
    }

    void Client::requestFile()
    {
        menu = REQUEST_FILE;

        // Tudo o que a recepcao usa e preparado antes de pedir o nome do arquivo.
        auto assembler = std::make_unique<FileTransfer::FileAssembler>();
        bool assemblerUsed = false;

        while (true)
        {
            clearScreen();
            std::cout << "[ESC] Retornar ao menu principal\n\n" << MENU_TEXT_REQUEST_FILE << std::flush;

            bool cancelled = false;
            const std::string filename = readLine(cancelled);
            if (cancelled)
                break;
            if (filename.empty())
                continue;

            std::cout << '\n';

            TransferResult result = TransferResult::HASH_MISMATCH;
            TransferInfo info;

            for (int attempt = 1; attempt <= MAX_DOWNLOAD_ATTEMPTS; attempt++)
            {
                if (assemblerUsed)
                    assembler = std::make_unique<FileTransfer::FileAssembler>();
                assemblerUsed = true;

                std::cout << "Solicitando \"" << filename << "\" (tentativa " << attempt << "/" << MAX_DOWNLOAD_ATTEMPTS << ")\n" << std::flush;
                result = downloadFile(*socket, filename, *assembler, info);

                if (result != TransferResult::HASH_MISMATCH)
                    break;

                std::cout << "[ERRO] SHA-256 recebido não confere com o arquivo.\n" << std::flush;
            }

            if (result == TransferResult::CONNECTION_LOST)
            {
                dropConnection(ERROR_MESSAGE_CONNECTION_LOST);
                return;
            }

            if (result == TransferResult::PROTOCOL_ERROR)
            {
                // O fluxo ficou fora de sincronia: nao da para reaproveitar a conexao.
                dropConnection(ERROR_MESSAGE_PROTOCOL);
                return;
            }

            if (result == TransferResult::SERVER_ERROR)
            {
                std::cout << errorMessageFor(info.errorReason);
            }
            else if (result == TransferResult::HASH_MISMATCH)
            {
                std::cout << "[ERRO] Integridade não verificada após " << MAX_DOWNLOAD_ATTEMPTS << " tentativas.\n";
            }
            else
            {
                std::filesystem::path name = std::filesystem::path(filename).filename();
                if (name.empty())
                    name = "arquivo_recebido";

                std::error_code ec;
                std::filesystem::create_directories(DOWNLOAD_DIR, ec);
                const std::filesystem::path output = std::filesystem::path(DOWNLOAD_DIR) / name;

                std::cout << "SHA-256 verificado: " << info.digestHex << '\n';
                if (assembler->saveTo(output))
                    std::cout << "Arquivo salvo em " << output.string() << '\n';
                else
                    std::cout << "[ERRO] Não foi possível salvar em " << output.string() << '\n';
            }

            std::cout << "\n[ESC] Retornar ao menu principal   [ENTER] Solicitar outro arquivo\n" << std::flush;

            int key;
            do
            {
                key = readKey(-1);
            } while (key != KEY_ESC && key != KEY_EOF && key != '\r' && key != '\n');

            if (key != '\r' && key != '\n')
                break;
        }

        menu = MAIN;
    }

    void Client::chat()
    {
        menu = CHAT;

        clearScreen();
        std::cout << "[ESC] Retornar ao menu principal\n\n" << std::flush;

        ChatSession session;
        {
            std::lock_guard<std::mutex> lock(session.outputMutex);
            session.redrawPrompt();
        }

        // Thread exclusiva do teclado.
        std::thread keyboard(chatKeyboardLoop, std::ref(*socket), std::ref(session));

        // Thread principal: mensagens do servidor.
        while (!session.stop)
        {
            if (!socket->waitForData(std::chrono::milliseconds(100)))
                continue;

            Protocol::Message message;
            if (!Protocol::receiveMessage(*socket, message))
            {
                session.fail();
                break;
            }

            if (message.appCode == Protocol::CHAT_BROADCAST)
                session.printLine(std::string(message.payload.begin(), message.payload.end()));
        }

        keyboard.join();

        if (session.connectionLost)
        {
            dropConnection(ERROR_MESSAGE_CONNECTION_LOST);
            return;
        }

        menu = MAIN;
    }
}
