#include "Core/ZipWriter.h"
#include <windows.h>
#include <memory>
#include "Core/Text.h"

namespace RivalsForge::Zip {

namespace {

constexpr uint32_t kLocalHeader = 0x04034b50, kCentralHeader = 0x02014b50, kEndOfDirectory = 0x06054b50;
constexpr uint32_t kZip64Locator = 0x07064b50, kZip64EndOfDirectory = 0x06064b50;
constexpr uint64_t kMax32 = 0xFFFFFFFFull;
constexpr uint16_t kUtf8 = 0x0800, kVersion = 20, kVersion64 = 45;
constexpr size_t kChunk = 1 << 20;

// Little-endian fields appended to a header
struct Bytes {
    std::string data;
    void U16(uint16_t v) { data.append(reinterpret_cast<const char*>(&v), 2); }
    void U32(uint32_t v) { data.append(reinterpret_cast<const char*>(&v), 4); }
    void U64(uint64_t v) { data.append(reinterpret_cast<const char*>(&v), 8); }
    void Str(const std::string& s) { data += s; }
};

uint32_t Low32(uint64_t v) { return v >= kMax32 ? static_cast<uint32_t>(kMax32) : static_cast<uint32_t>(v); }

} // namespace

Writer::~Writer() {
    if (file) CloseHandle(static_cast<HANDLE>(file));
}

std::unique_ptr<Writer> Writer::Create(const std::wstring& path, Failure* failure) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        if (failure) *failure = { Error::WriteFailed, GetLastError() };
        return nullptr;
    }
    std::unique_ptr<Writer> w(new Writer());
    w->file = h;
    return w;
}

bool Writer::Write(const void* data, size_t size) {
    const auto* p = static_cast<const uint8_t*>(data);
    while (size > 0) {
        DWORD written = 0;
        const DWORD n = static_cast<DWORD>(std::min<size_t>(size, kChunk));
        if (!WriteFile(static_cast<HANDLE>(file), p, n, &written, nullptr) || written != n) return false;
        p += n;
        size -= n;
    }
    return true;
}

uint64_t Writer::Position() const {
    LARGE_INTEGER zero{}, pos{};
    SetFilePointerEx(static_cast<HANDLE>(file), zero, &pos, FILE_CURRENT);
    return static_cast<uint64_t>(pos.QuadPart);
}

bool Writer::Seek(uint64_t offset) {
    LARGE_INTEGER to{};
    to.QuadPart = static_cast<LONGLONG>(offset);
    return SetFilePointerEx(static_cast<HANDLE>(file), to, nullptr, FILE_BEGIN) != 0;
}

bool Writer::AddFile(const std::wstring& entryName, const std::wstring& source, Failure* failure, const std::function<void(uint64_t)>& progress) {
    HANDLE in = CreateFileW(source.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (in == INVALID_HANDLE_VALUE) {
        if (failure) *failure = { Error::OpenFailed, GetLastError() };
        return false;
    }
    std::unique_ptr<void, decltype(&CloseHandle)> guard(in, &CloseHandle);
    LARGE_INTEGER size{};
    FILETIME written{}, local{};
    GetFileSizeEx(in, &size);
    GetFileTime(in, nullptr, nullptr, &written);
    FileTimeToLocalFileTime(&written, &local);
    uint16_t dosDate = 0, dosTime = 0;
    FileTimeToDosDateTime(&local, &dosDate, &dosTime);

    DWORD readError = 0;
    const bool ok = AddStream(
        entryName, static_cast<uint64_t>(size.QuadPart), dosTime, dosDate,
        [&](const std::function<bool(const uint8_t*, size_t)>& write) {
            const auto buffer = std::make_unique_for_overwrite<uint8_t[]>(kChunk);
            for (;;) {
                DWORD read = 0;
                if (!ReadFile(in, buffer.get(), static_cast<DWORD>(kChunk), &read, nullptr)) {
                    readError = GetLastError();
                    return false;
                }
                if (read == 0) return true;
                if (!write(buffer.get(), read)) return false;
            }
        },
        failure, progress);
    if (!ok && readError && failure) *failure = { Error::OpenFailed, readError };
    return ok;
}

bool Writer::AddStream(const std::wstring& entryName, uint64_t size, uint16_t dosTime, uint16_t dosDate,
                       const std::function<bool(const std::function<bool(const uint8_t*, size_t)>& write)>& produce, Failure* failure,
                       const std::function<void(uint64_t)>& progress) {
    const auto fail = [&](Error error, DWORD win32) {
        if (failure) *failure = { error, win32 };
        return false;
    };
    Record r;
    r.name = Text::Narrow(entryName);
    r.size = size;
    r.offset = Position();
    r.dosTime = dosTime;
    r.dosDate = dosDate;
    const bool big = r.size >= kMax32;

    // The local header, its CRC filled in once the data is through
    Bytes h;
    h.U32(kLocalHeader);
    h.U16(big ? kVersion64 : kVersion);
    h.U16(kUtf8);
    h.U16(0);   // stored
    h.U16(r.dosTime);
    h.U16(r.dosDate);
    h.U32(0);
    h.U32(Low32(r.size));
    h.U32(Low32(r.size));
    h.U16(static_cast<uint16_t>(r.name.size()));
    h.U16(big ? 20 : 0);
    h.Str(r.name);
    if (big) {
        h.U16(0x0001);
        h.U16(16);
        h.U64(r.size);
        h.U64(r.size);
    }
    if (!Write(h.data.data(), h.data.size())) return fail(Error::WriteFailed, GetLastError());

    uint64_t copied = 0;
    uint32_t crc = 0;
    DWORD writeError = 0;
    const bool produced = produce([&](const uint8_t* data, size_t n) {
        crc = Crc32(data, n, crc);
        if (!Write(data, n)) {
            writeError = GetLastError();
            return false;
        }
        copied += n;
        if (progress) progress(copied);
        return true;
    });
    if (writeError) return fail(Error::WriteFailed, writeError);
    if (!produced) {
        if (failure && failure->error == Error::None) *failure = { Error::Corrupt, 0 };
        return false;
    }
    if (copied != r.size) return fail(Error::OpenFailed, ERROR_READ_FAULT);   // the data wasn't the size announced
    r.crc = crc;

    const uint64_t end = Position();
    if (!Seek(r.offset + 14) || !Write(&crc, 4) || !Seek(end)) return fail(Error::WriteFailed, GetLastError());
    records.push_back(std::move(r));
    return true;
}

bool Writer::Finish(Failure* failure) {
    const auto fail = [&] {
        if (failure) *failure = { Error::WriteFailed, GetLastError() };
        return false;
    };
    const uint64_t directoryStart = Position();
    Bytes d;
    for (const Record& r : records) {
        const bool bigSize = r.size >= kMax32, bigOffset = r.offset >= kMax32;
        Bytes extra;
        if (bigSize || bigOffset) {
            extra.U16(0x0001);
            extra.U16(static_cast<uint16_t>((bigSize ? 16 : 0) + (bigOffset ? 8 : 0)));
            if (bigSize) {
                extra.U64(r.size);
                extra.U64(r.size);
            }
            if (bigOffset) extra.U64(r.offset);
        }
        const uint16_t version = bigSize || bigOffset ? kVersion64 : kVersion;
        d.U32(kCentralHeader);
        d.U16(version);   // made by: MS-DOS
        d.U16(version);
        d.U16(kUtf8);
        d.U16(0);
        d.U16(r.dosTime);
        d.U16(r.dosDate);
        d.U32(r.crc);
        d.U32(Low32(r.size));
        d.U32(Low32(r.size));
        d.U16(static_cast<uint16_t>(r.name.size()));
        d.U16(static_cast<uint16_t>(extra.data.size()));
        d.U16(0);      // comment
        d.U16(0);      // disk
        d.U16(0);      // internal attributes
        d.U32(0x20);   // FILE_ATTRIBUTE_ARCHIVE
        d.U32(Low32(r.offset));
        d.Str(r.name);
        d.Str(extra.data);
    }
    const uint64_t directorySize = d.data.size();
    const uint64_t count = records.size();
    if (count >= 0xFFFF || directoryStart >= kMax32 || directorySize >= kMax32) {
        const uint64_t zip64At = directoryStart + directorySize;
        d.U32(kZip64EndOfDirectory);
        d.U64(44);
        d.U16(kVersion64);
        d.U16(kVersion64);
        d.U32(0);
        d.U32(0);
        d.U64(count);
        d.U64(count);
        d.U64(directorySize);
        d.U64(directoryStart);
        d.U32(kZip64Locator);
        d.U32(0);
        d.U64(zip64At);
        d.U32(1);
    }
    d.U32(kEndOfDirectory);
    d.U16(0);
    d.U16(0);
    d.U16(static_cast<uint16_t>(count >= 0xFFFF ? 0xFFFF : count));
    d.U16(static_cast<uint16_t>(count >= 0xFFFF ? 0xFFFF : count));
    d.U32(Low32(directorySize));
    d.U32(Low32(directoryStart));
    d.U16(0);
    if (!Write(d.data.data(), d.data.size())) return fail();
    const bool closed = CloseHandle(static_cast<HANDLE>(file)) != 0;
    file = nullptr;
    return closed || fail();
}

} // namespace RivalsForge::Zip
