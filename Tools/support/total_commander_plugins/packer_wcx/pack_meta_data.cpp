#include "pack_meta_data.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <tuple>

#include "lz4.h"

static bool LZ4CompressorDecompress(const std::vector<uint8_t>& in,
                                    std::vector<uint8_t>& out)
{
    int32_t decompressResult =
    LZ4_decompress_fast(reinterpret_cast<const char*>(in.data()),
                        reinterpret_cast<char*>(out.data()),
                        static_cast<uint32_t>(out.size()));
    if (decompressResult < 0)
    {
        return false;
    }
    return true;
}

PackMetaData::PackMetaData(const void* ptr, std::size_t size)
{
    Deserialize(ptr, size);
}

uint32_t PackMetaData::GetNumFiles() const
{
    return tableFiles.size();
}

uint32_t PackMetaData::GetNumPacks() const
{
    return tablePacks.size();
}

uint32_t PackMetaData::GetPackIndexForFile(const uint32_t fileIndex) const
{
    return tableFiles.at(fileIndex);
}

void PackMetaData::GetPackInfo(const uint32_t packIndex, std::string& packName, std::string& dependencies) const
{
    const auto& tuple = tablePacks.at(packIndex);
    packName = std::get<0>(tuple);
    dependencies = std::get<1>(tuple);
}

std::vector<uint8_t> PackMetaData::Serialize() const
{
    return std::vector<uint8_t>();
}

struct membuf : public std::streambuf
{
    membuf(const void* ptr, size_t size)
    {
        char* begin = const_cast<char*>(static_cast<const char*>(ptr));
        char* end = const_cast<char*>(begin + size);
        this->setg(begin, begin, end);
    }
};

void PackMetaData::Deserialize(const void* ptr, size_t size)
{
    assert(ptr != nullptr);
    assert(size >= 16);

    membuf buf(ptr, size);

    std::istream file(&buf);

    // 4b header - "meta"
    // 4b num_files
    // num_files b
    // 4b - uncompressed_size
    // 4b - compressed_size
    // compressed_size b
    std::array<char, 4> header;
    file.read(&header[0], 4);
    if (header != std::array<char, 4>{ 'm', 'e', 't', 'a' })
    {
        throw std::runtime_error("read metadata error - not meta");
    }
    uint32_t numFiles = 0;
    file.read(reinterpret_cast<char*>(&numFiles), 4);
    if (!file)
    {
        throw std::runtime_error("read metadata error - no numFiles");
    }
    tableFiles.resize(numFiles);

    const uint32_t numFilesBytes = numFiles * 4;
    file.read(reinterpret_cast<char*>(&tableFiles[0]), numFilesBytes);
    if (!file)
    {
        throw std::runtime_error("read metadata error - no tableFiles");
    }

    uint32_t uncompressedSize = 0;
    file.read(reinterpret_cast<char*>(&uncompressedSize), 4);
    if (!file)
    {
        throw std::runtime_error("read metadata error - no uncompressedSize");
    }
    uint32_t compressedSize = 0;
    file.read(reinterpret_cast<char*>(&compressedSize), 4);
    if (!file)
    {
        throw std::runtime_error("read metadata error - no compressedSize");
    }

    assert(16 + numFilesBytes + compressedSize == size);

    std::vector<uint8_t> compressedBuf(compressedSize);

    file.read(reinterpret_cast<char*>(&compressedBuf[0]), compressedSize);
    if (!file)
    {
        throw std::runtime_error("read metadata error - no compressedBuf");
    }

    assert(uncompressedSize >= compressedSize);

    std::vector<uint8_t> uncompressedBuf(uncompressedSize);

    if (!LZ4CompressorDecompress(compressedBuf, uncompressedBuf))
    {
        throw std::runtime_error("read metadata error - can't decompress");
    }

    const char* startBuf = reinterpret_cast<const char*>(&uncompressedBuf[0]);
    const char* endBuf = reinterpret_cast<const char*>(&uncompressedBuf[uncompressedSize]);

    membuf outBuf(startBuf, uncompressedSize);
    std::istream ss(&outBuf);

    // now parse decompressed packs data line by line (%s %s\n) format
    std::string packName;
    std::string packDependency;
    for (; !ss.eof();)
    {
        ss >> packName >> packDependency;
        tablePacks.push_back(std::tuple<std::string, std::string>(packName, packDependency));
    }
}
