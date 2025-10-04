#include <lob/backtester.hpp>
#include <lob/order_book.hpp>
#include <lob/signals.hpp>
#include <iostream>
#include <memory>

namespace lob {

/// Simple market making strategy
class SimpleMarketMaker : public Strategy {
private:
    double spread_target_{0.02};  // Target spread as fraction of mid price
    lob::Quantity order_size_{100};
    lob::Price tick_size_{1};
    
    // Order management
    std::unordered_map<std::string, OrderId> active_bids_;
    std::unordered_map<std::string, OrderId> active_asks_;
    
    // Risk management
    double max_position_{1000};
    double current_position_{0};
    
    // Performance tracking
    size_t orders_placed_{0};
    size_t orders_filled_{0};
    double total_pnl_{0};
    
public:
    explicit SimpleMarketMaker(std::string id = "simple_mm") : Strategy(std::move(id)) {}
    
    void initialize() override {
        std::cout << "Initializing Simple Market Maker strategy..." << std::endl;
    }
    
    void on_market_data(const MarketEvent& event) override {
        // Only process events for our symbol of interest
        if (event.symbol != "AAPL") return;
        
        // Get the order book for this symbol (this would be provided by the backtester)
        // For this example, we'll assume we can access it somehow
        
        // Calculate our desired bid and ask prices
        update_quotes(event.symbol);
    }
    
    void on_fill(const FillEvent& event) override {
        orders_filled_++;
        
        // Update position
        if (event.side == Side::Buy) {
            current_position_ += static_cast<double>(event.fill_quantity);
        } else {
            current_position_ -= static_cast<double>(event.fill_quantity);
        }
        
        // Update P&L
        total_pnl_ += calculate_fill_pnl(event);
        
        std::cout << "Fill: " << event.symbol << " " << 
                     (event.side == Side::Buy ? "BUY" : "SELL") << " " <<
                     event.fill_quantity << "@" << event.fill_price << 
                     " Position: " << current_position_ << 
                     " PnL: " << total_pnl_ << std::endl;
                     
        // Update quotes after fill
        update_quotes(event.symbol);
    }
    
private:
    void update_quotes(const std::string& symbol) {
        // Cancel existing orders first
        cancel_all_orders(symbol);
        
        // Check position limits
        if (std::abs(current_position_) >= max_position_) {
            std::cout << "Position limit reached, not placing new orders" << std::endl;
            return;
        }
        
        // Calculate theoretical mid price and spread
        auto theo_prices = calculate_theoretical_prices(symbol);
        if (!theo_prices) return;
        
        auto [theo_mid, bid_price, ask_price] = *theo_prices;
        
        // Adjust for inventory (position skewing)
        double inventory_skew = calculate_inventory_skew();
        bid_price = static_cast<lob::Price>(bid_price + inventory_skew);
        ask_price = static_cast<lob::Price>(ask_price + inventory_skew);
        
        // Place new orders if we're not at position limits
        if (current_position_ > -max_position_ * 0.8) {  // Can still buy
            place_order(symbol, Side::Buy, bid_price, order_size_, 
                       OrderType::Limit, std::chrono::nanoseconds{0});
        }
        
        if (current_position_ < max_position_ * 0.8) {  // Can still sell
            place_order(symbol, Side::Sell, ask_price, order_size_, 
                       OrderType::Limit, std::chrono::nanoseconds{0});
        }
        
        orders_placed_ += 2;
    }
    
    std::optional<std::tuple<double, lob::Price, lob::Price>> 
    calculate_theoretical_prices(const std::string& symbol) {
        // In a real implementation, we'd get this from the backtester
        // For now, we'll use dummy values
        
        // Simulate getting market data
        lob::Price best_bid = 10000;  // $100.00 in fixed point
        lob::Price best_ask = 10005;  // $100.05 in fixed point
        
        if (best_bid == 0 || best_ask == 0) {
            return std::nullopt;  // No quotes available
        }
        
        double mid_price = (best_bid + best_ask) / 2.0;
        double current_spread = best_ask - best_bid;
        
        // Calculate our desired spread
        double desired_spread = std::max(
            mid_price * spread_target_,  // Percentage-based spread
            static_cast<double>(tick_size_ * 2)  // Minimum 2 tick spread
        );
        
        // Calculate our bid and ask prices
        lob::Price our_bid = static_cast<lob::Price>(mid_price - desired_spread / 2.0);
        lob::Price our_ask = static_cast<lob::Price>(mid_price + desired_spread / 2.0);
        
        // Round to tick size
        our_bid = (our_bid / tick_size_) * tick_size_;
        our_ask = (our_ask / tick_size_) * tick_size_;
        
        return std::make_tuple(mid_price, our_bid, our_ask);
    }
    
    double calculate_inventory_skew() const {
        // Skew quotes based on current position
        // Positive position -> skew quotes lower (encourage selling)
        // Negative position -> skew quotes higher (encourage buying)
        
        double position_ratio = current_position_ / max_position_;
        double skew_factor = -position_ratio * 0.5 * tick_size_;  // Max 0.5 tick skew
        
        return skew_factor;
    }
    
    void cancel_all_orders(const std::string& symbol) {
        // Cancel existing bid orders
        for (const auto& [sym, order_id] : active_bids_) {
            if (sym == symbol) {
                cancel_order(order_id);
            }
        }
        active_bids_.erase(symbol);
        
        // Cancel existing ask orders
        for (const auto& [sym, order_id] : active_asks_) {
            if (sym == symbol) {
                cancel_order(order_id);
            }
        }
        active_asks_.erase(symbol);
    }
    
    double calculate_fill_pnl(const FillEvent& event) {
        // Simplified P&L calculation
        // In reality, this would be more complex with proper position tracking
        
        double mid_price = 10002.5;  // Dummy mid price
        double fill_price = static_cast<double>(event.fill_price);
        
        if (event.side == Side::Buy) {
            return mid_price - fill_price;  // Profit if bought below mid
        } else {
            return fill_price - mid_price;  // Profit if sold above mid
        }
    }
    
public:
    // Performance reporting
    void print_performance() const {
        std::cout << "\n=== Market Maker Performance ===" << std::endl;
        std::cout << "Orders placed: " << orders_placed_ << std::endl;
        std::cout << "Orders filled: " << orders_filled_ << std::endl;
        std::cout << "Fill rate: " << (orders_placed_ > 0 ? 
                      static_cast<double>(orders_filled_) / orders_placed_ * 100.0 : 0.0) 
                  << "%" << std::endl;
        std::cout << "Final position: " << current_position_ << std::endl;
        std::cout << "Total P&L: $" << total_pnl_ / 100.0 << std::endl;
        std::cout << "================================\n" << std::endl;
    }
    
    // Getters for analysis
    double get_total_pnl() const { return total_pnl_; }
    double get_position() const { return current_position_; }
    size_t get_orders_placed() const { return orders_placed_; }
    size_t get_orders_filled() const { return orders_filled_; }
};

/// Momentum/trend following strategy
class MomentumStrategy : public Strategy {
private:
    size_t lookback_periods_{20};
    double momentum_threshold_{0.02};  // 2% threshold
    
    std::vector<double> price_history_;
    double current_position_{0};
    double max_position_{500};
    
public:
    explicit MomentumStrategy(std::string id = "momentum") : Strategy(std::move(id)) {}
    
    void initialize() override {
        std::cout << "Initializing Momentum Strategy..." << std::endl;
        price_history_.reserve(lookback_periods_ * 2);
    }
    
    void on_market_data(const MarketEvent& event) override {
        if (event.symbol != "AAPL") return;
        
        // Update price history
        double mid_price = get_mid_price(event.symbol);  // Dummy implementation
        price_history_.push_back(mid_price);
        
        // Keep only recent history
        if (price_history_.size() > lookback_periods_) {
            price_history_.erase(price_history_.begin());
        }
        
        // Calculate momentum signal
        if (price_history_.size() >= lookback_periods_) {
            double momentum = calculate_momentum();
            execute_momentum_strategy(event.symbol, momentum);
        }
    }
    
    void on_fill(const FillEvent& event) override {
        if (event.side == Side::Buy) {
            current_position_ += static_cast<double>(event.fill_quantity);
        } else {
            current_position_ -= static_cast<double>(event.fill_quantity);
        }
        
        std::cout << "Momentum fill: " << event.symbol << " " << 
                     (event.side == Side::Buy ? "BUY" : "SELL") << " " <<
                     event.fill_quantity << "@" << event.fill_price << 
                     " Position: " << current_position_ << std::endl;
    }
    
private:
    double get_mid_price(const std::string& symbol) {
        // Dummy implementation - would get from order book
        return 100.0 + (std::rand() % 1000) / 10000.0;  // Random walk around $100
    }
    
    double calculate_momentum() {
        if (price_history_.size() < lookback_periods_) return 0.0;
        
        double start_price = price_history_[0];
        double end_price = price_history_.back();
        
        return (end_price - start_price) / start_price;
    }
    
    void execute_momentum_strategy(const std::string& symbol, double momentum) {
        if (std::abs(momentum) < momentum_threshold_) {
            return;  // No strong trend
        }
        
        if (momentum > momentum_threshold_ && current_position_ < max_position_) {
            // Strong upward momentum - buy
            lob::Quantity order_size = std::min(100.0, max_position_ - current_position_);
            place_order(symbol, Side::Buy, 0, static_cast<lob::Quantity>(order_size), 
                       OrderType::Market);
                       
        } else if (momentum < -momentum_threshold_ && current_position_ > -max_position_) {
            // Strong downward momentum - sell
            lob::Quantity order_size = std::min(100.0, max_position_ + current_position_);
            place_order(symbol, Side::Sell, 0, static_cast<lob::Quantity>(order_size), 
                       OrderType::Market);
        }
    }
};

/// Order imbalance signal-based strategy
class OrderImbalanceStrategy : public Strategy {
private:
    double imbalance_threshold_{0.7};  // 70% imbalance threshold
    lob::Quantity base_order_size_{50};
    
    double current_position_{0};
    double max_position_{1000};
    
public:
    explicit OrderImbalanceStrategy(std::string id = "order_imbalance") 
        : Strategy(std::move(id)) {}
    
    void initialize() override {
        std::cout << "Initializing Order Imbalance Strategy..." << std::endl;
    }
    
    void on_market_data(const MarketEvent& event) override {
        if (event.symbol != "AAPL") return;
        
        // Calculate order imbalance signal
        auto signals = get_market_signals(event.symbol);
        if (!signals) return;
        
        double imbalance = signals->order_imbalance();
        double microprice = signals->microprice();
        double market_quality = signals->effective_spread();
        
        // Execute strategy based on signals
        execute_imbalance_strategy(event.symbol, imbalance, microprice, market_quality);
    }
    
    void on_fill(const FillEvent& event) override {
        if (event.side == Side::Buy) {
            current_position_ += static_cast<double>(event.fill_quantity);
        } else {
            current_position_ -= static_cast<double>(event.fill_quantity);
        }
        
        std::cout << "Imbalance fill: " << event.symbol << " " << 
                     (event.side == Side::Buy ? "BUY" : "SELL") << " " <<
                     event.fill_quantity << "@" << event.fill_price << 
                     " Position: " << current_position_ << std::endl;
    }
    
private:
    std::optional<lob::Signals::MarketQualityMetrics> 
    get_market_signals(const std::string& symbol) {
        // In real implementation, this would get the order book from backtester
        // and calculate signals. For demo, we'll return dummy values.
        
        lob::Signals::MarketQualityMetrics metrics;
        metrics.imbalance = 0.6 + (std::rand() % 400) / 1000.0 - 0.2;  // 0.4 to 0.8
        metrics.microprice = 100.0 + (std::rand() % 100) / 1000.0;
        metrics.effective_spread = 0.05 + (std::rand() % 50) / 10000.0;  // 0.05% to 0.1%
        
        return metrics;
    }
    
    void execute_imbalance_strategy(const std::string& symbol, 
                                   double imbalance, 
                                   double microprice,
                                   double market_quality) {
        // Only trade when market quality is good (tight spread)
        if (market_quality > 0.08) {  // 8 bps spread threshold
            return;
        }
        
        // Strong buy imbalance - expect price to move up
        if (imbalance > imbalance_threshold_ && current_position_ < max_position_) {
            lob::Quantity order_size = std::min(
                static_cast<double>(base_order_size_),
                max_position_ - current_position_
            );
            
            // Use microprice for better execution
            lob::Price limit_price = static_cast<lob::Price>(microprice + 0.5);  // Slightly aggressive
            
            place_order(symbol, Side::Buy, limit_price, 
                       static_cast<lob::Quantity>(order_size), OrderType::Limit);
                       
        }
        // Strong sell imbalance - expect price to move down  
        else if (imbalance < (1.0 - imbalance_threshold_) && current_position_ > -max_position_) {
            lob::Quantity order_size = std::min(
                static_cast<double>(base_order_size_),
                max_position_ + current_position_
            );
            
            lob::Price limit_price = static_cast<lob::Price>(microprice - 0.5);  // Slightly aggressive
            
            place_order(symbol, Side::Sell, limit_price, 
                       static_cast<lob::Quantity>(order_size), OrderType::Limit);
        }
    }
    
public:
    double get_position() const { return current_position_; }
};

} // namespace lob

// Example usage
int main() {
    std::cout << "LOB Strategy Examples" << std::endl;
    
    // Create backtester
    lob::Backtester backtester(1000000.0);  // $1M initial capital
    
    // Add strategies
    backtester.add_strategy(std::make_unique<lob::SimpleMarketMaker>());
    backtester.add_strategy(std::make_unique<lob::MomentumStrategy>());
    backtester.add_strategy(std::make_unique<lob::OrderImbalanceStrategy>());
    
    // Load market data (dummy for this example)
    // backtester.load_market_data("data/AAPL_L3_20231201.csv");
    
    std::cout << "Strategies loaded successfully!" << std::endl;
    std::cout << "In a real implementation, call backtester.run() to start backtesting." << std::endl;
    
    return 0;
}