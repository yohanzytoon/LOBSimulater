#include <catch2/catch_all.hpp>
#include "lob/order_book.hpp"
#include "lob/signals.hpp"

using namespace lob;

TEST_CASE("Imbalance and microprice") {
    OrderBook b{"X"};
    Timestamp t=1;
    for (int i=0;i<3;i++) b.addOrder(Order{100+i, doubleToPrice(100.00 - i*0.01), 100, Side::BID, t++});
    for (int i=0;i<2;i++) b.addOrder(Order{200+i, doubleToPrice(100.01 + i*0.01), 50,  Side::ASK, t++});
    OrderImbalanceSignal s(3, 0.2);
    auto sig = s.calculate(b);
    REQUIRE(sig.type == Signal::ORDER_IMBALANCE);
    REQUIRE(std::abs(sig.value) > 0.0);
}