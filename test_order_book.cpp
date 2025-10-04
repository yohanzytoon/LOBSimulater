#include <gtest/gtest.h>
#include <lob/order_book.hpp>
#include <memory>

class OrderBookTest : public ::testing::Test {
protected:
    void SetUp() override {
        book = std::make_unique<lob::OrderBook>("AAPL", 1);
    }

    void TearDown() override {
        book.reset();
    }

    std::unique_ptr<lob::OrderBook> book;
};

// Basic order book functionality tests
TEST_F(OrderBookTest, InitialState) {
    EXPECT_EQ(book->symbol(), "AAPL");
    EXPECT_EQ(book->tick_size(), 1);
    EXPECT_EQ(book->best_bid(), 0);
    EXPECT_EQ(book->best_ask(), 0);
    EXPECT_EQ(book->mid_price(), 0);
    EXPECT_EQ(book->spread(), 0);
    EXPECT_EQ(book->order_count(), 0);
    EXPECT_FALSE(book->is_crossed());
}

TEST_F(OrderBookTest, AddBuyOrder) {
    auto order_id = book->add_order(lob::Side::Buy, 100, 50, lob::OrderType::Limit, "client1");
    
    EXPECT_GT(order_id, 0);
    EXPECT_EQ(book->best_bid(), 100);
    EXPECT_EQ(book->best_bid_quantity(), 50);
    EXPECT_EQ(book->best_ask(), 0);
    EXPECT_EQ(book->order_count(), 1);
}

TEST_F(OrderBookTest, AddSellOrder) {
    auto order_id = book->add_order(lob::Side::Sell, 105, 75, lob::OrderType::Limit, "client2");
    
    EXPECT_GT(order_id, 0);
    EXPECT_EQ(book->best_ask(), 105);
    EXPECT_EQ(book->best_ask_quantity(), 75);
    EXPECT_EQ(book->best_bid(), 0);
    EXPECT_EQ(book->order_count(), 1);
}

TEST_F(OrderBookTest, TwoSidedBook) {
    auto bid_id = book->add_order(lob::Side::Buy, 100, 50, lob::OrderType::Limit);
    auto ask_id = book->add_order(lob::Side::Sell, 105, 75, lob::OrderType::Limit);
    
    EXPECT_EQ(book->best_bid(), 100);
    EXPECT_EQ(book->best_ask(), 105);
    EXPECT_EQ(book->mid_price(), 102);  // (100 + 105) / 2 = 102.5, rounded down to 102
    EXPECT_EQ(book->spread(), 5);
    EXPECT_EQ(book->order_count(), 2);
    EXPECT_FALSE(book->is_crossed());
}

TEST_F(OrderBookTest, OrderCancellation) {
    auto order_id = book->add_order(lob::Side::Buy, 100, 50, lob::OrderType::Limit);
    EXPECT_EQ(book->order_count(), 1);
    EXPECT_EQ(book->best_bid(), 100);
    
    bool cancelled = book->cancel_order(order_id);
    EXPECT_TRUE(cancelled);
    EXPECT_EQ(book->order_count(), 0);
    EXPECT_EQ(book->best_bid(), 0);
}

TEST_F(OrderBookTest, OrderModification) {
    auto order_id = book->add_order(lob::Side::Buy, 100, 50, lob::OrderType::Limit);
    EXPECT_EQ(book->best_bid_quantity(), 50);
    
    bool modified = book->modify_order(order_id, 101, 75);
    EXPECT_TRUE(modified);
    EXPECT_EQ(book->best_bid(), 101);
    EXPECT_EQ(book->best_bid_quantity(), 75);
}

TEST_F(OrderBookTest, MultipleLevels) {
    // Add multiple bid levels
    book->add_order(lob::Side::Buy, 100, 50, lob::OrderType::Limit);
    book->add_order(lob::Side::Buy, 99, 30, lob::OrderType::Limit);
    book->add_order(lob::Side::Buy, 98, 40, lob::OrderType::Limit);
    
    // Add multiple ask levels
    book->add_order(lob::Side::Sell, 105, 25, lob::OrderType::Limit);
    book->add_order(lob::Side::Sell, 106, 35, lob::OrderType::Limit);
    book->add_order(lob::Side::Sell, 107, 45, lob::OrderType::Limit);
    
    EXPECT_EQ(book->best_bid(), 100);
    EXPECT_EQ(book->best_ask(), 105);
    
    auto bid_levels = book->get_bid_levels(3);
    auto ask_levels = book->get_ask_levels(3);
    
    EXPECT_EQ(bid_levels.size(), 3);
    EXPECT_EQ(ask_levels.size(), 3);
    
    // Check bid levels are sorted descending
    EXPECT_EQ(bid_levels[0]->price, 100);
    EXPECT_EQ(bid_levels[1]->price, 99);
    EXPECT_EQ(bid_levels[2]->price, 98);
    
    // Check ask levels are sorted ascending
    EXPECT_EQ(ask_levels[0]->price, 105);
    EXPECT_EQ(ask_levels[1]->price, 106);
    EXPECT_EQ(ask_levels[2]->price, 107);
}

TEST_F(OrderBookTest, MarketOrderExecution) {
    // Set up the book
    book->add_order(lob::Side::Sell, 105, 30, lob::OrderType::Limit);
    book->add_order(lob::Side::Sell, 106, 40, lob::OrderType::Limit);
    
    // Add market buy order
    auto market_order_id = book->add_order(lob::Side::Buy, 0, 25, lob::OrderType::Market);
    
    // Check that trades were generated
    auto trades = book->get_trades();
    EXPECT_GT(trades.size(), 0);
    
    // First ask level should have reduced quantity
    EXPECT_EQ(book->best_ask(), 105);
    EXPECT_EQ(book->best_ask_quantity(), 5);  // 30 - 25 = 5
}

TEST_F(OrderBookTest, PriceTimePriority) {
    // Add orders at the same price level
    auto order1_id = book->add_order(lob::Side::Buy, 100, 30, lob::OrderType::Limit, "client1");
    auto order2_id = book->add_order(lob::Side::Buy, 100, 20, lob::OrderType::Limit, "client2");
    auto order3_id = book->add_order(lob::Side::Buy, 100, 25, lob::OrderType::Limit, "client3");
    
    EXPECT_EQ(book->best_bid_quantity(), 75);  // 30 + 20 + 25
    
    // Get the level and check order sequence
    auto bid_levels = book->get_bid_levels(1);
    ASSERT_EQ(bid_levels.size(), 1);
    
    const auto& level = bid_levels[0];
    EXPECT_EQ(level->orders.size(), 3);
    
    // Orders should be in time priority (first in, first out)
    EXPECT_EQ(level->orders[0]->order_id, order1_id);
    EXPECT_EQ(level->orders[1]->order_id, order2_id);
    EXPECT_EQ(level->orders[2]->order_id, order3_id);
}

TEST_F(OrderBookTest, CrossedBook) {
    // Add orders that would cross the book
    book->add_order(lob::Side::Buy, 100, 50, lob::OrderType::Limit);
    
    // Add aggressive sell order that crosses
    auto sell_order_id = book->add_order(lob::Side::Sell, 100, 30, lob::OrderType::Limit);
    
    // Should generate a trade
    auto trades = book->get_trades();
    EXPECT_GT(trades.size(), 0);
    
    // Check the trade details
    const auto& trade = trades[0];
    EXPECT_EQ(trade.price, 100);
    EXPECT_EQ(trade.quantity, 30);
    EXPECT_EQ(trade.aggressor_side, lob::Side::Sell);
}

TEST_F(OrderBookTest, BookStatistics) {
    // Add some orders
    book->add_order(lob::Side::Buy, 100, 50, lob::OrderType::Limit);
    book->add_order(lob::Side::Buy, 99, 30, lob::OrderType::Limit);
    book->add_order(lob::Side::Sell, 105, 40, lob::OrderType::Limit);
    book->add_order(lob::Side::Sell, 106, 35, lob::OrderType::Limit);
    
    auto stats = book->get_stats();
    
    EXPECT_EQ(stats.total_orders, 4);
    EXPECT_EQ(stats.bid_levels, 2);
    EXPECT_EQ(stats.ask_levels, 2);
    EXPECT_EQ(stats.total_bid_quantity, 80);  // 50 + 30
    EXPECT_EQ(stats.total_ask_quantity, 75);  // 40 + 35
    EXPECT_EQ(stats.total_trades, 0);
}

TEST_F(OrderBookTest, StressTest) {
    // Add many orders to test performance
    const int num_orders = 1000;
    
    for (int i = 0; i < num_orders; ++i) {
        lob::Price price = 100 + (i % 10) - 5;  // Prices from 95 to 104
        lob::Side side = (i % 2 == 0) ? lob::Side::Buy : lob::Side::Sell;
        
        if (side == lob::Side::Sell) {
            price += 10;  // Ensure no crossing
        }
        
        book->add_order(side, price, 10 + (i % 20), lob::OrderType::Limit, "client" + std::to_string(i));
    }
    
    EXPECT_EQ(book->order_count(), num_orders);
    EXPECT_GT(book->best_bid(), 0);
    EXPECT_GT(book->best_ask(), 0);
    EXPECT_LT(book->best_bid(), book->best_ask());
}

// Test order retrieval
TEST_F(OrderBookTest, OrderRetrieval) {
    auto order_id = book->add_order(lob::Side::Buy, 100, 50, lob::OrderType::Limit, "test_client");
    
    auto order = book->get_order(order_id);
    ASSERT_NE(order, nullptr);
    
    EXPECT_EQ(order->order_id, order_id);
    EXPECT_EQ(order->symbol, "AAPL");
    EXPECT_EQ(order->side, lob::Side::Buy);
    EXPECT_EQ(order->price, 100);
    EXPECT_EQ(order->quantity, 50);
    EXPECT_EQ(order->client_id, "test_client");
    EXPECT_TRUE(order->is_active());
    EXPECT_EQ(order->remaining_quantity(), 50);
}

// Test invalid operations
TEST_F(OrderBookTest, InvalidOperations) {
    // Try to cancel non-existent order
    EXPECT_FALSE(book->cancel_order(999999));
    
    // Try to modify non-existent order
    EXPECT_FALSE(book->modify_order(999999, 100, 50));
    
    // Try to get non-existent order
    auto order = book->get_order(999999);
    EXPECT_EQ(order, nullptr);
}

// Performance timing test (optional, can be disabled in CI)
TEST_F(OrderBookTest, DISABLED_PerformanceTest) {
    const int num_operations = 100000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Add orders
    for (int i = 0; i < num_operations; ++i) {
        lob::Price price = 100 + (i % 100) - 50;
        book->add_order(lob::Side::Buy, price, 10, lob::OrderType::Limit);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Should be able to add 100k orders in reasonable time (adjust threshold as needed)
    double ops_per_sec = (num_operations * 1000000.0) / duration.count();
    std::cout << "Add operations per second: " << ops_per_sec << std::endl;
    
    EXPECT_GT(ops_per_sec, 50000);  // At least 50k ops/sec
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}