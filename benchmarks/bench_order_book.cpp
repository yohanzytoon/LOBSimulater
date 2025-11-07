#include "lob/order_book.hpp"
#include <iostream>
#include <random>
#include <chrono>
using namespace lob;

int main() {
    OrderBook b{"BENCH"};
    std::mt19937_64 rng(42);
    std::uniform_int_distribution<int> side(0,1);
    std::uniform_int_distribution<int> qty(1, 200);
    const Price mid = doubleToPrice(100.00);
    const int N = 200000;

    auto t0 = std::chrono::steady_clock::now();
    for (int i=0;i<N;i++) {
        const Side s = side(rng)==0 ? Side::BID : Side::ASK;
        const Price p = mid + (s==Side::BID ? -1 : +1) * (i%10);
        b.addOrder(Order{100000+i, p, static_cast<Quantity>(qty(rng)), s, static_cast<Timestamp>(i)});
    }
    auto t1 = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count();
    std::cout << "Added " << N << " orders in " << ms << " ms => " << (N/ms) << " kops/s\n";
}