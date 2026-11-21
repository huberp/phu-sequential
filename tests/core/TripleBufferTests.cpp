#include <catch2/catch_test_macros.hpp>

#include "sequencing/TripleBuffer.h"

#include <array>

// M18 Triple buffer: wait-free latest-value hand-off (sec. 17, arch. 3.12).

using phu::sequencing::TripleBuffer;

TEST_CASE("TripleBuffer: consumer sees nothing new before the first publish", "[M18]") {
    TripleBuffer<int> buffer(7);
    CHECK_FALSE(buffer.update());
    CHECK(buffer.read() == 7);
}

TEST_CASE("TripleBuffer: consumer takes only the latest published value", "[M18]") {
    TripleBuffer<std::array<int, 4>> buffer;
    buffer.write({1, 1, 1, 1});
    buffer.write({2, 2, 2, 2});
    buffer.write({3, 3, 3, 3});
    REQUIRE(buffer.update());
    CHECK(buffer.read()[0] == 3);
    CHECK_FALSE(buffer.update());
    CHECK(buffer.read()[3] == 3);

    buffer.write({4, 4, 4, 4});
    REQUIRE(buffer.update());
    CHECK(buffer.read()[0] == 4);
}
