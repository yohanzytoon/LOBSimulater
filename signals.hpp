#pragma once

#include "order_book.hpp"
#include <cmath>
#include <vector>

namespace lob {

/// Microstructure signals and analytics
class Signals {
private:
    const OrderBook* book_;
    
public:
    explicit Signals(const OrderBook* book) : book_(book) {}

    /// Calculate order imbalance: I = Q_bid / (Q_bid + Q_ask)
    /// Returns value between 0 and 1, where:
    /// - 0.5 = balanced book
    /// - > 0.5 = more buying pressure  
    /// - < 0.5 = more selling pressure
    [[nodiscard]] double order_imbalance() const noexcept {
        Quantity bid_qty = book_->best_bid_quantity();
        Quantity ask_qty = book_->best_ask_quantity();
        
        if (bid_qty == 0 && ask_qty == 0) return 0.5;
        
        return static_cast<double>(bid_qty) / (bid_qty + ask_qty);
    }

    /// Calculate microprice using Stoikov's formulation
    /// Microprice = Mid + (1/pi) * arctan(imbalance) * spread/2
    /// This is a better predictor of short-term price moves than mid-price
    [[nodiscard]] double microprice() const noexcept {
        Price mid = book_->mid_price();
        if (mid == 0) return 0.0;
        
        double imbalance = order_imbalance();
        Price spread = book_->spread();
        
        // Convert imbalance to symmetric range [-1, 1]
        double symmetric_imbalance = 2.0 * imbalance - 1.0;
        
        // Microprice adjustment using arctangent
        double adjustment = (2.0 / M_PI) * std::atan(symmetric_imbalance) * spread / 2.0;
        
        return static_cast<double>(mid) + adjustment;
    }

    /// Calculate weighted mid-price
    /// WMP = I * ask_price + (1-I) * bid_price
    [[nodiscard]] double weighted_mid_price() const noexcept {
        Price bid = book_->best_bid();
        Price ask = book_->best_ask();
        
        if (bid == 0 || ask == 0) return 0.0;
        
        double imbalance = order_imbalance();
        return imbalance * ask + (1.0 - imbalance) * bid;
    }

    /// Calculate order book pressure (depth-weighted imbalance)
    /// Considers multiple levels of the book with exponential decay
    [[nodiscard]] double book_pressure(size_t levels = 5, double decay = 0.5) const noexcept {
        auto bid_levels = book_->get_bid_levels(levels);
        auto ask_levels = book_->get_ask_levels(levels);
        
        double weighted_bid_qty = 0.0;
        double weighted_ask_qty = 0.0;
        double weight = 1.0;
        
        size_t max_levels = std::max(bid_levels.size(), ask_levels.size());
        
        for (size_t i = 0; i < max_levels; ++i) {
            if (i < bid_levels.size()) {
                weighted_bid_qty += bid_levels[i]->total_quantity * weight;
            }
            if (i < ask_levels.size()) {
                weighted_ask_qty += ask_levels[i]->total_quantity * weight;
            }
            weight *= decay;  // Exponential decay for deeper levels
        }
        
        if (weighted_bid_qty == 0.0 && weighted_ask_qty == 0.0) return 0.5;
        
        return weighted_bid_qty / (weighted_bid_qty + weighted_ask_qty);
    }

    /// Calculate price impact estimation
    /// Estimates the price impact of a market order of given size
    [[nodiscard]] double price_impact(Side side, Quantity order_size) const noexcept {
        Price initial_price = (side == Side::Buy) ? book_->best_ask() : book_->best_bid();
        if (initial_price == 0) return 0.0;
        
        auto levels = (side == Side::Buy) ? book_->get_ask_levels(20) : book_->get_bid_levels(20);
        
        Quantity remaining_size = order_size;
        Price last_price = initial_price;
        
        for (const auto* level : levels) {
            if (remaining_size <= level->total_quantity) {
                break;
            }
            remaining_size -= level->total_quantity;
            last_price = level->price;
        }
        
        // Return relative price impact
        return std::abs(static_cast<double>(last_price - initial_price)) / initial_price;
    }

    /// Calculate effective spread
    /// Effective spread considers the cost of a round-trip transaction
    [[nodiscard]] double effective_spread() const noexcept {
        Price bid = book_->best_bid();
        Price ask = book_->best_ask();
        Price mid = book_->mid_price();
        
        if (bid == 0 || ask == 0 || mid == 0) return 0.0;
        
        // Round-trip cost as percentage of mid-price
        return static_cast<double>(ask - bid) / mid;
    }

    /// Calculate order flow toxicity (probability of informed trading)
    /// Uses volume-synchronized probability of informed trading (VPIN)
    [[nodiscard]] double order_flow_toxicity(const std::vector<Trade>& recent_trades, 
                                           size_t lookback_trades = 50) const noexcept {
        if (recent_trades.size() < 2) return 0.0;
        
        // Calculate VPIN over recent trades
        Quantity buy_volume = 0;
        Quantity sell_volume = 0;
        
        size_t start_idx = (recent_trades.size() > lookback_trades) ? 
                          recent_trades.size() - lookback_trades : 0;
        
        for (size_t i = start_idx; i < recent_trades.size(); ++i) {
            if (recent_trades[i].aggressor_side == Side::Buy) {
                buy_volume += recent_trades[i].quantity;
            } else {
                sell_volume += recent_trades[i].quantity;
            }
        }
        
        Quantity total_volume = buy_volume + sell_volume;
        if (total_volume == 0) return 0.0;
        
        // VPIN = |buy_volume - sell_volume| / total_volume
        return static_cast<double>(std::abs(static_cast<int64_t>(buy_volume) - 
                                          static_cast<int64_t>(sell_volume))) / total_volume;
    }

    /// Calculate realized spread
    /// Measures the profitability of providing liquidity
    [[nodiscard]] double realized_spread(Price execution_price, Side execution_side, 
                                       Price future_mid_price) const noexcept {
        if (future_mid_price == 0) return 0.0;
        
        double mid_price_change = static_cast<double>(future_mid_price - book_->mid_price());
        double price_diff = static_cast<double>(execution_price - book_->mid_price());
        
        // For buy orders: positive if price moved up after execution
        // For sell orders: positive if price moved down after execution
        double sign = (execution_side == Side::Buy) ? 1.0 : -1.0;
        
        return sign * (price_diff - mid_price_change);
    }

    /// Calculate order book resilience
    /// Measures how quickly the book recovers after a large trade
    [[nodiscard]] double book_resilience() const noexcept {
        // Simplified resilience measure based on depth near the touch
        auto bid_levels = book_->get_bid_levels(3);
        auto ask_levels = book_->get_ask_levels(3);
        
        if (bid_levels.empty() || ask_levels.empty()) return 0.0;
        
        Quantity near_touch_depth = 0;
        for (const auto* level : bid_levels) {
            near_touch_depth += level->total_quantity;
        }
        for (const auto* level : ask_levels) {
            near_touch_depth += level->total_quantity;
        }
        
        // Normalize by spread (lower spread with higher depth = more resilient)
        Price spread = book_->spread();
        if (spread == 0) return 0.0;
        
        return static_cast<double>(near_touch_depth) / spread;
    }

    /// Calculate market quality metrics
    struct MarketQualityMetrics {
        double spread_bps;           // Spread in basis points
        double depth;               // Total depth at best levels
        double imbalance;           // Order imbalance
        double microprice;          // Microprice estimate
        double effective_spread;    // Effective spread
        double resilience;          // Book resilience
        double pressure;            // Multi-level pressure
        double volatility_proxy;    // High-frequency volatility proxy
    };

    /// Get comprehensive market quality metrics
    [[nodiscard]] MarketQualityMetrics get_market_quality() const noexcept {
        MarketQualityMetrics metrics;
        
        Price mid = book_->mid_price();
        Price spread = book_->spread();
        
        metrics.spread_bps = (mid > 0) ? (static_cast<double>(spread) / mid) * 10000 : 0.0;
        metrics.depth = static_cast<double>(book_->best_bid_quantity() + book_->best_ask_quantity());
        metrics.imbalance = order_imbalance();
        metrics.microprice = microprice();
        metrics.effective_spread = effective_spread();
        metrics.resilience = book_resilience();
        metrics.pressure = book_pressure();
        metrics.volatility_proxy = metrics.spread_bps * (1.0 - std::abs(0.5 - metrics.imbalance));
        
        return metrics;
    }

    /// Calculate queue position for a hypothetical order
    /// Returns position in queue (1-based) for an order at given price/side
    [[nodiscard]] size_t queue_position(Side side, Price price) const noexcept {
        const Level* level = nullptr;
        
        if (side == Side::Buy) {
            if (price > book_->best_bid()) return 1;  // Better than best bid
            level = book_->get_bid_levels(1000).front();  // Get level at price
        } else {
            if (price < book_->best_ask()) return 1;  // Better than best ask
            level = book_->get_ask_levels(1000).front();  // Get level at price
        }
        
        return level ? level->orders.size() + 1 : 1;
    }

    /// Calculate arrival rate of orders at best bid/ask
    /// Requires historical order data (simplified version)
    [[nodiscard]] double order_arrival_rate() const noexcept {
        // This would typically require a time-series of order arrivals
        // For now, return a proxy based on current book depth
        auto bid_levels = book_->get_bid_levels(1);
        auto ask_levels = book_->get_ask_levels(1);
        
        size_t total_orders = 0;
        if (!bid_levels.empty()) total_orders += bid_levels[0]->orders.size();
        if (!ask_levels.empty()) total_orders += ask_levels[0]->orders.size();
        
        return static_cast<double>(total_orders);
    }
};

} // namespace lob