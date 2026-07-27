#include <catch2/catch_test_macros.hpp>

#include <cstdint>

#include "core/serialization/ByteStream.h"

using sr::core::serialization::ByteReader;
using sr::core::serialization::ByteWriter;

TEST_CASE("ByteWriter/ByteReader round-trip mixed POD fields in order", "[serialization]") {
    ByteWriter writer;
    writer.Put(uint8_t{7});
    writer.Put(int32_t{-12345});
    writer.Put(3.5f);

    const std::vector<uint8_t> bytes = writer.Take();
    ByteReader reader(bytes);
    CHECK(reader.Get<uint8_t>() == 7);
    CHECK(reader.Get<int32_t>() == -12345);
    CHECK(reader.Get<float>() == 3.5f);
    CHECK(reader.Ok());
    CHECK(reader.Remaining() == 0);
}

TEST_CASE("ByteWriter/ByteReader round-trip a length-prefixed string", "[serialization]") {
    ByteWriter writer;
    writer.PutString("aegis_vanguard");
    writer.Put(uint16_t{42});

    ByteReader reader(writer.Data());
    CHECK(reader.GetString() == "aegis_vanguard");
    CHECK(reader.Get<uint16_t>() == 42);
    CHECK(reader.Ok());
}

TEST_CASE("ByteReader flags Ok() false on a truncated buffer instead of throwing",
          "[serialization]") {
    ByteWriter writer;
    writer.Put(int32_t{1});
    const std::vector<uint8_t> bytes = writer.Take();

    // Ask for more than the buffer has.
    ByteReader reader(bytes.data(), 2);
    (void)reader.Get<int32_t>();
    CHECK_FALSE(reader.Ok());
}

TEST_CASE("ByteReader flags Ok() false on a string length longer than the remaining bytes",
          "[serialization]") {
    ByteWriter writer;
    writer.Put(uint8_t{10});  // claims a 10-byte string, but nothing follows

    ByteReader reader(writer.Data());
    (void)reader.GetString();
    CHECK_FALSE(reader.Ok());
}
