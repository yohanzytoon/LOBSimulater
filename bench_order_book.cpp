#include <benchmark/benchmark.h>
#include <lob/order_book.hpp>
#include <lob/signals.hpp>
#include <random>
#include <memory>

// Global setup for benchmarks
class OrderBookFixture : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) override {
        book = std::make_unique<lob::OrderBook>("BENCH", 1);
        
        // Seed random number generator
        rng.seed(42);
        price_dist = std::uniform_int_distribution<lob::Price>(90, 110);
        quantity_dist = std::uniform_int_distribution<lob::Quantity>(10, 1000);
        side_dist = std::uniform_int_distribution<int>(0, 1);
        
        // Pre-populate the book with some orders for realistic benchmarking
        for (int i = 0; i < 100; ++i) {
            lob::Price price = 100 + (i % 10) - 5;
            lob::Side side = (i % 2 == 0) ? lob::Side::Buy : lob::Side::Sell;
            
            if (side == lob::Side::Sell) {
                price += 10;  // Ensure no crossing for initial setup
            }
            
            book->add_order(side, price, 50 + (i % 50), lob::OrderType::Limit);
        }
    }

    void TearDown(const ::benchmark::State& state) override {
        book.reset();
    }

protected:
    std::unique_ptr<lob::OrderBook> book;
    std::mt19937 rng;
    std::uniform_int_distribution<lob::Price> price_dist;
    std::uniform_int_distribution<lob::Quantity> quantity_dist;
    std::uniform_int_distribution<int> side_dist;
};

// Benchmark order addition
BENCHMARK_F(OrderBookFixture, AddOrder)(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        lob::Price price = price_dist(rng);
        lob::Quantity quantity = quantity_dist(rng);
        lob::Side side = static_cast<lob::Side>(side_dist(rng));
        state.ResumeTiming();
        
        auto order_id = book->add_order(side, price, quantity, lob::OrderType::Limit);
        benchmark::DoNotOptimize(order_id);
    }
    
    state.SetItemsProcessed(state.iterations());
}

// Benchmark order cancellation
BENCHMARK_F(OrderBookFixture, CancelOrder)(benchmark::State& state) {
    std::vector<lob::OrderId> order_ids;
    
    // Pre-add orders to cancel
    for (int i = 0; i < 10000; ++i) {
        auto order_id = book->add_order(lob::Side::Buy, 100 - (i % 5), 50, lob::OrderType::Limit);
        order_ids.push_back(order_id);
    }
    
    size_t index = 0;
    for (auto _ : state) {
        if (index >= order_ids.size()) {
            state.SkipWithError("Not enough orders to cancel");
            break;
        }
        
        bool cancelled = book->cancel_order(order_ids[index++]);
        benchmark::DoNotOptimize(cancelled);
    }
    
    state.SetItemsProcessed(state.iterations());
}

// Benchmark best bid/ask retrieval
BENCHMARK_F(OrderBookFixture, BestBidAsk)(benchmark::State& state) {
    for (auto _ : state) {
        auto bid = book->best_bid();
        auto ask = book->best_ask();
        benchmark::DoNotOptimize(bid);
        benchmark::DoNotOptimize(ask);
    }
    
    state.SetItemsProcessed(state.iterations() * 2);  // Two operations per iteration
}

// Benchmark market data access (L2)
BENCHMARK_F(OrderBookFixture, GetL2Data)(benchmark::State& state) {
    const int levels = state.range(0);
    
    for (auto _ : state) {
        auto bid_levels = book->get_bid_levels(levels);
        auto ask_levels = book->get_ask_levels(levels);
        benchmark::DoNotOptimize(bid_levels);
        benchmark::DoNotOptimize(ask_levels);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(OrderBookFixture, GetL2Data)->Arg(5)->Arg(10)->Arg(20);

// Benchmark order matching (market orders)
BENCHMARK_F(OrderBookFixture, MarketOrder)(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        lob::Quantity quantity = quantity_dist(rng);
        lob::Side side = static_cast<lob::Side>(side_dist(rng));
        state.ResumeTiming();
        
        auto order_id = book->add_order(side, 0, quantity, lob::OrderType::Market);
        benchmark::DoNotOptimize(order_id);
    }
    
    state.SetItemsProcessed(state.iterations());
}

// Benchmark microstructure signals calculation
BENCHMARK_F(OrderBookFixture, MicrostructureSignals)(benchmark::State& state) {
    lob::Signals signals(book.get());
    
    for (auto _ : state) {
        auto imbalance = signals.order_imbalance();
        auto microprice = signals.microprice();
        auto weighted_mid = signals.weighted_mid_price();
        auto pressure = signals.book_pressure();
        
        benchmark::DoNotOptimize(imbalance);
        benchmark::DoNotOptimize(microprice);
        benchmark::DoNotOptimize(weighted_mid);
        benchmark::DoNotOptimize(pressure);
    }
    
    state.SetItemsProcessed(state.iterations());
}

// Benchmark comprehensive market quality metrics
BENCHMARK_F(OrderBookFixture, MarketQualityMetrics)(benchmark::State& state) {
    lob::Signals signals(book.get());
    
    for (auto _ : state) {
        auto metrics = signals.get_market_quality();
        benchmark::DoNotOptimize(metrics);
    }
    
    state.SetItemsProcessed(state.iterations());
}

// Test different order book sizes
static void BenchmarkOrderBookSize(benchmark::State& state) {
    const int book_size = state.range(0);
    auto book = std::make_unique<lob::OrderBook>("SIZE_BENCH", 1);
    
    // Fill the book with specified number of orders
    for (int i = 0; i < book_size; ++i) {
        lob::Price price = 100 + (i % 100) - 50;
        lob::Side side = (i % 2 == 0) ? lob::Side::Buy : lob::Side::Sell;
        
        if (side == lob::Side::Sell) {
            price += 100;  // Ensure no crossing
        }
        
        book->add_order(side, price, 50, lob::OrderType::Limit);
    }
    
    std::mt19937 rng(42);
    std::uniform_int_distribution<lob::Price> price_dist(50, 150);
    std::uniform_int_distribution<lob::Quantity> quantity_dist(10, 100);
    std::uniform_int_distribution<int> side_dist(0, 1);
    
    for (auto _ : state) {
        state.PauseTiming();
        lob::Price price = price_dist(rng);
        lob::Quantity quantity = quantity_dist(rng);
        lob::Side side = static_cast<lob::Side>(side_dist(rng));
        if (side == lob::Side::Sell) price += 100;  // Avoid crossing
        state.ResumeTiming();
        
        auto order_id = book->add_order(side, price, quantity, lob::OrderType::Limit);
        benchmark::DoNotOptimize(order_id);
    }
    
    state.SetComplexityN(book_size);
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BenchmarkOrderBookSize)
    ->Range(100, 10000)
    ->Complexity(benchmark::oAuto);

// Memory usage benchmark
BENCHMARK_F(OrderBookFixture, MemoryFootprint)(benchmark::State& state) {
    const int orders_per_iteration = state.range(0);
    
    for (auto _ : state) {
        state.PauseTiming();
        std::vector<lob::OrderId> order_ids;
        order_ids.reserve(orders_per_iteration);
        state.ResumeTiming();
        
        // Add orders
        for (int i = 0; i < orders_per_iteration; ++i) {
            auto order_id = book->add_order(
                static_cast<lob::Side>(i % 2),
                100 + (i % 20) - 10,
                50,
                lob::OrderType::Limit
            );
            order_ids.push_back(order_id);
        }
        
        state.PauseTiming();
        // Clean up
        for (auto order_id : order_ids) {
            book->cancel_order(order_id);
        }
        state.ResumeTiming();
    }
    
    state.SetItemsProcessed(state.iterations() * orders_per_iteration);
}
BENCHMARK_REGISTER_F(OrderBookFixture, MemoryFootprint)
    ->Arg(100)
    ->Arg(1000)
    ->Arg(10000);

// Mixed operations benchmark (realistic trading scenario)
BENCHMARK_F(OrderBookFixture, MixedOperations)(benchmark::State& state) {
    std::vector<lob::OrderId> active_orders;
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> operation_dist(0, 9);  // 0-6: add, 7-8: cancel, 9: market
    
    for (auto _ : state) {
        int operation = operation_dist(rng);
        
        if (operation <= 6) {
            // Add order (70% of operations)
            state.PauseTiming();
            lob::Price price = price_dist(rng);
            lob::Quantity quantity = quantity_dist(rng);
            lob::Side side = static_cast<lob::Side>(side_dist(rng));
            if (side == lob::Side::Sell) price += 20;  // Reduce crossing probability
            state.ResumeTiming();
            
            auto order_id = book->add_order(side, price, quantity, lob::OrderType::Limit);
            active_orders.push_back(order_id);
            benchmark::DoNotOptimize(order_id);
            
        } else if (operation <= 8 && !active_orders.empty()) {
            // Cancel order (20% of operations)
            state.PauseTiming();
            std::uniform_int_distribution<size_t> index_dist(0, active_orders.size() - 1);
            size_t index = index_dist(rng);
            auto order_id = active_orders[index];
            active_orders.erase(active_orders.begin() + index);
            state.ResumeTiming();
            
            bool cancelled = book->cancel_order(order_id);
            benchmark::DoNotOptimize(cancelled);
            
        } else {
            // Market order (10% of operations)
            state.PauseTiming();
            lob::Quantity quantity = std::min(quantity_dist(rng), static_cast<lob::Quantity>(100));
            lob::Side side = static_cast<lob::Side>(side_dist(rng));
            state.ResumeTiming();
            
            auto order_id = book->add_order(side, 0, quantity, lob::OrderType::Market);
            benchmark::DoNotOptimize(order_id);
        }
    }
    
    state.SetItemsProcessed(state.iterations());
}

// Latency percentiles benchmark
BENCHMARK_F(OrderBookFixture, LatencyPercentiles)(benchmark::State& state) {
    std::vector<double> latencies;
    latencies.reserve(state.max_iterations);
    
    for (auto _ : state) {
        auto start = std::chrono::high_resolution_clock::now();
        
        auto order_id = book->add_order(
            lob::Side::Buy, 
            price_dist(rng), 
            quantity_dist(rng), 
            lob::OrderType::Limit
        );
        benchmark::DoNotOptimize(order_id);
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        latencies.push_back(duration.count());
    }
    
    // Calculate percentiles
    if (!latencies.empty()) {
        std::sort(latencies.begin(), latencies.end());
        
        auto p50 = latencies[latencies.size() * 50 / 100];
        auto p95 = latencies[latencies.size() * 95 / 100];
        auto p99 = latencies[latencies.size() * 99 / 100];
        
        state.counters["P50_ns"] = benchmark::Counter(p50);
        state.counters["P95_ns"] = benchmark::Counter(p95);
        state.counters["P99_ns"] = benchmark::Counter(p99);
    }
    
    state.SetItemsProcessed(state.iterations());
}

// Throughput benchmark
static void BenchmarkThroughput(benchmark::State& state) {
    const int num_threads = state.range(0);
    auto book = std::make_unique<lob::OrderBook>("THROUGHPUT", 1);
    
    std::atomic<int> order_counter{0};
    
    auto worker = [&]() {
        std::mt19937 rng(std::hash<std::thread::id>{}(std::this_thread::get_id()));
        std::uniform_int_distribution<lob::Price> price_dist(95, 105);
        std::uniform_int_distribution<lob::Quantity> quantity_dist(10, 100);
        std::uniform_int_distribution<int> side_dist(0, 1);
        
        while (state.KeepRunning()) {
            lob::Price price = price_dist(rng);
            lob::Quantity quantity = quantity_dist(rng);
            lob::Side side = static_cast<lob::Side>(side_dist(rng));
            
            // Note: This is not thread-safe! This benchmark is mainly to show
            // the structure for testing single-threaded throughput
            auto order_id = book->add_order(side, price, quantity, lob::OrderType::Limit);
            benchmark::DoNotOptimize(order_id);
            order_counter++;
        }
    };
    
    if (num_threads == 1) {
        worker();
    } else {
        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back(worker);
        }
        for (auto& t : threads) {
            t.join();
        }
    }
    
    state.SetItemsProcessed(order_counter.load());
    state.counters["Orders/sec"] = benchmark::Counter(
        order_counter.load(), benchmark::Counter::kIsRate
    );
}
BENCHMARK(BenchmarkThroughput)->Arg(1)->UseRealTime();

// Register custom counters
BENCHMARK_MAIN();