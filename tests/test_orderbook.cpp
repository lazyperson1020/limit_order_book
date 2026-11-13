#include "lob/OrderBook.hpp"
#include "test_framework.hpp"

#include <stdexcept>

using namespace lob;

TEST_CASE(empty_book_has_no_best_prices) {
    OrderBook book;
    CHECK(!book.bestBid().has_value());
    CHECK(!book.bestAsk().has_value());
    CHECK(book.orderCount() == 0);
}

TEST_CASE(non_crossing_orders_rest_on_book) {
    OrderBook book;
    auto trades1 = book.addLimitOrder(1, Side::Buy, 100, 10);
    auto trades2 = book.addLimitOrder(2, Side::Sell, 105, 5);

    CHECK(trades1.empty());
    CHECK(trades2.empty());
    REQUIRE(book.bestBid().has_value());
    REQUIRE(book.bestAsk().has_value());
    CHECK(*book.bestBid() == 100);
    CHECK(*book.bestAsk() == 105);
    CHECK(book.orderCount() == 2);
}

TEST_CASE(crossing_order_generates_trade_at_resting_price) {
    OrderBook book;
    book.addLimitOrder(1, Side::Sell, 100, 10);
    auto trades = book.addLimitOrder(2, Side::Buy, 101, 4);

    REQUIRE(trades.size() == 1);
    CHECK(trades[0].price == 100); // trade executes at resting (maker) price
    CHECK(trades[0].quantity == 4);
    CHECK(trades[0].makerOrderId == 1);
    CHECK(trades[0].takerOrderId == 2);

    // Resting order partially filled, remainder still on book.
    REQUIRE(book.bestAsk().has_value());
    CHECK(*book.bestAsk() == 100);
    auto depth = book.depth(Side::Sell, 1);
    REQUIRE(depth.size() == 1);
    CHECK(depth[0].totalQuantity == 6);

    // Taker fully filled, nothing rests for it.
    CHECK(!book.hasOrder(2));
}

TEST_CASE(order_fully_filled_across_multiple_resting_orders) {
    OrderBook book;
    book.addLimitOrder(1, Side::Sell, 100, 3);
    book.addLimitOrder(2, Side::Sell, 100, 4);
    book.addLimitOrder(3, Side::Sell, 101, 10);

    auto trades = book.addLimitOrder(4, Side::Buy, 101, 5);

    REQUIRE(trades.size() == 2);
    // FIFO: order 1 (arrived first at 100) fills before order 2.
    CHECK(trades[0].makerOrderId == 1);
    CHECK(trades[0].quantity == 3);
    CHECK(trades[1].makerOrderId == 2);
    CHECK(trades[1].quantity == 2);

    CHECK(!book.hasOrder(1));
    CHECK(book.hasOrder(2)); // order 2 still has 2 remaining
    CHECK(book.hasOrder(3)); // untouched, price 101 never reached
}

TEST_CASE(time_priority_fifo_at_same_price_level) {
    OrderBook book;
    book.addLimitOrder(1, Side::Buy, 100, 5);
    book.addLimitOrder(2, Side::Buy, 100, 5);
    book.addLimitOrder(3, Side::Buy, 100, 5);

    auto trades = book.addLimitOrder(4, Side::Sell, 100, 12);

    REQUIRE(trades.size() == 3);
    CHECK(trades[0].makerOrderId == 1);
    CHECK(trades[0].quantity == 5);
    CHECK(trades[1].makerOrderId == 2);
    CHECK(trades[1].quantity == 5);
    CHECK(trades[2].makerOrderId == 3);
    CHECK(trades[2].quantity == 2);
}

TEST_CASE(cancel_removes_order_and_empty_level) {
    OrderBook book;
    book.addLimitOrder(1, Side::Buy, 100, 10);

    CHECK(book.cancelOrder(1));
    CHECK(!book.hasOrder(1));
    CHECK(!book.bestBid().has_value());
}

TEST_CASE(cancel_unknown_id_returns_false) {
    OrderBook book;
    CHECK(!book.cancelOrder(999));
}

TEST_CASE(cancel_leaves_other_orders_at_level_intact) {
    OrderBook book;
    book.addLimitOrder(1, Side::Buy, 100, 5);
    book.addLimitOrder(2, Side::Buy, 100, 7);

    CHECK(book.cancelOrder(1));
    auto depth = book.depth(Side::Buy, 1);
    REQUIRE(depth.size() == 1);
    CHECK(depth[0].totalQuantity == 7);
    CHECK(depth[0].orderCount == 1);
}

TEST_CASE(modify_order_changes_price_and_loses_priority) {
    OrderBook book;
    book.addLimitOrder(1, Side::Buy, 100, 5);
    book.addLimitOrder(2, Side::Buy, 100, 5);

    // Order 1 moves to the back of the 100 level by modifying (even to the same price).
    book.modifyOrder(1, 100, 8);

    auto trades = book.addLimitOrder(3, Side::Sell, 100, 5);
    REQUIRE(trades.size() == 1);
    CHECK(trades[0].makerOrderId == 2); // order 2 now has priority
}

TEST_CASE(modify_unknown_id_is_noop) {
    OrderBook book;
    auto trades = book.modifyOrder(42, 100, 5);
    CHECK(trades.empty());
}

TEST_CASE(ioc_order_discards_unfilled_remainder) {
    OrderBook book;
    book.addLimitOrder(1, Side::Sell, 100, 3);

    auto trades = book.addLimitOrder(2, Side::Buy, 100, 10, TimeInForce::IOC);

    REQUIRE(trades.size() == 1);
    CHECK(trades[0].quantity == 3);
    CHECK(!book.hasOrder(2)); // remaining 7 was discarded, not rested
    CHECK(!book.bestAsk().has_value());
}

TEST_CASE(fok_order_kills_when_not_fully_fillable) {
    OrderBook book;
    book.addLimitOrder(1, Side::Sell, 100, 3);

    auto trades = book.addLimitOrder(2, Side::Buy, 100, 10, TimeInForce::FOK);

    CHECK(trades.empty());
    CHECK(!book.hasOrder(2));
    // The resting order must be untouched since the FOK never executed.
    REQUIRE(book.bestAsk().has_value());
    CHECK(*book.bestAsk() == 100);
    auto depth = book.depth(Side::Sell, 1);
    REQUIRE(depth.size() == 1);
    CHECK(depth[0].totalQuantity == 3);
}

TEST_CASE(fok_order_executes_fully_when_fillable) {
    OrderBook book;
    book.addLimitOrder(1, Side::Sell, 100, 6);
    book.addLimitOrder(2, Side::Sell, 101, 6);

    auto trades = book.addLimitOrder(3, Side::Buy, 101, 10, TimeInForce::FOK);

    REQUIRE(trades.size() == 2);
    CHECK(trades[0].quantity == 6);
    CHECK(trades[1].quantity == 4);
    CHECK(!book.hasOrder(3));
}

TEST_CASE(market_order_sweeps_multiple_levels) {
    OrderBook book;
    book.addLimitOrder(1, Side::Sell, 100, 3);
    book.addLimitOrder(2, Side::Sell, 101, 3);
    book.addLimitOrder(3, Side::Sell, 102, 3);

    auto trades = book.addMarketOrder(4, Side::Buy, 8);

    REQUIRE(trades.size() == 3);
    CHECK(trades[0].price == 100);
    CHECK(trades[1].price == 101);
    CHECK(trades[2].price == 102);
    CHECK(trades[2].quantity == 2);
    CHECK(!book.hasOrder(4));
}

TEST_CASE(market_order_discards_remainder_when_book_exhausted) {
    OrderBook book;
    book.addLimitOrder(1, Side::Sell, 100, 3);

    auto trades = book.addMarketOrder(2, Side::Buy, 10);

    REQUIRE(trades.size() == 1);
    CHECK(trades[0].quantity == 3);
    CHECK(!book.hasOrder(2));
    CHECK(!book.bestAsk().has_value());
}

TEST_CASE(duplicate_order_id_throws) {
    OrderBook book;
    book.addLimitOrder(1, Side::Buy, 100, 5);

    bool threw = false;
    try {
        book.addLimitOrder(1, Side::Sell, 105, 5);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
}

TEST_CASE(depth_respects_max_levels_and_price_ordering) {
    OrderBook book;
    book.addLimitOrder(1, Side::Buy, 100, 1);
    book.addLimitOrder(2, Side::Buy, 99, 1);
    book.addLimitOrder(3, Side::Buy, 101, 1);

    auto depth = book.depth(Side::Buy, 2);
    REQUIRE(depth.size() == 2);
    CHECK(depth[0].price == 101); // best bid first (descending)
    CHECK(depth[1].price == 100);
}

TEST_CASE(modify_with_zero_quantity_throws_and_keeps_original_order) {
    OrderBook book;
    book.addLimitOrder(1, Side::Buy, 100, 5);

    bool threw = false;
    try {
        book.modifyOrder(1, 101, 0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
    CHECK(book.hasOrder(1));
    REQUIRE(book.bestBid().has_value());
    CHECK(*book.bestBid() == 100);
}
