#pragma once

#include "lob/Types.hpp"

#include <bits/stdc++.h>

using namespace std;

namespace lob {

struct PriceLevelInfo {
    Price price{};
    Quantity totalQuantity{};
    size_t orderCount{};
};

// Price-time priority limit order book.
class OrderBook {
public:
    // Matches what it can, then handles the rest per tif.
    vector<Trade> addLimitOrder(OrderId id, Side side, Price price, Quantity quantity,
                                      TimeInForce tif = TimeInForce::GTC);

    // Matches at any price, never rests.
    vector<Trade> addMarketOrder(OrderId id, Side side, Quantity quantity);

    // Returns false if the id is not on the book.
    bool cancelOrder(OrderId id);

    // Cancel and re-add as GTC. Loses time priority.
    vector<Trade> modifyOrder(OrderId id, Price newPrice, Quantity newQuantity);

    optional<Price> bestBid() const;
    optional<Price> bestAsk() const;

    // Up to maxLevels levels, best price first.
    vector<PriceLevelInfo> depth(Side side, size_t maxLevels) const;

    bool hasOrder(OrderId id) const;
    size_t orderCount() const { return locations_.size(); }

private:
    using Level = list<Order>;
    using BidBook = map<Price, Level, greater<Price>>;
    using AskBook = map<Price, Level, less<Price>>;

    struct OrderLocation {
        Side side;
        Price price;
        Level::iterator it;
    };

    BidBook bids_;
    AskBook asks_;
    unordered_map<OrderId, OrderLocation> locations_;
    Timestamp clock_ = 0;

    Timestamp nextTimestamp() { return ++clock_; }

    // Used by FOK: can the order fill completely right now?
    bool canFullyFill(Side takerSide, Price limitPrice, Quantity quantity) const;

    // No limitPrice means market order. Returns unfilled quantity.
    Quantity matchAgainstAsks(OrderId takerId, optional<Price> limitPrice, Quantity quantity,
                               vector<Trade>& trades);

    Quantity matchAgainstBids(OrderId takerId, optional<Price> limitPrice, Quantity quantity,
                               vector<Trade>& trades);

    void insertResting(Side side, OrderId id, Price price, Quantity quantity);
};

} // namespace lob
