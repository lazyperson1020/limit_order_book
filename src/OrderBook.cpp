#include "lob/OrderBook.hpp"

#include <bits/stdc++.h>

using namespace std;

namespace lob {

vector<Trade> OrderBook::addLimitOrder(OrderId id, Side side, Price price, Quantity quantity,
                                             TimeInForce tif) {
    if (locations_.count(id)) {
        throw invalid_argument("addLimitOrder: duplicate order id");
    }
    if (quantity == 0) {
        throw invalid_argument("addLimitOrder: quantity must be positive");
    }

    vector<Trade> trades;

    if (tif == TimeInForce::FOK && !canFullyFill(side, price, quantity)) {
        return trades; // killed
    }

    Quantity remaining = (side == Side::Buy) ? matchAgainstAsks(id, price, quantity, trades)
                                              : matchAgainstBids(id, price, quantity, trades);

    if (remaining > 0 && tif == TimeInForce::GTC) {
        insertResting(side, id, price, remaining);
    }

    return trades;
}

vector<Trade> OrderBook::addMarketOrder(OrderId id, Side side, Quantity quantity) {
    if (locations_.count(id)) {
        throw invalid_argument("addMarketOrder: duplicate order id");
    }
    if (quantity == 0) {
        throw invalid_argument("addMarketOrder: quantity must be positive");
    }

    vector<Trade> trades;
    if (side == Side::Buy) {
        matchAgainstAsks(id, nullopt, quantity, trades);
    } else {
        matchAgainstBids(id, nullopt, quantity, trades);
    }
    return trades;
}

bool OrderBook::cancelOrder(OrderId id) {
    auto locIt = locations_.find(id);
    if (locIt == locations_.end()) {
        return false;
    }

    const OrderLocation& loc = locIt->second;
    if (loc.side == Side::Buy) {
        auto bookIt = bids_.find(loc.price);
        bookIt->second.erase(loc.it);
        if (bookIt->second.empty()) {
            bids_.erase(bookIt);
        }
    } else {
        auto bookIt = asks_.find(loc.price);
        bookIt->second.erase(loc.it);
        if (bookIt->second.empty()) {
            asks_.erase(bookIt);
        }
    }

    locations_.erase(locIt);
    return true;
}

vector<Trade> OrderBook::modifyOrder(OrderId id, Price newPrice, Quantity newQuantity) {
    auto locIt = locations_.find(id);
    if (locIt == locations_.end()) {
        return {};
    }
    // Check first so a bad modify keeps the order.
    if (newQuantity == 0) {
        throw invalid_argument("modifyOrder: quantity must be positive");
    }
    Side side = locIt->second.side;
    cancelOrder(id);
    return addLimitOrder(id, side, newPrice, newQuantity, TimeInForce::GTC);
}

optional<Price> OrderBook::bestBid() const {
    if (bids_.empty()) return nullopt;
    return bids_.begin()->first;
}

optional<Price> OrderBook::bestAsk() const {
    if (asks_.empty()) return nullopt;
    return asks_.begin()->first;
}

vector<PriceLevelInfo> OrderBook::depth(Side side, size_t maxLevels) const {
    vector<PriceLevelInfo> result;

    auto collect = [&](const auto& book) {
        for (auto it = book.begin(); it != book.end() && result.size() < maxLevels; ++it) {
            Quantity total = 0;
            for (const Order& o : it->second) total += o.quantity;
            result.push_back(PriceLevelInfo{it->first, total, it->second.size()});
        }
    };

    if (side == Side::Buy) collect(bids_);
    else collect(asks_);

    return result;
}

bool OrderBook::hasOrder(OrderId id) const {
    return locations_.find(id) != locations_.end();
}

bool OrderBook::canFullyFill(Side takerSide, Price limitPrice, Quantity quantity) const {
    Quantity available = 0;

    if (takerSide == Side::Buy) {
        for (auto it = asks_.begin(); it != asks_.end() && available < quantity; ++it) {
            if (it->first > limitPrice) break;
            for (const Order& o : it->second) available += o.quantity;
        }
    } else {
        for (auto it = bids_.begin(); it != bids_.end() && available < quantity; ++it) {
            if (it->first < limitPrice) break;
            for (const Order& o : it->second) available += o.quantity;
        }
    }

    return available >= quantity;
}

Quantity OrderBook::matchAgainstAsks(OrderId takerId, optional<Price> limitPrice,
                                      Quantity quantity, vector<Trade>& trades) {
    Quantity remaining = quantity;

    while (remaining > 0 && !asks_.empty()) {
        auto levelIt = asks_.begin();
        Price levelPrice = levelIt->first;
        if (limitPrice && levelPrice > *limitPrice) break;

        Level& level = levelIt->second;
        while (remaining > 0 && !level.empty()) {
            Order& resting = level.front();
            Quantity traded = min(remaining, resting.quantity);

            trades.push_back(Trade{takerId, resting.id, Side::Buy, levelPrice, traded, nextTimestamp()});

            remaining -= traded;
            resting.quantity -= traded;

            if (resting.quantity == 0) {
                locations_.erase(resting.id);
                level.pop_front();
            }
        }

        if (level.empty()) {
            asks_.erase(levelIt);
        }
    }

    return remaining;
}

Quantity OrderBook::matchAgainstBids(OrderId takerId, optional<Price> limitPrice,
                                      Quantity quantity, vector<Trade>& trades) {
    Quantity remaining = quantity;

    while (remaining > 0 && !bids_.empty()) {
        auto levelIt = bids_.begin();
        Price levelPrice = levelIt->first;
        if (limitPrice && levelPrice < *limitPrice) break;

        Level& level = levelIt->second;
        while (remaining > 0 && !level.empty()) {
            Order& resting = level.front();
            Quantity traded = min(remaining, resting.quantity);

            trades.push_back(Trade{takerId, resting.id, Side::Sell, levelPrice, traded, nextTimestamp()});

            remaining -= traded;
            resting.quantity -= traded;

            if (resting.quantity == 0) {
                locations_.erase(resting.id);
                level.pop_front();
            }
        }

        if (level.empty()) {
            bids_.erase(levelIt);
        }
    }

    return remaining;
}

void OrderBook::insertResting(Side side, OrderId id, Price price, Quantity quantity) {
    Timestamp ts = nextTimestamp();

    if (side == Side::Buy) {
        Level& level = bids_[price];
        level.push_back(Order{id, side, price, quantity, ts});
        locations_[id] = OrderLocation{side, price, prev(level.end())};
    } else {
        Level& level = asks_[price];
        level.push_back(Order{id, side, price, quantity, ts});
        locations_[id] = OrderLocation{side, price, prev(level.end())};
    }
}

} // namespace lob
