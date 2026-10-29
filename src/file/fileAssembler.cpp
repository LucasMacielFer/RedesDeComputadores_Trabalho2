#include "file/fileAssembler.h"
#include <fstream>

namespace FileTransfer
{
    FileAssembler::FileAssembler() :
    metadataReceived(false),
    digestReceived(false),
    expectedSize(0),
    expectedDigest{}
    {
    }

    FileAssembler::~FileAssembler()
    {
    }

    void FileAssembler::setMetadata(uint64_t totalSize)
    {
        expectedSize = totalSize;
        metadataReceived = true;
        buffer.reserve(static_cast<size_t>(totalSize));
    }

    void FileAssembler::appendChunk(const std::vector<uint8_t>& data)
    {
        hasher.update(data.data(), data.size());
        buffer.insert(buffer.end(), data.begin(), data.end());
    }

    void FileAssembler::setExpectedDigest(const Sha256Digest& digest)
    {
        expectedDigest = digest;
        digestReceived = true;
    }

    bool FileAssembler::hasMetadata() const
    {
        return metadataReceived;
    }

    bool FileAssembler::hasDigest() const
    {
        return digestReceived;
    }

    uint64_t FileAssembler::getBytesReceived() const
    {
        return buffer.size();
    }

    uint64_t FileAssembler::getExpectedSize() const
    {
        return expectedSize;
    }

    bool FileAssembler::isComplete() const
    {
        return metadataReceived && buffer.size() >= expectedSize;
    }

    bool FileAssembler::verifyIntegrity() const
    {
        return digestReceived && buffer.size() == expectedSize && hasher.finalize() == expectedDigest;
    }

    bool FileAssembler::saveTo(const std::filesystem::path& outputPath) const
    {
        std::ofstream out(outputPath, std::ios::binary);
        if (!out.is_open())
            return false;

        out.write(reinterpret_cast<const char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
        return out.good();
    }
}
