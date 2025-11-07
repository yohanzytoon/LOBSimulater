#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <map>
#include <memory>
#include <string>
#include <optional>
#include <chrono>
#include <algorithm>
#include <numeric>

namespace lob {

// Forward declarations
class Order;
class PriceLevel;
class OrderBook;

// Type aliases for clarity and performance
using OrderId = uint64_t;
using Price = int64_t;  // Price in ticks (1 tick = 0.01 cents)
using Quantity = uint32_t;
using Timestamp = uint64_t;  // Nanoseconds since epoch

// Constants for optimization
inline constexpr size_t CACHE_LINE_SIZE = 64;
inline constexpr size_t EXPECTED_ORDERS_PER_LEVEL = 16;
inline constexpr size_t PRICE_LEVEL_RESERVE = 100;

enum class Side : uint8_t {
    BID = 0,
    ASK = 1
};

enum class OrderType : uint8_t {
    LIMIT = 0,
    MARKET = 1,
    STOP = 2,
    STOP_LIMIT = 3
};

enum class TimeInForce : uint8_t {
    GTC = 0,  // Good Till Cancel
    IOC = 1,  // Immediate Or Cancel
    FOK = 2,  // Fill Or Kill
    GTD = 3   // Good Till Date
};

// Cache-aligned order structure
struct alignas(CACHE_LINE_SIZE) Order {
    OrderId id;
    Price price;
    Quantity quantity;
    Quantity remaining_quantity;
    Side side;
    OrderType type;
    TimeInForce tif;
    Timestamp timestamp;
    uint32_t participant_id;
    
    // Intrusive linked list for O(1) removal
    Order* next = nullptr;
    Order* prev = nullptr;
    PriceLevel* level = nullptr;
    
    Order() noexcept = default;
    Order(OrderId id_, Price price_, Quantity qty_, Side side_, Timestamp ts_) noexcept
        : id(id_), price(price_), quantity(qty_), remaining_quantity(qty_),
          side(side_), type(OrderType::LIMIT), tif(TimeInForce::GTC),
          timestamp(ts_), participant_id(0) {}
    
    [[nodiscard]] bool isBuy() const noexcept { return side == Side::BID; }
    [[nodiscard]] bool isSell() const noexcept { return side == Side::ASK; }
    [[nodiscard]] bool isFilled() const noexcept { return remaining_quantity == 0; }
    [[nodiscard]] Quantity filledQuantity() const noexcept { 
        return quantity - remaining_quantity; 
    }
};

// Price level maintains FIFO queue of orders
class PriceLevel {
public:
    Price price;
    Side side;
    Quantity total_quantity = 0;
    uint32_t order_count = 0;
    
    PriceLevel(Price p, Side s) noexcept : price(p), side(s) {}
    
    void addOrder(Order* order) noexcept;
    void removeOrder(Order* order) noexcept;
    void modifyOrder(Order* order, Quantity new_qty) noexcept;
    [[nodiscard]] Order* front() const noexcept { return head_; }
    [[nodiscard]] bool empty() const noexcept { return head_ == nullptr; }
    
private:
    Order* head_ = nullptr;
    Order* tail_ = nullptr;
};

// Execution report for filled orders
struct Execution {
    OrderId bid_id;
    OrderId ask_id;
    Price price;
    Quantity quantity;
    Timestamp timestamp;
    
    Execution(OrderId bid, OrderId ask, Price p, Quantity q, Timestamp ts)
        : bid_id(bid), ask_id(ask), price(p), quantity(q), timestamp(ts) {}
};

// Market data update for L2/L3 feeds
struct MarketDataUpdate {
    enum Type : uint8_t {
        ADD_ORDER,
        MODIFY_ORDER,
        CANCEL_ORDER,
        TRADE,
        CLEAR,
        SNAPSHOT
    };
    
    Type type;
    Side side;
    Price price;
    Quantity quantity;
    OrderId order_id;
    Timestamp timestamp;
};

// Order book statistics for analysis
struct BookStats {
    Price best_bid = 0;
    Price best_ask = 0;
    Quantity bid_volume = 0;
    Quantity ask_volume = 0;
    double spread = 0.0;
    double mid_price = 0.0;
    double microprice = 0.0;
    double imbalance = 0.0;
    uint32_t bid_levels = 0;
    uint32_t ask_levels = 0;
    uint32_t total_orders = 0;
};

// Main Order Book class - optimized for performance
class OrderBook {
public:
    explicit OrderBook(const std::string& symbol);
    ~OrderBook() = default;
    
    // Disable copy, enable move
    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;
    OrderBook(OrderBook&&) = default;
    OrderBook& operator=(OrderBook&&) = default;
    
    // Core order operations
    [[nodiscard]] bool addOrder(Order order) noexcept;
    [[nodiscard]] bool modifyOrder(OrderId id, Quantity new_quantity) noexcept;
    [[nodiscard]] bool cancelOrder(OrderId id) noexcept;
    
    // Market orders and matching
    [[nodiscard]] std::vector<Execution> processMarketOrder(
        Side side, Quantity quantity, Timestamp timestamp) noexcept;
    [[nodiscard]] std::vector<Execution> matchOrders() noexcept;
    
    // Query operations (const‑correct)
    [[nodiscard]] const Order* getOrder(OrderId id) const noexcept;
    [[nodiscard]] Price getBestBid() const noexcept;
    [[nodiscard]] Price getBestAsk() const noexcept;
    [[nodiscard]] double getSpread() const noexcept;
    [[nodiscard]] double getMidPrice() const noexcept;
    
    // Microstructure signals
    [[nodiscard]] double getMicroPrice(int levels = 1) const noexcept;
    [[nodiscard]] double getOrderImbalance(int levels = 5) const noexcept;
    [[nodiscard]] Quantity getQueuePosition(OrderId id) const noexcept;
    [[nodiscard]] BookStats getStats() const noexcept;
    
    // L2/L3 market data
    [[nodiscard]] std::vector<std::pair<Price, Quantity>> 
        getAggregatedBook(Side side, int levels = 10) const noexcept;
    [[nodiscard]] std::vector<Order> 
        getOrdersAtLevel(Price price, Side side) const noexcept;
    
    // Utilities
    void clear() noexcept;
    [[nodiscard]] size_t orderCount() const noexcept { return orders_.size(); }
    [[nodiscard]] const std::string& symbol() const noexcept { return symbol_; }
    
    // Performance metrics
    struct Metrics {
        uint64_t orders_added = 0;
        uint64_t orders_modified = 0;
        uint64_t orders_canceled = 0;
        uint64_t orders_matched = 0;
        uint64_t total_volume = 0;
        std::chrono::nanoseconds total_latency{0};
    };
    
    [[nodiscard]] const Metrics& getMetrics() const noexcept { return metrics_; }
    void resetMetrics() noexcept { metrics_ = Metrics{}; }
    
private:
    std::string symbol_;
    
    // Flat hash map for O(1) order lookup
    std::unordered_map<OrderId, std::unique_ptr<Order>> orders_;
    
    // Red‑black trees for price‑time priority (sorted by price)
    std::map<Price, std::unique_ptr<PriceLevel>, std::greater<>> bid_levels_;
    std::map<Price, std::unique_ptr<PriceLevel>, std::less<>> ask_levels_;
    
    // Cache best prices for fast access
    mutable Price cached_best_bid_ = 0;
    mutable Price cached_best_ask_ = 0;
    mutable bool cache_valid_ = false;
    
    // Performance tracking
    Metrics metrics_;
    
    // Helper methods
    void updateCache() const noexcept;
    void invalidateCache() noexcept { cache_valid_ = false; }
    PriceLevel* getOrCreateLevel(Price price, Side side) noexcept;
    void removeEmptyLevel(Price price, Side side) noexcept;
    
    template<typename Func>
    void executeMatch(Order* bid, Order* ask, Func&& callback) noexcept;
};

// Inline implementations for hot path functions
inline Price OrderBook::getBestBid() const noexcept {
    if (!cache_valid_) updateCache();
    return cached_best_bid_;
}

inline Price OrderBook::getBestAsk() const noexcept {
    if (!cache_valid_) updateCache();
    return cached_best_ask_;
}

inline double OrderBook::getSpread() const noexcept {
    if (!cache_valid_) updateCache();
    if (cached_best_bid_ == 0 || cached_best_ask_ == 0) return 0.0;
    return static_cast<double>(cached_best_ask_ - cached_best_bid_) / 100.0;
}

inline double OrderBook::getMidPrice() const noexcept {
    if (!cache_valid_) updateCache();
    if (cached_best_bid_ == 0 || cached_best_ask_ == 0) return 0.0;
    return static_cast<double>(cached_best_bid_ + cached_best_ask_) / 200.0;
}

// Utility functions
inline Price doubleToPrice(double price) noexcept {
    return static_cast<Price>(price * 100 + 0.5);
}

inline double priceToDouble(Price price) noexcept {
    return static_cast<double>(price) / 100.0;
}

} // namespace lob