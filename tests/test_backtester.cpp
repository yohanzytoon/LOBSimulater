#include <catch2/catch_all.hpp>
#include "lob/backtester.hpp"
#include "lob/event.hpp"

using namespace lob;

TEST_CASE("Backtester can run empty CSV") {
    Backtester bt;
    // Intentionally no data source -> run should return default result
    auto res = bt.run();
    REQUIRE(res.num_trades == 0);
}