#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace FileTransfer
{
    using Sha256Digest = std::array<uint8_t, 32>;

    class Sha256
    {
    private:
        uint32_t state[8];
        uint8_t block[64];
        size_t blockLength;
        uint64_t totalLength;

        void processBlock(const uint8_t* data);

    public:
        Sha256();

        void update(const uint8_t* data, size_t length);
        Sha256Digest finalize() const;
    };
}
