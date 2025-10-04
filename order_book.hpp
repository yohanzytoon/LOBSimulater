#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <robin_hood.h>

namespace lob {

/// Order side enumeration
enum class Side : uint8_t {
    Buy = 0,
    Sell = 1
};

/// Order type enumeration  
enum class OrderType : uint8_t {
    Market = 0,
    Limit = 1,
    Stop = 2,
    StopLimit = 3
};

/// Order status enumeration
enum class OrderStatus : uint8_t {
    New = 0,
    PartiallyFilled = 1,
    Filled = 2,
    Cancelled = 3,
    Rejected = 4,
    Expired = 5
};

/// Price type using fixed-point representation (price in ticks)
using Price = int64_t;

/// Quantity type
using Quantity = uint64_t;

/// Order ID type
using OrderId = uint64_t;

/// Timestamp type (nanoseconds since epoch)
using Timestamp = std::chrono::nanoseconds;

/// Order structure
struct Order {
    OrderId order_id{0};
    std::string symbol;
    Side side{Side::Buy};
    OrderType type{OrderType::Limit};
    Price price{0};
    Quantity quantity{0};
    Quantity filled_quantity{0};
    OrderStatus status{OrderStatus::New};
    Timestamp timestamp{0};
    std::string client_id;

    /// Check if order is active (can be matched)
    [[nodiscard]] bool is_active() const noexcept {
        return status == OrderStatus::New || status == OrderStatus::PartiallyFilled;
    }

    /// Get remaining quantity
    [[nodiscard]] Quantity remaining_quantity() const noexcept {
        return quantity - filled_quantity;
    }

    /// Check if order is completely filled
    [[nodiscard]] bool is_filled() const noexcept {
        return filled_quantity >= quantity;
    }
};

/// Level in the order book (price level)
struct Level {
    Price price{0};
    Quantity total_quantity{0};
    std::vector<std::shared_ptr<Order>> orders;

    /// Add order to level
    void add_order(std::shared_ptr<Order> order) {
        orders.push_back(std::move(order));
        total_quantity += orders.back()->remaining_quantity();
    }

    /// Remove order from level
    bool remove_order(OrderId order_id) noexcept {
        auto it = std::find_if(orders.begin(), orders.end(),
            [order_id](const auto& order) { return order->order_id == order_id; });
        
        if (it != orders.end()) {
            total_quantity -= (*it)->remaining_quantity();
            orders.erase(it);
            return true;
        }
        return false;
    }

    /// Check if level is empty
    [[nodiscard]] bool empty() const noexcept {
        return orders.empty();
    }
};

/// Book side (bid or ask)
class BookSide {
private:
    bool is_buy_side_;
    robin_hood::unordered_map<Price, std::unique_ptr<Level>> price_levels_;
    std::vector<Price> sorted_prices_;
    bool prices_dirty_{true};

    /// Sort prices based on side (descending for buy, ascending for sell)
    void sort_prices() const {
        if (!prices_dirty_) return;
        
        sorted_prices_.clear();
        sorted_prices_.reserve(price_levels_.size());
        
        for (const auto& [price, level] : price_levels_) {
            if (!level->empty()) {
                sorted_prices_.push_back(price);
            }
        }
        
        if (is_buy_side_) {
            // Buy side: highest price first
            std::sort(sorted_prices_.begin(), sorted_prices_.end(), std::greater<Price>());
        } else {
            // Sell side: lowest price first  
            std::sort(sorted_prices_.begin(), sorted_prices_.end());
        }
        
        prices_dirty_ = false;
    }

public:
    explicit BookSide(bool is_buy_side) : is_buy_side_(is_buy_side) {}

    /// Add order to book side
    void add_order(std::shared_ptr<Order> order) {
        Price price = order->price;
        
        auto it = price_levels_.find(price);
        if (it == price_levels_.end()) {
            auto level = std::make_unique<Level>();
            level->price = price;
            level->add_order(order);
            price_levels_[price] = std::move(level);
            prices_dirty_ = true;
        } else {
            it->second->add_order(order);
        }
    }

    /// Remove order from book side
    bool remove_order(OrderId order_id, Price price) {
        auto it = price_levels_.find(price);
        if (it != price_levels_.end()) {
            bool removed = it->second->remove_order(order_id);
            if (removed && it->second->empty()) {
                price_levels_.erase(it);
                prices_dirty_ = true;
            }
            return removed;
        }
        return false;
    }

    /// Get best price (highest bid or lowest ask)
    [[nodiscard]] Price best_price() const {
        sort_prices();
        return sorted_prices_.empty() ? 0 : sorted_prices_[0];
    }

    /// Get level at specific price
    [[nodiscard]] const Level* get_level(Price price) const {
        auto it = price_levels_.find(price);
        return it != price_levels_.end() ? it->second.get() : nullptr;
    }

    /// Get all price levels (sorted)
    [[nodiscard]] std::vector<const Level*> get_levels(size_t max_levels = 10) const {
        sort_prices();
        std::vector<const Level*> levels;
        levels.reserve(std::min(max_levels, sorted_prices_.size()));
        
        for (size_t i = 0; i < std::min(max_levels, sorted_prices_.size()); ++i) {
            auto it = price_levels_.find(sorted_prices_[i]);
            if (it != price_levels_.end()) {
                levels.push_back(it->second.get());
            }
        }
        
        return levels;
    }

    /// Check if book side is empty
    [[nodiscard]] bool empty() const {
        return price_levels_.empty();
    }

    /// Get total quantity at best price
    [[nodiscard]] Quantity best_quantity() const {
        const auto* level = get_level(best_price());
        return level ? level->total_quantity : 0;
    }
};

/// Trade execution result
struct Trade {
    OrderId aggressor_order_id;
    OrderId passive_order_id;
    std::string symbol;
    Side aggressor_side;
    Price price;
    Quantity quantity;
    Timestamp timestamp;
};

/// Limit Order Book implementation
class OrderBook {
private:
    std::string symbol_;
    BookSide bid_side_{true};   // Buy side
    BookSide ask_side_{false};  // Sell side
    robin_hood::unordered_map<OrderId, std::shared_ptr<Order>> order_map_;
    std::vector<Trade> trades_;
    OrderId next_order_id_{1};
    Price tick_size_{1};  // Minimum price increment

    /// Generate unique order ID
    [[nodiscard]] OrderId generate_order_id() noexcept {
        return next_order_id_++;
    }

    /// Get current timestamp
    [[nodiscard]] static Timestamp current_timestamp() noexcept {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch());
    }

    /// Match orders and generate trades
    std::vector<Trade> match_order(std::shared_ptr<Order> order);

public:
    /// Constructor
    explicit OrderBook(std::string symbol, Price tick_size = 1) 
        : symbol_(std::move(symbol)), tick_size_(tick_size) {}

    /// Add new order to the book
    [[nodiscard]] OrderId add_order(Side side, Price price, Quantity quantity, 
                                   OrderType type = OrderType::Limit, 
                                   const std::string& client_id = "");

    /// Cancel existing order
    bool cancel_order(OrderId order_id) noexcept;

    /// Modify existing order
    bool modify_order(OrderId order_id, Price new_price, Quantity new_quantity);

    /// Get order by ID
    [[nodiscard]] std::shared_ptr<Order> get_order(OrderId order_id) const;

    /// Get best bid price
    [[nodiscard]] Price best_bid() const noexcept {
        return bid_side_.best_price();
    }

    /// Get best ask price  
    [[nodiscard]] Price best_ask() const noexcept {
        return ask_side_.best_price();
    }

    /// Get mid price
    [[nodiscard]] Price mid_price() const noexcept {
        Price bid = best_bid();
        Price ask = best_ask();
        return (bid > 0 && ask > 0) ? (bid + ask) / 2 : 0;
    }

    /// Get spread
    [[nodiscard]] Price spread() const noexcept {
        Price bid = best_bid();
        Price ask = best_ask();
        return (bid > 0 && ask > 0) ? ask - bid : 0;
    }

    /// Get best bid quantity
    [[nodiscard]] Quantity best_bid_quantity() const noexcept {
        return bid_side_.best_quantity();
    }

    /// Get best ask quantity
    [[nodiscard]] Quantity best_ask_quantity() const noexcept {
        return ask_side_.best_quantity();
    }

    /// Get order book levels (L2 data)
    [[nodiscard]] std::vector<const Level*> get_bid_levels(size_t max_levels = 10) const {
        return bid_side_.get_levels(max_levels);
    }

    [[nodiscard]] std::vector<const Level*> get_ask_levels(size_t max_levels = 10) const {
        return ask_side_.get_levels(max_levels);
    }

    /// Get all trades
    [[nodiscard]] const std::vector<Trade>& get_trades() const noexcept {
        return trades_;
    }

    /// Clear all trades (for memory management)
    void clear_trades() noexcept {
        trades_.clear();
    }

    /// Get symbol
    [[nodiscard]] const std::string& symbol() const noexcept {
        return symbol_;
    }

    /// Get tick size
    [[nodiscard]] Price tick_size() const noexcept {
        return tick_size_;
    }

    /// Print book state (for debugging)
    void print_book(std::ostream& os, size_t max_levels = 5) const;

    /// Check if book is crossed (bid >= ask)
    [[nodiscard]] bool is_crossed() const noexcept {
        return best_bid() >= best_ask() && best_bid() > 0 && best_ask() > 0;
    }

    /// Get total number of active orders
    [[nodiscard]] size_t order_count() const noexcept {
        return order_map_.size();
    }

    /// Get book statistics
    struct BookStats {
        size_t total_orders{0};
        size_t bid_levels{0};
        size_t ask_levels{0};
        Quantity total_bid_quantity{0};
        Quantity total_ask_quantity{0};
        size_t total_trades{0};
    };

    [[nodiscard]] BookStats get_stats() const;
};

} // namespace lob