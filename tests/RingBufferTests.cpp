#include "ln/RingBuffer.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("ln::RingBuffer basic push/pop", "[ln::RingBuffer]") {
    std::array<int, 4> storage{};
    ln::RingBuffer<int> rb(storage);

    REQUIRE(rb.empty());
    REQUIRE(rb.size() == 0);
    REQUIRE(rb.capacity() == 4);
    REQUIRE(rb.front() == nullptr);

    REQUIRE(rb.push(1));
    REQUIRE_FALSE(rb.empty());
    REQUIRE(rb.size() == 1);
    REQUIRE(rb.front() != nullptr);
    REQUIRE(*rb.front() == 1);

    auto v = rb.pop();
    REQUIRE(v.has_value());
    REQUIRE(v.value() == 1);
    REQUIRE(rb.empty());
}

TEST_CASE("ln::RingBuffer fill and full", "[ln::RingBuffer]") {
    std::array<int, 3> storage{};
    ln::RingBuffer<int> rb(storage);

    REQUIRE(rb.get_free_space() == 3);

    REQUIRE(rb.push(10));
    REQUIRE(rb.push(20));
    REQUIRE(rb.push(30));

    REQUIRE(rb.full());
    REQUIRE(rb.size() == 3);
    REQUIRE(rb.get_free_space() == 0);

    // Cannot push when full (PushMode::normal)
    REQUIRE_FALSE(rb.push(40));
}

TEST_CASE("ln::RingBuffer preserves FIFO order across wrap-around", "[ln::RingBuffer]") {
    std::array<int, 3> storage{};
    ln::RingBuffer<int> rb(storage);

    // Fill then pop one to force wrap-around on next pushes
    REQUIRE(rb.push(1));
    REQUIRE(rb.push(2));
    REQUIRE(rb.push(3));

    auto v1 = rb.pop();
    REQUIRE(v1.has_value());
    REQUIRE(v1.value() == 1);

    // Now head advanced, tail at capacity, push should wrap tail
    REQUIRE(rb.push(4));

    // Remaining order should be 2,3,4
    auto v2 = rb.pop();
    auto v3 = rb.pop();
    auto v4 = rb.pop();

    REQUIRE(v2.has_value());
    REQUIRE(v3.has_value());
    REQUIRE(v4.has_value());
    REQUIRE(v2.value() == 2);
    REQUIRE(v3.value() == 3);
    REQUIRE(v4.value() == 4);

    REQUIRE(rb.empty());
}

TEST_CASE("ln::RingBuffer::clear resets state", "[ln::RingBuffer]") {
    std::array<int, 4> storage{};
    ln::RingBuffer<int> rb(storage);

    REQUIRE(rb.push(1));
    REQUIRE(rb.push(2));

    rb.clear();

    REQUIRE(rb.empty());
    REQUIRE(rb.size() == 0);
    REQUIRE(rb.get_free_space() == rb.capacity());

    // After clear, should behave as freshly constructed
    REQUIRE(rb.push(3));
    auto v = rb.pop();
    REQUIRE(v.has_value());
    REQUIRE(v.value() == 3);
}

TEST_CASE("ln::RingBuffer::push_overwrite overwrites oldest element", "[ln::RingBuffer]") {
    std::array<int, 3> storage{};
    ln::RingBuffer<int> rb(storage);

    REQUIRE(rb.push(1));
    REQUIRE(rb.push(2));
    REQUIRE(rb.push(3));
    REQUIRE(rb.full());

    REQUIRE(rb.push_overwrite(4)); // overwrite oldest (1)
    REQUIRE(rb.full());
    REQUIRE(rb.size() == 3);
    REQUIRE(rb.capacity() == 3);

    auto v1 = rb.pop();
    auto v2 = rb.pop();
    auto v3 = rb.pop();

    REQUIRE(v1.has_value());
    REQUIRE(v2.has_value());
    REQUIRE(v3.has_value());
    CHECK(*v1 == 2);
    CHECK(*v2 == 3);
    CHECK(*v3 == 4);
}

TEST_CASE("ln::RingBuffer::push(std::span) in normal mode fails when not enough space", "[ln::RingBuffer][span]") {
    std::array<int, 4> storage{};
    ln::RingBuffer<int> rb(storage);

    std::array<int, 2> initial{{1, 2}};
    REQUIRE(rb.push(initial));
    CHECK(rb.size() == 2);

    std::array<int, 3> too_many{{3, 4, 5}};

    REQUIRE_FALSE(rb.push(too_many));
    CHECK(rb.size() == 2);

    auto v1 = rb.pop();
    auto v2 = rb.pop();

    REQUIRE(v1.has_value());
    REQUIRE(v2.has_value());
    CHECK(*v1 == 1);
    CHECK(*v2 == 2);
    CHECK(rb.empty());
}

TEST_CASE("ln::RingBuffer::push(std::span) in normal mode succeeds when enough space", "[ln::RingBuffer][span]") {
    std::array<int, 5> storage{};
    ln::RingBuffer<int> rb(storage);

    std::array<int, 2> first{{1, 2}};
    REQUIRE(rb.push(first));

    std::array<int, 3> second{{3, 4, 5}};
    REQUIRE(rb.push(second));

    CHECK(rb.full());
    CHECK(rb.size() == 5);

    for (int expected : {1, 2, 3, 4, 5}) {
        auto v = rb.pop();
        REQUIRE(v.has_value());
        CHECK(*v == expected);
    }
    CHECK(rb.empty());
}

TEST_CASE("ln::RingBuffer::push(std::span) in overwrite mode replaces oldest elements", "[ln::RingBuffer][span]") {
    std::array<int, 4> storage{};
    ln::RingBuffer<int> rb(storage);

    std::array<int, 3> first{{1, 2, 3}};
    REQUIRE(rb.push(first));
    CHECK(rb.size() == 3);

    std::array<int, 3> overwrite_vals{{4, 5, 6}};

    REQUIRE(rb.push_overwrite(overwrite_vals)); // overwrite 2 oldest
    CHECK(rb.size() == 4);

    auto v1 = rb.pop();
    auto v2 = rb.pop();
    auto v3 = rb.pop();
    auto v4 = rb.pop();

    REQUIRE(v1.has_value());
    REQUIRE(v2.has_value());
    REQUIRE(v3.has_value());
    REQUIRE(v4.has_value());
    CHECK(*v1 == 3);
    CHECK(*v2 == 4);
    CHECK(*v3 == 5);
    CHECK(*v4 == 6);
    CHECK(rb.empty());
}

TEST_CASE("ln::RingBuffer::push(std::span) handles wrap-around correctly", "[ln::RingBuffer][span]") {
    std::array<int, 5> storage{};
    ln::RingBuffer<int> rb(storage);

    std::array<int, 3> a{{1, 2, 3}};
    REQUIRE(rb.push(a));

    // Pop two elements to advance tail and create wrap opportunity
    REQUIRE(rb.pop().has_value());
    REQUIRE(rb.pop().has_value());

    std::array<int, 4> b{{4, 5, 6, 7}};

    // capacity=5, size=1, free=4, pushing 4 -> should wrap internally
    REQUIRE(rb.push(b));
    CHECK(rb.full());

    for (int expected : {3, 4, 5, 6, 7}) {
        auto v = rb.pop();
        REQUIRE(v.has_value());
        CHECK(*v == expected);
    }
    CHECK(rb.empty());
}
