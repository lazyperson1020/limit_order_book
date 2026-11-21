#pragma once

#include <bits/stdc++.h>

using namespace std;

namespace lob {

using OrderId = uint64_t;
using Price = int64_t;      // integer ticks, no floating point
using Quantity = uint64_t;
using Timestamp = uint64_t; // sequence number, not wall-clock

enum class Side { Buy, Sell };

enum class TimeInForce {
    GTC, // good till cancel: remainder rests
    IOC, // immediate or cancel: remainder dropped
    FOK  // fill or kill: all or nothing
};

struct Order {
    OrderId id{};
    Side side{};
    Price price{};
    Quantity quantity{}; // remaining quantity
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
