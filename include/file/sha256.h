#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <openssl/evp.h>

namespace FileTransfer
{
    using Sha256Digest = std::array<uint8_t, 32>;

    class Sha256
    {
    private:
        EVP_MD_CTX* ctx;

    public:
        Sha256();
        ~Sha256();

        Sha256(const Sha256&) = delete;
        Sha256& operator=(const Sha256&) = delete;

        void update(const uint8_t* data, size_t length);
        Sha256Digest finalize() const;
    };
}
