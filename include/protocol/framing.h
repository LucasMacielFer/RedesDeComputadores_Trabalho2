#pragma once
#include <cstddef>
#include <cstdint>
#include "network/tcpSocket.h"
#include "protocol/protocol.h"

namespace Protocol
{
    // Formato de uma mensagem no fio (TCP e fluxo de bytes, entao o tamanho vai no cabecalho):
    //   [1 byte: ApplicationCode][8 bytes: tamanho do payload, big-endian][payload]
    //
    // Payload por tipo:
    //   REQUEST        nome do arquivo (bytes UTF-8)
    //   METADATA       tamanho do arquivo, 8 bytes big-endian
    //   DATA           bytes do arquivo
    //   FILE_HASH      SHA-256 do arquivo, 32 bytes (enviado depois de todos os DATA)
    //   FILE_ERROR     1 byte com FileErrorReason
    //   CHAT_MSG       texto (bytes UTF-8)
    //   CHAT_BROADCAST texto (bytes UTF-8)
    constexpr size_t HEADER_SIZE = 9;
    constexpr uint64_t MAX_PAYLOAD_SIZE = 4 * 1024 * 1024;

    // Envia cabecalho e payload em uma unica chamada de send.
    bool sendMessage(Network::TcpSocket& socket, ApplicationCode code, const uint8_t* payload, size_t length);

    // Bloqueia ate receber uma mensagem completa. Retorna false se a conexao caiu
    // ou se o par violou o protocolo (codigo desconhecido, payload maior que MAX_PAYLOAD_SIZE).
    bool receiveMessage(Network::TcpSocket& socket, Message& message);
}
