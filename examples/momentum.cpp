#include "lob/backtester.hpp"
#include "lob/event.hpp"
using namespace lob;
int main() {
    Backtester bt;
    bt.addStrategy(std::make_unique<MomentumStrategy>(30, 1.5, 0.3));
    bt.setDataSource(std::make_unique<CSVDataSource>("data/l2_mo_day1.csv"));
    auto res = bt.run();
    return 0;
}