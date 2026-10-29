#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include "file/sha256.h"
#include "protocol/protocol.h"

namespace FileTransfer
{
    // Le o arquivo em blocos e calcula o SHA-256 durante a leitura.
    // O digest so e valido depois que o arquivo inteiro foi lido (isFinished()).
    class FileTransferSession
    {
    private:
        std::ifstream file;
        uint64_t fileSize;
        uint64_t bytesSent;
        bool failed;
        Sha256 hasher;

    public:
        FileTransferSession(std::ifstream file, uint64_t fileSize);
        ~FileTransferSession();

        uint64_t getFileSize() const;
        bool isFinished() const;
        // Verdadeiro se o arquivo terminou antes do esperado (ex.: foi truncado durante o envio).
        bool hasFailed() const;

        std::vector<uint8_t> readNextChunk(size_t maxSize);
        Sha256Digest getDigest() const;
    };

    class FileSerializer
    {
    private:
        std::filesystem::path bucketRoot;

    public:
        FileSerializer(std::filesystem::path bucketRoot);
        ~FileSerializer();
        std::shared_ptr<FileTransferSession> open(const std::string& requestedName, Protocol::FileErrorReason& outError) const;
    };
}
