#include <catch2/catch_all.hpp>
#include "lob/order_book.hpp"

using namespace lob;

TEST_CASE("Add/Match crossing orders") {
    OrderBook b{"TEST"};
    Timestamp t=1;
    REQUIRE(b.addOrder(Order{1, doubleToPrice(100.00), 100, Side::BID, t++}));
    REQUIRE(b.addOrder(Order{2, doubleToPrice(99.90), 100, Side::ASK, t++}));
    auto execs = b.matchOrders();
    REQUIRE(execs.size()==1);
    REQUIRE(b.orderCount()==0);
    REQUIRE(b.getSpread()==0.0);
}