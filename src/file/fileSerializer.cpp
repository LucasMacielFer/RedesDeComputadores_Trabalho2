#include "file/fileSerializer.h"
#include <algorithm>
#include <system_error>

namespace FileTransfer
{
    namespace
    {
        bool isInside(const std::filesystem::path& bucketRoot, const std::filesystem::path& candidate)
        {
            const std::string root = bucketRoot.string();
            const std::string target = candidate.string();

            if (target == root)
                return true;

            return target.size() > root.size() &&
                   target.compare(0, root.size(), root) == 0 &&
                   target[root.size()] == std::filesystem::path::preferred_separator;
        }
    }

    FileTransferSession::FileTransferSession() :
        fileSize(0),
        bytesSent(0),
        failed(false)
    {
    }

    FileTransferSession::~FileTransferSession()
    {
        if (file.is_open())
            file.close();
    }

    bool FileTransferSession::open(const std::filesystem::path& path)
    {
        file.open(path, std::ios::binary);
        if (!file.is_open())
            return false;

        std::error_code ec;
        fileSize = std::filesystem::file_size(path, ec);
        if (ec)
        {
            file.close();
            return false;
        }

        return true;
    }

    uint64_t FileTransferSession::getFileSize() const
    {
        return fileSize;
    }

    bool FileTransferSession::isFinished() const
    {
        return failed || bytesSent >= fileSize;
    }

    bool FileTransferSession::hasFailed() const
    {
        return failed;
    }

    Sha256Digest FileTransferSession::getDigest() const
    {
        return hasher.finalize();
    }

    std::vector<uint8_t> FileTransferSession::readNextChunk(size_t maxSize)
    {
        const uint64_t remaining = fileSize - bytesSent;
        const size_t toRead = static_cast<size_t>(std::min<uint64_t>(remaining, maxSize));

        std::vector<uint8_t> chunk(toRead);
        file.read(reinterpret_cast<char*>(chunk.data()), static_cast<std::streamsize>(toRead));

        const size_t got = static_cast<size_t>(file.gcount());
        if (got < toRead)
        {
            chunk.resize(got);
            failed = true;
        }

        hasher.update(chunk.data(), chunk.size());
        bytesSent += got;

        return chunk;
    }

    FileSerializer::FileSerializer(std::filesystem::path bucketRoot) :
        bucketRoot(std::filesystem::weakly_canonical(bucketRoot))
    {
    }

    FileSerializer::~FileSerializer()
    {
    }

    bool FileSerializer::open(const std::string& requestedName, FileTransferSession& session, Protocol::FileErrorReason& outError) const
    {
        if (requestedName.empty() || requestedName.find("..") != std::string::npos)
        {
            outError = Protocol::PERMISSION_DENIED;
            return false;
        }

        const std::filesystem::path requested(requestedName);
        if (requested.is_absolute())
        {
            outError = Protocol::PERMISSION_DENIED;
            return false;
        }

        std::error_code ec;
        const std::filesystem::path candidate = std::filesystem::weakly_canonical(bucketRoot / requested, ec);

        if (ec || !isInside(bucketRoot, candidate))
        {
            outError = Protocol::PERMISSION_DENIED;
            return false;
        }

        if (!std::filesystem::exists(candidate, ec) || !std::filesystem::is_regular_file(candidate, ec))
        {
            outError = Protocol::NOT_FOUND;
            return false;
        }

        if (!session.open(candidate))
        {
            outError = Protocol::NOT_FOUND;
            return false;
        }

        return true;
    }
}
