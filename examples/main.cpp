#include "lob/OrderBook.hpp"

#include <iostream>

using namespace lob;

namespace {

const char* sideName(Side side) {
    return side == Side::Buy ? "BUY" : "SELL";
}

void printTrades(const std::vector<Trade>& trades) {
    for (const auto& t : trades) {
        std::cout << "  TRADE " << sideName(t.takerSide) << " taker=" << t.takerOrderId << " maker=" << t.makerOrderId
                  << " price=" << t.price << " qty=" << t.quantity << "\n";
    }
}

void printDepth(const OrderBook& book) {
    std::cout << "-- book depth --\n";
    auto asks = book.depth(Side::Sell, 5);
    for (auto it = asks.rbegin(); it != asks.rend(); ++it) {
        std::cout << "  ASK " << it->price << " x " << it->totalQuantity << " (" << it->orderCount << " orders)\n";
    }
    auto bids = book.depth(Side::Buy, 5);
    for (const auto& level : bids) {
        std::cout << "  BID " << level.price << " x " << level.totalQuantity << " (" << level.orderCount << " orders)\n";
    }
}

} // namespace

int main() {
    OrderBook book;

    std::cout << "Adding resting orders...\n";
    book.addLimitOrder(1, Side::Buy, 99, 10);
    book.addLimitOrder(2, Side::Buy, 100, 5);
    book.addLimitOrder(3, Side::Sell, 102, 8);
    book.addLimitOrder(4, Side::Sell, 103, 12);
    printDepth(book);

    std::cout << "\nSubmitting crossing limit order (BUY 6 @ 102)...\n";
    auto trades = book.addLimitOrder(5, Side::Buy, 102, 6);
    printTrades(trades);
    printDepth(book);

    std::cout << "\nSubmitting IOC order (SELL 20 @ 99, IOC)...\n";
    trades = book.addLimitOrder(6, Side::Sell, 99, 20, TimeInForce::IOC);
    printTrades(trades);
    printDepth(book);

    std::cout << "\nCancelling order 4...\n";
    book.cancelOrder(4);
    printDepth(book);

    std::cout << "\nSubmitting market order (BUY 3)...\n";
    trades = book.addMarketOrder(7, Side::Buy, 3);
    printTrades(trades);
    printDepth(book);

    return 0;
}
