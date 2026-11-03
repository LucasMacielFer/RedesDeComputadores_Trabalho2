#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "file/sha256.h"
#include "protocol/protocol.h"

namespace FileTransfer
{
    class FileTransferSession
    {
    private:
        std::ifstream file;
        uint64_t fileSize;
        uint64_t bytesSent;
        bool failed;
        Sha256 hasher;

    public:
        FileTransferSession();
        ~FileTransferSession();

        bool open(const std::filesystem::path& path);

        uint64_t getFileSize() const;
        bool isFinished() const;
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
        bool open(const std::string& requestedName, FileTransferSession& session, Protocol::FileErrorReason& outError) const;
    };
}
