#include "lob/backtester.hpp"
#include "lob/event.hpp"
#include <iostream>

using namespace lob;

int main() {
    Backtester bt;
    // Add a default market maker strategy. Parameters can be tuned later.
    bt.addStrategy(std::make_unique<MarketMakerStrategy>());
    // Supply a path to a sample CSV file. This should be replaced with real data.
    bt.setDataSource(std::make_unique<CSVDataSource>("data/sample_l3.csv"));
    auto res = bt.run();
    std::cout << "Sharpe: " << res.sharpe << "  MaxDD: " << res.max_drawdown << "\n";
    return 0;
}