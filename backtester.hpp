#pragma once

#include "order_book.hpp"
#include "signals.hpp"
#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

namespace lob {

/// Forward declarations
class Strategy;
class Portfolio;

/// Event types
enum class EventType : uint8_t {
    Market = 0,     // Market data event (order book update)
    Signal = 1,     // Trading signal generated
    Order = 2,      // Order placement event
    Fill = 3,       // Order fill event
    Timer = 4       // Timer-based event
};

/// Base event class
struct Event {
    EventType type;
    Timestamp timestamp;
    std::string symbol;
    
    explicit Event(EventType event_type, Timestamp ts = Timestamp{0}, std::string sym = "")
        : type(event_type), timestamp(ts), symbol(std::move(sym)) {}
    
    virtual ~Event() = default;
};

/// Market data event (order book update)
struct MarketEvent : public Event {
    enum class UpdateType : uint8_t {
        Add = 0,
        Modify = 1,
        Cancel = 2,
        Trade = 3
    };
    
    UpdateType update_type;
    OrderId order_id{0};
    Side side{Side::Buy};
    Price price{0};
    Quantity quantity{0};
    
    MarketEvent(Timestamp ts, std::string symbol, UpdateType type, 
                OrderId oid = 0, Side s = Side::Buy, Price p = 0, Quantity q = 0)
        : Event(EventType::Market, ts, std::move(symbol))
        , update_type(type), order_id(oid), side(s), price(p), quantity(q) {}
};

/// Signal event (strategy generated)
struct SignalEvent : public Event {
    enum class SignalType : uint8_t {
        Buy = 0,
        Sell = 1,
        Hold = 2
    };
    
    SignalType signal_type;
    double confidence{0.0};  // Signal confidence [0, 1]
    std::string strategy_id;
    std::unordered_map<std::string, double> metadata;  // Additional signal data
    
    SignalEvent(Timestamp ts, std::string symbol, SignalType type, 
                double conf, std::string strat_id)
        : Event(EventType::Signal, ts, std::move(symbol))
        , signal_type(type), confidence(conf), strategy_id(std::move(strat_id)) {}
};

/// Order event (order placement/modification/cancellation)
struct OrderEvent : public Event {
    enum class OrderAction : uint8_t {
        Place = 0,
        Modify = 1,
        Cancel = 2
    };
    
    OrderAction action;
    OrderId order_id{0};  // 0 for new orders
    Side side{Side::Buy};
    OrderType order_type{OrderType::Limit};
    Price price{0};
    Quantity quantity{0};
    std::string client_id;
    std::string strategy_id;
    
    OrderEvent(Timestamp ts, std::string symbol, OrderAction act, Side s, 
               OrderType type, Price p, Quantity q, std::string client = "", 
               std::string strat = "")
        : Event(EventType::Order, ts, std::move(symbol))
        , action(act), side(s), order_type(type), price(p), quantity(q)
        , client_id(std::move(client)), strategy_id(std::move(strat)) {}
};

/// Fill event (order execution)
struct FillEvent : public Event {
    OrderId order_id;
    Side side;
    Price fill_price;
    Quantity fill_quantity;
    std::string strategy_id;
    double commission{0.0};
    
    FillEvent(Timestamp ts, std::string symbol, OrderId oid, Side s, 
              Price price, Quantity qty, std::string strat_id, double comm = 0.0)
        : Event(EventType::Fill, ts, std::move(symbol))
        , order_id(oid), side(s), fill_price(price), fill_quantity(qty)
        , strategy_id(std::move(strat_id)), commission(comm) {}
};

/// Timer event (periodic events)
struct TimerEvent : public Event {
    std::string timer_id;
    std::chrono::duration<double> interval;
    
    TimerEvent(Timestamp ts, std::string id, std::chrono::duration<double> intv)
        : Event(EventType::Timer, ts, ""), timer_id(std::move(id)), interval(intv) {}
};

/// Event comparator for priority queue (earliest timestamp first)
struct EventComparator {
    bool operator()(const std::shared_ptr<Event>& a, const std::shared_ptr<Event>& b) const {
        return a->timestamp > b->timestamp;  // Min-heap
    }
};

/// Portfolio position tracking
struct Position {
    std::string symbol;
    int64_t quantity{0};  // Net position (+ long, - short)
    double avg_price{0.0};
    double realized_pnl{0.0};
    double unrealized_pnl{0.0};
    double total_fees{0.0};
    
    /// Update position with a fill
    void update(Side fill_side, Quantity fill_qty, Price fill_price, double commission = 0.0);
    
    /// Calculate unrealized PnL at given market price
    void update_unrealized_pnl(Price market_price);
    
    /// Get total PnL
    [[nodiscard]] double total_pnl() const noexcept {
        return realized_pnl + unrealized_pnl - total_fees;
    }
};

/// Portfolio management
class Portfolio {
private:
    std::unordered_map<std::string, Position> positions_;
    double initial_capital_;
    double available_cash_;
    std::vector<FillEvent> trade_history_;
    
public:
    explicit Portfolio(double initial_capital) 
        : initial_capital_(initial_capital), available_cash_(initial_capital) {}
    
    /// Process fill event and update positions
    void process_fill(const FillEvent& fill);
    
    /// Get position for symbol
    [[nodiscard]] const Position* get_position(const std::string& symbol) const;
    
    /// Get current portfolio value
    [[nodiscard]] double portfolio_value(const std::unordered_map<std::string, Price>& market_prices) const;
    
    /// Get total realized PnL
    [[nodiscard]] double total_realized_pnl() const;
    
    /// Get total unrealized PnL
    [[nodiscard]] double total_unrealized_pnl() const;
    
    /// Get available cash
    [[nodiscard]] double available_cash() const noexcept { return available_cash_; }
    
    /// Get trade history
    [[nodiscard]] const std::vector<FillEvent>& trade_history() const noexcept {
        return trade_history_;
    }
    
    /// Reset portfolio to initial state
    void reset();
};

/// Abstract strategy base class
class Strategy {
protected:
    std::string strategy_id_;
    Portfolio* portfolio_{nullptr};
    std::function<void(std::shared_ptr<Event>)> event_publisher_;
    
public:
    explicit Strategy(std::string id) : strategy_id_(std::move(id)) {}
    virtual ~Strategy() = default;
    
    /// Initialize strategy
    virtual void initialize() {}
    
    /// Handle market data event
    virtual void on_market_data(const MarketEvent& event) = 0;
    
    /// Handle fill event
    virtual void on_fill(const FillEvent& event) {}
    
    /// Handle timer event
    virtual void on_timer(const TimerEvent& event) {}
    
    /// Set portfolio reference
    void set_portfolio(Portfolio* portfolio) { portfolio_ = portfolio; }
    
    /// Set event publisher
    void set_event_publisher(std::function<void(std::shared_ptr<Event>)> publisher) {
        event_publisher_ = std::move(publisher);
    }
    
    /// Get strategy ID
    [[nodiscard]] const std::string& strategy_id() const noexcept { return strategy_id_; }

protected:
    /// Helper function to place orders
    void place_order(const std::string& symbol, Side side, Price price, Quantity quantity,
                     OrderType type = OrderType::Limit, Timestamp delay = Timestamp{0});
    
    /// Helper function to cancel orders
    void cancel_order(OrderId order_id, Timestamp delay = Timestamp{0});
};

/// Backtesting execution handler
class ExecutionHandler {
private:
    std::unordered_map<std::string, std::unique_ptr<OrderBook>> order_books_;
    std::function<void(std::shared_ptr<Event>)> event_publisher_;
    double commission_rate_{0.0001};  // 1 bps default
    
public:
    /// Set event publisher
    void set_event_publisher(std::function<void(std::shared_ptr<Event>)> publisher) {
        event_publisher_ = std::move(publisher);
    }
    
    /// Process market data event
    void process_market_event(const MarketEvent& event);
    
    /// Process order event  
    void process_order_event(const OrderEvent& event);
    
    /// Get order book for symbol
    [[nodiscard]] OrderBook* get_order_book(const std::string& symbol);
    
    /// Set commission rate
    void set_commission_rate(double rate) noexcept { commission_rate_ = rate; }
    
    /// Calculate commission for trade
    [[nodiscard]] double calculate_commission(Price price, Quantity quantity) const noexcept {
        return static_cast<double>(price * quantity) * commission_rate_;
    }
};

/// Main backtesting engine
class Backtester {
private:
    std::priority_queue<std::shared_ptr<Event>, std::vector<std::shared_ptr<Event>>, EventComparator> event_queue_;
    std::vector<std::unique_ptr<Strategy>> strategies_;
    std::unique_ptr<Portfolio> portfolio_;
    std::unique_ptr<ExecutionHandler> execution_handler_;
    
    Timestamp current_time_{0};
    bool running_{false};
    
    /// Event handlers
    void handle_market_event(const MarketEvent& event);
    void handle_signal_event(const SignalEvent& event);
    void handle_order_event(const OrderEvent& event);
    void handle_fill_event(const FillEvent& event);
    void handle_timer_event(const TimerEvent& event);
    
public:
    /// Constructor
    explicit Backtester(double initial_capital = 1000000.0);
    
    /// Add strategy
    void add_strategy(std::unique_ptr<Strategy> strategy);
    
    /// Load market data from file
    bool load_market_data(const std::string& filename);
    
    /// Add event to queue
    void add_event(std::shared_ptr<Event> event);
    
    /// Run backtest
    void run();
    
    /// Get portfolio
    [[nodiscard]] Portfolio* get_portfolio() const noexcept { return portfolio_.get(); }
    
    /// Get current time
    [[nodiscard]] Timestamp current_time() const noexcept { return current_time_; }
    
    /// Check if backtester is running
    [[nodiscard]] bool is_running() const noexcept { return running_; }
    
    /// Stop backtester
    void stop() noexcept { running_ = false; }
    
    /// Get order book for symbol
    [[nodiscard]] OrderBook* get_order_book(const std::string& symbol) const {
        return execution_handler_->get_order_book(symbol);
    }
    
    /// Set commission rate
    void set_commission_rate(double rate) {
        execution_handler_->set_commission_rate(rate);
    }
};

/// Backtest results and performance metrics
struct BacktestResults {
    double total_return{0.0};
    double annualized_return{0.0};
    double sharpe_ratio{0.0};
    double sortino_ratio{0.0};
    double max_drawdown{0.0};
    double volatility{0.0};
    double beta{0.0};
    double alpha{0.0};
    double var_95{0.0};         // Value at Risk (95%)
    double cvar_95{0.0};        // Conditional VaR
    double calmar_ratio{0.0};   // Return / Max Drawdown
    
    size_t total_trades{0};
    size_t winning_trades{0};
    double win_rate{0.0};
    double avg_win{0.0};
    double avg_loss{0.0};
    double profit_factor{0.0};  // Gross profit / Gross loss
    
    double total_fees{0.0};
    double turnover{0.0};       // Total volume / Average AUM
    
    std::chrono::duration<double> backtest_duration{0};
    std::chrono::duration<double> simulation_time{0};
};

/// Performance analyzer
class PerformanceAnalyzer {
private:
    const Portfolio* portfolio_;
    std::vector<double> returns_;
    std::vector<double> portfolio_values_;
    std::vector<Timestamp> timestamps_;
    
public:
    explicit PerformanceAnalyzer(const Portfolio* portfolio) : portfolio_(portfolio) {}
    
    /// Add portfolio snapshot
    void add_snapshot(Timestamp timestamp, double portfolio_value);
    
    /// Calculate comprehensive performance metrics
    [[nodiscard]] BacktestResults calculate_performance() const;
    
    /// Calculate Sharpe ratio
    [[nodiscard]] double calculate_sharpe_ratio(double risk_free_rate = 0.0) const;
    
    /// Calculate maximum drawdown
    [[nodiscard]] double calculate_max_drawdown() const;
    
    /// Calculate Value at Risk
    [[nodiscard]] double calculate_var(double confidence = 0.95) const;
    
    /// Export results to CSV
    void export_to_csv(const std::string& filename) const;
};

} // namespace lob