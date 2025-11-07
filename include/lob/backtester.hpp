#pragma once

#include "lob/order_book.hpp"
#include "lob/signals.hpp"
#include "lob/metrics.hpp"

#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <queue>
#include <unordered_map>
#include <fstream>

namespace lob {

// Forward declarations
class Strategy;
class Portfolio;
class EventQueue;

// Event types for backtesting
struct Event {
    enum Type : uint8_t {
        MARKET_DATA,
        SIGNAL,
        ORDER,
        FILL,
        END_OF_DAY
    };
    
    Type type;
    Timestamp timestamp;
    std::string symbol;
    
    // Market data fields
    std::optional<MarketDataUpdate> market_update;
    
    // Signal fields
    std::optional<Signal> signal;
    
    // Order fields
    std::optional<Order> order;
    
    // Fill fields
    std::optional<Execution> execution;
    
    bool operator<(const Event& other) const {
        return timestamp > other.timestamp;  // Min heap by timestamp
    }
};

// Position tracking
struct Position {
    std::string symbol;
    int64_t quantity = 0;  // Positive = long, negative = short
    double average_price = 0.0;
    double realized_pnl = 0.0;
    double unrealized_pnl = 0.0;
    uint64_t total_traded = 0;
    
    void updatePosition(int64_t qty_change, double price) noexcept;
    [[nodiscard]] double getUnrealizedPnL(double current_price) const noexcept;
    [[nodiscard]] double getTotalPnL(double current_price) const noexcept;
    [[nodiscard]] bool isFlat() const noexcept { return quantity == 0; }
};

// Portfolio management
class Portfolio {
public:
    explicit Portfolio(double initial_capital = 1000000.0);
    
    // Position management
    void updatePosition(const std::string& symbol, int64_t qty_change, double price);
    [[nodiscard]] const Position* getPosition(const std::string& symbol) const;
    [[nodiscard]] int64_t getNetPosition(const std::string& symbol) const;
    
    // P&L calculations
    [[nodiscard]] double getRealizedPnL() const noexcept;
    [[nodiscard]] double getUnrealizedPnL(const std::unordered_map<std::string, double>& prices) const;
    [[nodiscard]] double getTotalPnL(const std::unordered_map<std::string, double>& prices) const;
    
    // Risk metrics
    [[nodiscard]] double getEquity(const std::unordered_map<std::string, double>& prices) const;
    [[nodiscard]] double getMarginUsed() const noexcept;
    [[nodiscard]] double getLeverage(const std::unordered_map<std::string, double>& prices) const;
    [[nodiscard]] double getMaxDrawdown() const noexcept { return max_drawdown_; }
    
    // Transaction costs
    void setCommissionRate(double rate) noexcept { commission_rate_ = rate; }
    void setSlippageModel(std::function<double(const Order&)> model) { 
        slippage_model_ = std::move(model); 
    }
    
    // Portfolio snapshots
    struct Snapshot {
        Timestamp timestamp;
        double equity;
        double cash;
        double realized_pnl;
        double unrealized_pnl;
        std::unordered_map<std::string, Position> positions;
    };
    
    [[nodiscard]] Snapshot takeSnapshot(
        Timestamp timestamp, 
        const std::unordered_map<std::string, double>& prices) const;
    
private:
    double initial_capital_;
    double cash_;
    double commission_rate_ = 0.0001;  // 1 bps default
    std::function<double(const Order&)> slippage_model_;
    
    std::unordered_map<std::string, Position> positions_;
    
    // Tracking
    double total_commission_ = 0.0;
    double total_slippage_ = 0.0;
    double max_equity_ = 0.0;
    double max_drawdown_ = 0.0;
    
    void updateDrawdown(double current_equity) noexcept;
};

// Strategy interface
class Strategy {
public:
    virtual ~Strategy() = default;
    
    // Called on each event
    virtual void onMarketData(const MarketDataUpdate& update, 
                             const OrderBook& book,
                             Portfolio& portfolio) = 0;
    
    virtual void onSignal(const Signal& signal,
                         const OrderBook& book,
                         Portfolio& portfolio) = 0;
    
    virtual void onFill(const Execution& execution,
                       Portfolio& portfolio) = 0;
    
    // Lifecycle callbacks
    virtual void onStart() {}
    virtual void onEnd(const Portfolio& portfolio) {}
    
    // Order generation
    [[nodiscard]] virtual std::vector<Order> generateOrders(
        const OrderBook& book,
        const Portfolio& portfolio) { return {}; }
    
    // Configuration
    void setParameters(const std::unordered_map<std::string, double>& params) {
        parameters_ = params;
    }
    
protected:
    std::unordered_map<std::string, double> parameters_;
    
    // Helper to get parameter with default
    [[nodiscard]] double getParam(const std::string& name, double default_val = 0.0) const {
        auto it = parameters_.find(name);
        return (it != parameters_.end()) ? it->second : default_val;
    }
};

// Market data source interface
class DataSource {
public:
    virtual ~DataSource() = default;
    virtual bool hasNext() const = 0;
    virtual Event getNext() = 0;
    virtual void reset() = 0;
};

// CSV file data source
class CSVDataSource : public DataSource {
public:
    explicit CSVDataSource(const std::string& filepath);
    
    bool hasNext() const override;
    Event getNext() override;
    void reset() override;
    
private:
    std::string filepath_;
    std::ifstream file_;
    std::queue<Event> buffer_;
    
    void loadBuffer();
    Event parseLine(const std::string& line);
};

// Main backtester engine
class Backtester {
public:
    Backtester();
    ~Backtester() = default;
    
    // Configuration
    void addStrategy(std::unique_ptr<Strategy> strategy);
    void setDataSource(std::unique_ptr<DataSource> source);
    void setInitialCapital(double capital) {
        initial_capital_ = capital;
        // reset portfolio to new capital
        portfolio_ = std::make_unique<Portfolio>(capital);
    }
    void setCommissionRate(double rate) {
        commission_rate_ = rate;
        if (portfolio_) portfolio_->setCommissionRate(rate);
    }
    
    // Run backtest
    BacktestResult run();
    
    // Real‑time simulation mode
    void step(const Event& event);
    void processEvent(const Event& event);
    
    // Analysis
    [[nodiscard]] const Portfolio& getPortfolio() const { return *portfolio_; }
    [[nodiscard]] const BacktestResult& getResults() const { return last_result_; }
    
    // Performance profiling
    struct PerformanceStats {
        uint64_t events_processed = 0;
        uint64_t orders_sent = 0;
        uint64_t orders_filled = 0;
        std::chrono::nanoseconds total_strategy_time{0};
        std::chrono::nanoseconds total_matching_time{0};
        std::chrono::nanoseconds total_signal_time{0};
        
        [[nodiscard]] double getAverageStrategyLatency() const {
            return events_processed > 0 ? 
                   total_strategy_time.count() / static_cast<double>(events_processed) : 0.0;
        }
    };
    
    [[nodiscard]] const PerformanceStats& getPerformanceStats() const { 
        return perf_stats_; 
    }
    
private:
    // Components
    std::vector<std::unique_ptr<Strategy>> strategies_;
    std::unique_ptr<DataSource> data_source_;
    std::unique_ptr<Portfolio> portfolio_;
    std::unordered_map<std::string, std::unique_ptr<OrderBook>> order_books_;
    std::unique_ptr<SignalGenerator> signal_generator_;
    
    // Configuration
    double initial_capital_ = 1000000.0;
    double commission_rate_ = 0.0001;
    
    // Event processing
    std::priority_queue<Event> event_queue_;
    std::unordered_map<std::string, double> current_prices_;
    
    // Results
    BacktestResult last_result_;
    PerformanceStats perf_stats_;
    std::vector<Portfolio::Snapshot> portfolio_history_;
    
    // Helper methods
    void processMarketData(const Event& event);
    void processSignal(const Event& event);
    void processOrder(const Event& event);
    void processFill(const Event& event);
    void updateMetrics(Timestamp timestamp);
    
    OrderBook& getOrCreateOrderBook(const std::string& symbol);
};

// Example strategy implementations
class MarketMakerStrategy : public Strategy {
public:
    explicit MarketMakerStrategy(double spread_bps = 10.0, 
                                 double size = 100.0,
                                 double inventory_limit = 1000.0);
    
    void onMarketData(const MarketDataUpdate& update,
                     const OrderBook& book,
                     Portfolio& portfolio) override;
    
    void onSignal(const Signal& signal,
                 const OrderBook& book,
                 Portfolio& portfolio) override;
    
    void onFill(const Execution& execution,
               Portfolio& portfolio) override;
    
    std::vector<Order> generateOrders(const OrderBook& book,
                                     const Portfolio& portfolio) override;
    
private:
    double spread_bps_;
    double order_size_;
    double max_inventory_;
    std::unordered_map<OrderId, Order> active_orders_;
    
    void cancelAllOrders();
    void updateQuotes(const OrderBook& book, const Portfolio& portfolio);
};

class MomentumStrategy : public Strategy {
public:
    explicit MomentumStrategy(int lookback_periods = 20,
                             double entry_threshold = 2.0,
                             double exit_threshold = 0.5);
    
    void onMarketData(const MarketDataUpdate& update,
                     const OrderBook& book,
                     Portfolio& portfolio) override;
    
    void onSignal(const Signal& signal,
                 const OrderBook& book,
                 Portfolio& portfolio) override;
    
    void onFill(const Execution& execution,
               Portfolio& portfolio) override;
    
private:
    int lookback_periods_;
    double entry_z_score_;
    double exit_z_score_;
    
    std::deque<double> price_history_;
    bool in_position_ = false;
    
    [[nodiscard]] double calculateZScore() const;
    [[nodiscard]] bool shouldEnterLong() const;
    [[nodiscard]] bool shouldExitPosition() const;
};

} // namespace lob