#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>
#include "file/sha256.h"

namespace FileTransfer
{
    class FileAssembler
    {
    private:
        bool metadataReceived;
        bool digestReceived;
        uint64_t expectedSize;
        Sha256Digest expectedDigest;
        Sha256 hasher;
        std::vector<uint8_t> buffer;

    public:
        FileAssembler();
        ~FileAssembler();

        void setMetadata(uint64_t totalSize);
        void appendChunk(const std::vector<uint8_t>& data);
        // O hash chega depois dos dados, na mensagem FILE_HASH.
        void setExpectedDigest(const Sha256Digest& digest);

        bool hasMetadata() const;
        bool hasDigest() const;
        uint64_t getBytesReceived() const;
        uint64_t getExpectedSize() const;
        bool isComplete() const;

        bool verifyIntegrity() const;
        bool saveTo(const std::filesystem::path& outputPath) const;
    };
}
