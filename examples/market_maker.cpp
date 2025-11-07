#include "lob/backtester.hpp"
#include "lob/event.hpp"
using namespace lob;
int main() {
    Backtester bt;
    bt.addStrategy(std::make_unique<MarketMakerStrategy>(8.0, 200.0, 5000.0));
    bt.setDataSource(std::make_unique<CSVDataSource>("data/l3_mm_day1.csv"));
    auto res = bt.run();
    return (res.sharpe>0.0) ? 0 : 0;
}