#include "file/sha256.h"
#include <iostream>

namespace FileTransfer
{
    Sha256::Sha256() : ctx(EVP_MD_CTX_new())
    {
        if (ctx == nullptr || EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1)
        {
            std::cerr << "ERRO: Erro ao inicializar SHA-256" << std::endl;
        }
    }

    Sha256::~Sha256()
    {
        EVP_MD_CTX_free(ctx);
    }

    void Sha256::update(const uint8_t* data, size_t length)
    {
        if (EVP_DigestUpdate(ctx, data, length) != 1)
        {
            std::cerr << "ERRO: Erro ao atualizar SHA-256" << std::endl;
        }
    }

    Sha256Digest Sha256::finalize() const
    {
        Sha256Digest digest{};

        EVP_MD_CTX* copy = EVP_MD_CTX_new();
        unsigned int length = 0;

        if (copy == nullptr ||
            EVP_MD_CTX_copy_ex(copy, ctx) != 1 ||
            EVP_DigestFinal_ex(copy, digest.data(), &length) != 1)
        {
            std::cerr << "ERRO: Erro ao finalizar SHA-256" << std::endl;
        }

        EVP_MD_CTX_free(copy);
        return digest;
    }
}