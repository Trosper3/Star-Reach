#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

// core/serialization/ -- the unified serialization pipeline (architecture.md section 5, section
// 9's legacy migration table).
//
// ByteWriter/ByteReader are a direct port of ../StarReach2's net/Protocol.h -- little-endian
// raw-POD copy, which is fine for the x86/x64 targets this game ships on. If a big-endian or
// mixed-arch target is ever added, the Put/Get helpers below are the only place that changes;
// nothing that calls them needs to know.
//
// This is the ONE pipeline: the same ByteWriter that packs a wire message (net/, deferred with
// ENet) also writes a .sav file (SaveMigrator, a separate issue). Both operate on blueprint form
// only (Law 3) -- see BlueprintSerialization.h for what actually gets encoded.
namespace sr::core::serialization {

class ByteWriter {
public:
    // T must be a trivially-copyable POD -- exactly what every field in a blueprint struct is
    // (Law 3). Anything else (a std::string, a std::vector) needs its own Put overload, the way
    // PutString does below.
    template <typename T>
    void Put(const T& value) {
        static_assert(std::is_trivially_copyable_v<T>, "ByteWriter::Put requires a POD type");
        const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
        data_.insert(data_.end(), bytes, bytes + sizeof(T));
    }

    // Length-prefixed string (uint8_t length, capped at 255 bytes) -- mirrors ByteReader::
    // GetString. Truncates silently past 255 chars; every string this pipeline carries today
    // (defIds, display names) is well under that.
    void PutString(const std::string& value) {
        const uint8_t length = static_cast<uint8_t>(std::min(value.size(), std::size_t{255}));
        Put(length);
        data_.insert(data_.end(), value.begin(), value.begin() + length);
    }

    const std::vector<uint8_t>& Data() const { return data_; }
    std::vector<uint8_t> Take() { return std::move(data_); }

private:
    std::vector<uint8_t> data_;
};

class ByteReader {
public:
    ByteReader(const uint8_t* data, std::size_t size) : cursor_(data), remaining_(size) {}
    explicit ByteReader(const std::vector<uint8_t>& data) : ByteReader(data.data(), data.size()) {}

    // Returns a default-constructed T and latches Ok() to false once a read runs past the
    // buffer, rather than throwing -- a truncated save or a short packet is bad input, not a
    // programmer error, and every Decode() below reads that flag instead of a stack of
    // exception handlers.
    template <typename T>
    T Get() {
        static_assert(std::is_trivially_copyable_v<T>, "ByteReader::Get requires a POD type");
        T value{};
        if (remaining_ < sizeof(T)) {
            ok_ = false;
            return value;
        }
        std::memcpy(&value, cursor_, sizeof(T));
        cursor_ += sizeof(T);
        remaining_ -= sizeof(T);
        return value;
    }

    std::string GetString() {
        const uint8_t length = Get<uint8_t>();
        if (!ok_ || remaining_ < length) {
            ok_ = false;
            return {};
        }
        std::string value(reinterpret_cast<const char*>(cursor_), length);
        cursor_ += length;
        remaining_ -= length;
        return value;
    }

    bool Ok() const { return ok_; }
    std::size_t Remaining() const { return remaining_; }

private:
    const uint8_t* cursor_ = nullptr;
    std::size_t remaining_ = 0;
    bool ok_ = true;
};

}  // namespace sr::core::serialization
