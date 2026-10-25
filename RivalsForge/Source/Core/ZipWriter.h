#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "Core/Zip.h"

// Writing zip archives: files added whole from disk, stored (mod .ucas / .pak data is already
// compressed, deflating it again saves next to nothing), streamed so a big file never sits in
// memory; ZIP64 records when a file or the archive passes 4 GB; names in UTF-8.
namespace RivalsForge::Zip {

class Writer {
public:
    ~Writer();
    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;

    // Creates (or replaces) the archive; null (and `failure`) when it can't
    static std::unique_ptr<Writer> Create(const std::wstring& path, Failure* failure = nullptr);

    // Adds the file at `source` as `entryName` ("mod_P.pak", "/" between folders). `progress` gets
    // the bytes copied so far. False (and `failure`) on error.
    bool AddFile(const std::wstring& entryName, const std::wstring& source, Failure* failure = nullptr,
                 const std::function<void(uint64_t copied)>& progress = {});
    // Adds an entry of `size` bytes whose data `produce` hands to `write` a piece at a time (false
    // from `produce` stops it: `failure` keeps what the producer set, else Corrupt). `dosTime` /
    // `dosDate`: its last write time.
    bool AddStream(const std::wstring& entryName, uint64_t size, uint16_t dosTime, uint16_t dosDate,
                   const std::function<bool(const std::function<bool(const uint8_t*, size_t)>& write)>& produce, Failure* failure = nullptr,
                   const std::function<void(uint64_t copied)>& progress = {});
    // Writes the central directory and closes the file. False (and `failure`) on error.
    bool Finish(Failure* failure = nullptr);

private:
    struct Record {
        std::string name;   // UTF-8
        uint32_t crc = 0;
        uint64_t size = 0, offset = 0;
        uint16_t dosTime = 0, dosDate = 0;
    };
    Writer() = default;
    bool Write(const void* data, size_t size);
    uint64_t Position() const;
    bool Seek(uint64_t offset);

    void* file = nullptr;   // HANDLE
    std::vector<Record> records;
};

} // namespace RivalsForge::Zip
