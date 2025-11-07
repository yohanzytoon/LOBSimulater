#include "lob/backtester.hpp"
#include "lob/event.hpp"
using namespace lob;
int main() {
    Backtester bt;
    // A simple strategy could be added here to consume signals from SignalGenerator
    bt.setDataSource(std::make_unique<CSVDataSource>("data/l3_exec_day1.csv"));
    auto res = bt.run();
    return 0;
}