#include "lob/order_book.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace lob {

// PriceLevel implementation
void PriceLevel::addOrder(Order* order) noexcept {
    order->level = this;
    order->next = nullptr;
    order->prev = tail_;
    
    if (tail_) {
        tail_->next = order;
    } else {
        head_ = order;
    }
    tail_ = order;
    
    total_quantity += order->remaining_quantity;
    ++order_count;
}

void PriceLevel::removeOrder(Order* order) noexcept {
    if (order->prev) {
        order->prev->next = order->next;
    } else {
        head_ = order->next;
    }
    
    if (order->next) {
        order->next->prev = order->prev;
    } else {
        tail_ = order->prev;
    }
    
    total_quantity -= order->remaining_quantity;
    --order_count;
    
    order->next = nullptr;
    order->prev = nullptr;
    order->level = nullptr;
}

void PriceLevel::modifyOrder(Order* order, Quantity new_qty) noexcept {
    auto qty_diff = static_cast<int32_t>(new_qty) - 
                    static_cast<int32_t>(order->remaining_quantity);
    total_quantity += qty_diff;
    order->remaining_quantity = new_qty;
    order->quantity = std::max(order->quantity, new_qty);
}

// OrderBook implementation
OrderBook::OrderBook(const std::string& symbol) 
    : symbol_(symbol) {
    // Reserve space for expected number of price levels
    bid_levels_.clear();
    ask_levels_.clear();
    orders_.reserve(10000);  // Pre‑allocate for typical book size
}

bool OrderBook::addOrder(Order order) noexcept {
    auto start = std::chrono::steady_clock::now();
    
    // Check for duplicate order ID
    if (orders_.find(order.id) != orders_.end()) {
        return false;
    }
    
    // Create order object
    auto order_ptr = std::make_unique<Order>(std::move(order));
    Order* raw_ptr = order_ptr.get();
    
    // Get or create price level
    PriceLevel* level = getOrCreateLevel(raw_ptr->price, raw_ptr->side);
    level->addOrder(raw_ptr);
    
    // Store order
    orders_[raw_ptr->id] = std::move(order_ptr);
    
    // Update metrics
    ++metrics_.orders_added;
    invalidateCache();
    
    auto end = std::chrono::steady_clock::now();
    metrics_.total_latency += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    
    return true;
}

bool OrderBook::modifyOrder(OrderId id, Quantity new_quantity) noexcept {
    auto start = std::chrono::steady_clock::now();
    
    auto it = orders_.find(id);
    if (it == orders_.end()) {
        return false;
    }
    
    Order* order = it->second.get();
    
    // If increasing quantity, move to back of queue (price‑time priority)
    if (new_quantity > order->remaining_quantity) {
        PriceLevel* level = order->level;
        level->removeOrder(order);
        order->remaining_quantity = new_quantity;
        order->quantity = new_quantity;
        level->addOrder(order);
    } else {
        // Decreasing quantity maintains queue position
        order->level->modifyOrder(order, new_quantity);
    }
    
    ++metrics_.orders_modified;
    invalidateCache();
    
    auto end = std::chrono::steady_clock::now();
    metrics_.total_latency += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    
    return true;
}

bool OrderBook::cancelOrder(OrderId id) noexcept {
    auto start = std::chrono::steady_clock::now();
    
    auto it = orders_.find(id);
    if (it == orders_.end()) {
        return false;
    }
    
    Order* order = it->second.get();
    PriceLevel* level = order->level;
    
    // Remove from level
    level->removeOrder(order);
    
    // Remove empty level
    if (level->empty()) {
        removeEmptyLevel(level->price, level->side);
    }
    
    // Remove order
    orders_.erase(it);
    
    ++metrics_.orders_canceled;
    invalidateCache();
    
    auto end = std::chrono::steady_clock::now();
    metrics_.total_latency += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    
    return true;
}

std::vector<Execution> OrderBook::processMarketOrder(
    Side side, Quantity quantity, Timestamp timestamp) noexcept {
    
    std::vector<Execution> executions;
    executions.reserve(10);  // Pre‑allocate for typical fills
    
    auto& opposite_levels = (side == Side::BID) ? ask_levels_ : bid_levels_;
    Quantity remaining = quantity;
    
    while (remaining > 0 && !opposite_levels.empty()) {
        auto& [price, level] = *opposite_levels.begin();
        
        while (remaining > 0 && !level->empty()) {
            Order* order = level->front();
            Quantity fill_qty = std::min(remaining, order->remaining_quantity);
            
            // Create execution
            if (side == Side::BID) {
                executions.emplace_back(0, order->id, price, fill_qty, timestamp);
            } else {
                executions.emplace_back(order->id, 0, price, fill_qty, timestamp);
            }
            
            // Update quantities
            remaining -= fill_qty;
            order->remaining_quantity -= fill_qty;
            level->total_quantity -= fill_qty;
            
            metrics_.total_volume += fill_qty;
            ++metrics_.orders_matched;
            
            // Remove filled order
            if (order->isFilled()) {
                OrderId filled_id = order->id;
                level->removeOrder(order);
                orders_.erase(filled_id);
            }
        }
        
        // Remove empty level
        if (level->empty()) {
            opposite_levels.erase(opposite_levels.begin());
        }
    }
    
    invalidateCache();
    return executions;
}

std::vector<Execution> OrderBook::matchOrders() noexcept {
    std::vector<Execution> executions;
    
    while (!bid_levels_.empty() && !ask_levels_.empty()) {
        auto& bid_level = bid_levels_.begin()->second;
        auto& ask_level = ask_levels_.begin()->second;
        
        // Check if orders can match
        if (bid_level->price < ask_level->price) {
            break;  // No crossing orders
        }
        
        // Match orders at crossing prices
        while (!bid_level->empty() && !ask_level->empty()) {
            Order* bid = bid_level->front();
            Order* ask = ask_level->front();
            
            Price match_price = (bid->timestamp < ask->timestamp) ? 
                               bid->price : ask->price;
            Quantity match_qty = std::min(bid->remaining_quantity, 
                                         ask->remaining_quantity);
            
            // Create execution
            executions.emplace_back(bid->id, ask->id, match_price, 
                                   match_qty, std::max(bid->timestamp, ask->timestamp));
            
            // Update orders
            bid->remaining_quantity -= match_qty;
            ask->remaining_quantity -= match_qty;
            bid_level->total_quantity -= match_qty;
            ask_level->total_quantity -= match_qty;
            
            metrics_.total_volume += match_qty;
            ++metrics_.orders_matched;
            
            // Remove filled orders
            if (bid->isFilled()) {
                OrderId bid_id = bid->id;
                bid_level->removeOrder(bid);
                orders_.erase(bid_id);
            }
            if (ask->isFilled()) {
                OrderId ask_id = ask->id;
                ask_level->removeOrder(ask);
                orders_.erase(ask_id);
            }
        }
        
        // Remove empty levels
        if (bid_level->empty()) {
            bid_levels_.erase(bid_levels_.begin());
        }
        if (ask_level->empty()) {
            ask_levels_.erase(ask_levels_.begin());
        }
    }
    
    if (!executions.empty()) {
        invalidateCache();
    }
    
    return executions;
}

const Order* OrderBook::getOrder(OrderId id) const noexcept {
    auto it = orders_.find(id);
    return (it != orders_.end()) ? it->second.get() : nullptr;
}

double OrderBook::getMicroPrice(int levels) const noexcept {
    if (bid_levels_.empty() || ask_levels_.empty()) {
        return 0.0;
    }
    
    Quantity bid_qty = 0, ask_qty = 0;
    double weighted_bid = 0.0, weighted_ask = 0.0;
    
    // Calculate weighted bid
    int count = 0;
    for (const auto& [price, level] : bid_levels_) {
        if (++count > levels) break;
        bid_qty += level->total_quantity;
        weighted_bid += priceToDouble(price) * level->total_quantity;
    }
    
    // Calculate weighted ask
    count = 0;
    for (const auto& [price, level] : ask_levels_) {
        if (++count > levels) break;
        ask_qty += level->total_quantity;
        weighted_ask += priceToDouble(price) * level->total_quantity;
    }
    
    if (bid_qty + ask_qty == 0) {
        return getMidPrice();
    }
    
    // Microprice formula: weighted average by inverse queue size
    double bid_weight = static_cast<double>(ask_qty) / (bid_qty + ask_qty);
    double ask_weight = static_cast<double>(bid_qty) / (bid_qty + ask_qty);
    
    return bid_weight * (weighted_bid / bid_qty) + 
           ask_weight * (weighted_ask / ask_qty);
}

double OrderBook::getOrderImbalance(int levels) const noexcept {
    Quantity bid_volume = 0, ask_volume = 0;
    
    // Sum bid volume
    int count = 0;
    for (const auto& [price, level] : bid_levels_) {
        if (++count > levels) break;
        bid_volume += level->total_quantity;
    }
    
    // Sum ask volume
    count = 0;
    for (const auto& [price, level] : ask_levels_) {
        if (++count > levels) break;
        ask_volume += level->total_quantity;
    }
    
    if (bid_volume + ask_volume == 0) {
        return 0.0;
    }
    
    // Imbalance: (bid - ask) / (bid + ask)
    return static_cast<double>(bid_volume - ask_volume) / 
           static_cast<double>(bid_volume + ask_volume);
}

Quantity OrderBook::getQueuePosition(OrderId id) const noexcept {
    auto it = orders_.find(id);
    if (it == orders_.end()) {
        return 0;
    }
    
    const Order* order = it->second.get();
    const PriceLevel* level = order->level;
    if (!level) {
        return 0;
    }
    
    Quantity position = 0;
    const Order* current = level->front();
    
    while (current && current != order) {
        position += current->remaining_quantity;
        current = current->next;
    }
    
    return position;
}

BookStats OrderBook::getStats() const noexcept {
    BookStats stats;
    
    if (!bid_levels_.empty()) {
        stats.best_bid = bid_levels_.begin()->first;
        stats.bid_levels = bid_levels_.size();
        for (const auto& [price, level] : bid_levels_) {
            stats.bid_volume += level->total_quantity;
        }
    }
    
    if (!ask_levels_.empty()) {
        stats.best_ask = ask_levels_.begin()->first;
        stats.ask_levels = ask_levels_.size();
        for (const auto& [price, level] : ask_levels_) {
            stats.ask_volume += level->total_quantity;
        }
    }
    
    stats.spread = getSpread();
    stats.mid_price = getMidPrice();
    stats.microprice = getMicroPrice();
    stats.imbalance = getOrderImbalance();
    stats.total_orders = orders_.size();
    
    return stats;
}

std::vector<std::pair<Price, Quantity>> 
OrderBook::getAggregatedBook(Side side, int levels) const noexcept {
    std::vector<std::pair<Price, Quantity>> result;
    result.reserve(levels);
    
    const auto& level_map = (side == Side::BID) ? bid_levels_ : ask_levels_;
    
    int count = 0;
    for (const auto& [price, level] : level_map) {
        if (++count > levels) break;
        result.emplace_back(price, level->total_quantity);
    }
    
    return result;
}

std::vector<Order> OrderBook::getOrdersAtLevel(Price price, Side side) const noexcept {
    std::vector<Order> result;
    
    const auto& level_map = (side == Side::BID) ? bid_levels_ : ask_levels_;
    auto it = level_map.find(price);
    
    if (it != level_map.end()) {
        const Order* current = it->second->front();
        while (current) {
            result.push_back(*current);
            current = current->next;
        }
    }
    
    return result;
}

void OrderBook::clear() noexcept {
    orders_.clear();
    bid_levels_.clear();
    ask_levels_.clear();
    invalidateCache();
}

void OrderBook::updateCache() const noexcept {
    cached_best_bid_ = bid_levels_.empty() ? 0 : bid_levels_.begin()->first;
    cached_best_ask_ = ask_levels_.empty() ? 0 : ask_levels_.begin()->first;
    cache_valid_ = true;
}

PriceLevel* OrderBook::getOrCreateLevel(Price price, Side side) noexcept {
    auto& levels = (side == Side::BID) ? bid_levels_ : ask_levels_;
    
    auto it = levels.find(price);
    if (it != levels.end()) {
        return it->second.get();
    }
    
    auto level = std::make_unique<PriceLevel>(price, side);
    PriceLevel* ptr = level.get();
    levels[price] = std::move(level);
    
    return ptr;
}

void OrderBook::removeEmptyLevel(Price price, Side side) noexcept {
    auto& levels = (side == Side::BID) ? bid_levels_ : ask_levels_;
    levels.erase(price);
}

} // namespace lob