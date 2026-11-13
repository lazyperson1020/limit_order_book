#pragma once

#include <cstdint>

namespace lob {

using OrderId = std::uint64_t;
using Price = std::int64_t;      // integer ticks; avoids floating-point comparison issues
using Quantity = std::uint64_t;
using Timestamp = std::uint64_t; // monotonically increasing sequence number, not wall-clock

enum class Side { Buy, Sell };

enum class TimeInForce {
    GTC, // Good-Till-Cancel: any unfilled remainder rests on the book
    IOC, // Immediate-Or-Cancel: fills what it can right now, remainder is discarded
    FOK  // Fill-Or-Kill: must fill completely immediately, or nothing executes at all
};

struct Order {
    OrderId id{};
    Side side{};
    Price price{};
    Quantity quantity{}; // remaining (not original) quantity
    Timestamp timestamp{};
};

struct Trade {
    OrderId takerOrderId{};
    OrderId makerOrderId{};
    Side takerSide{};
    Price price{};
    Quantity quantity{};
    Timestamp timestamp{};
};

} // namespace lob
