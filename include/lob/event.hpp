#pragma once

#include "lob/order_book.hpp"
#include "lob/signals.hpp"
#include <optional>

namespace lob {

// A generic event type used by the backtester.  It can represent market
// data updates, generated signals, orders and fills.  Events are ordered
// in a priority queue by timestamp (earliest first).
struct Event {
    enum Type : uint8_t { MARKET_DATA, SIGNAL, ORDER, FILL, END_OF_DAY };
    Type type{};
    Timestamp timestamp{};
    std::string symbol;
    
    // Optional payloads for each event type.  Only one of these will
    // typically be engaged depending on the event type.  std::optional
    // avoids undefined behaviour from uninitialised values.
    std::optional<MarketDataUpdate> market_update;
    std::optional<Signal>          signal;
    std::optional<Order>           order;
    std::optional<Execution>       execution;
    
    // Comparison operator to turn the STL priority_queue into a min‑heap
    bool operator<(const Event& other) const { return timestamp > other.timestamp; }
};

} // namespace lob