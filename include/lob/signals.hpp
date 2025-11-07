#pragma once

#include "lob/order_book.hpp"
#include <vector>
#include <deque>
#include <cmath>
#include <numeric>
#include <optional>
#include <unordered_map>

namespace lob {

// A signal encapsulates a microstructure metric derived from the order
// book.  Each signal has a type, a symbol, a value and a confidence
// score.  Additional metadata can be supplied via the metadata map.
struct Signal {
    enum Type : uint8_t {
        ORDER_IMBALANCE, MICROPRICE, SPREAD, VOLATILITY, MOMENTUM,
        MEAN_REVERSION, TRADE_FLOW, QUEUE_POSITION, BOOK_PRESSURE, CUSTOM
    };
    Type type;
    std::string symbol;
    double value;
    double confidence;
    Timestamp timestamp;
    std::unordered_map<std::string, double> metadata;
    
    Signal(Type t, const std::string& sym, double val, double conf = 1.0)
        : type(t), symbol(sym), value(val), confidence(conf),
          timestamp(std::chrono::steady_clock::now().time_since_epoch().count()) {}
};

// Abstract base class for signal calculators.  Concrete classes must
// implement calculate() and may override update() and reset() if they
// maintain internal state.
class SignalCalculator {
public:
    virtual ~SignalCalculator() = default;
    [[nodiscard]] virtual Signal calculate(const OrderBook& book) const = 0;
    [[nodiscard]] virtual std::string getName() const = 0;
    virtual void update(const OrderBook&) {}
    virtual void reset() {}
};

// Order imbalance calculates the difference between bid and ask volume
// over the top N levels.  Positive values indicate buying pressure.
class OrderImbalanceSignal : public SignalCalculator {
public:
    explicit OrderImbalanceSignal(int levels = 5, double threshold = 0.3)
        : levels_(levels), threshold_(threshold) {}
    [[nodiscard]] Signal calculate(const OrderBook& book) const override;
    [[nodiscard]] std::string getName() const override { return "OrderImbalance"; }
    [[nodiscard]] double getVolumeImbalance(const OrderBook& book) const;
    [[nodiscard]] double getOrderCountImbalance(const OrderBook& book) const;
    [[nodiscard]] double getWeightedImbalance(const OrderBook& book) const;
private:
    int levels_;
    double threshold_;
    [[nodiscard]] double calculateImbalance(Quantity bid_v, Quantity ask_v) const;
};

// Microprice signal returns the microprice (weighted mid) of the book.
class MicropriceSignal : public SignalCalculator {
public:
    explicit MicropriceSignal(int levels = 1, bool use_size_weighting = true)
        : levels_(levels), use_size_weighting_(use_size_weighting) {}
    [[nodiscard]] Signal calculate(const OrderBook& book) const override;
    [[nodiscard]] std::string getName() const override { return "Microprice"; }
    [[nodiscard]] double getSimpleMicroprice(const OrderBook& book) const;
    [[nodiscard]] double getWeightedMicroprice(const OrderBook& book) const;
    [[nodiscard]] double getDepthWeightedMicroprice(const OrderBook& book) const;
private:
    int levels_;
    bool use_size_weighting_;
};

// Book pressure measures the relative aggression of recent quoting on
// each side of the book.  Higher net pressure implies stronger buying
// or selling momentum.
class BookPressureSignal : public SignalCalculator {
public:
    explicit BookPressureSignal(int lookback_events = 100)
        : lookback_events_(lookback_events) {}
    [[nodiscard]] Signal calculate(const OrderBook& book) const override;
    [[nodiscard]] std::string getName() const override { return "BookPressure"; }
    void update(const OrderBook& book) override;
    void reset() override { recent_events_.clear(); }
    [[nodiscard]] double getBuyPressure() const;
    [[nodiscard]] double getSellPressure() const;
    [[nodiscard]] double getNetPressure() const { return getBuyPressure() - getSellPressure(); }
private:
    int lookback_events_;
    struct PressureEvent { Timestamp timestamp; Side side; double aggression_score; };
    std::deque<PressureEvent> recent_events_;
    [[nodiscard]] double calculateAggression(const Order& order, const OrderBook& book) const;
};

// Trade flow compares recent aggressive buy and sell volume.  It uses a
// decayed sum of trade quantities to emphasise recent trades.
class TradeFlowSignal : public SignalCalculator {
public:
    explicit TradeFlowSignal(int lookback_trades = 50, double decay_factor = 0.95)
        : lookback_trades_(lookback_trades), decay_factor_(decay_factor) {}
    [[nodiscard]] Signal calculate(const OrderBook& book) const override;
    [[nodiscard]] std::string getName() const override { return "TradeFlow"; }
    void update(const OrderBook& book) override;
    void onTrade(const Execution& exec);
    void reset() override { recent_trades_.clear(); buy_volume_ = sell_volume_ = 0.0; }
    [[nodiscard]] double getBuyVolume() const { return buy_volume_; }
    [[nodiscard]] double getSellVolume() const { return sell_volume_; }
    [[nodiscard]] double getVWAP() const;
private:
    int lookback_trades_;
    double decay_factor_;
    struct Trade { Timestamp timestamp; Price price; Quantity quantity; Side aggressor_side; };
    std::deque<Trade> recent_trades_;
    double buy_volume_{0.0}, sell_volume_{0.0};
};

// Spread signal provides the z‑score of the current spread relative to
// its moving average and standard deviation.  It can be used to detect
// when spreads are unusually wide or tight.
class SpreadSignal : public SignalCalculator {
public:
    explicit SpreadSignal(int ma_periods = 20) : ma_periods_(ma_periods) {}
    [[nodiscard]] Signal calculate(const OrderBook& book) const override;
    [[nodiscard]] std::string getName() const override { return "Spread"; }
    void update(const OrderBook& book) override;
    void reset() override { spread_history_.clear(); }
    [[nodiscard]] double getCurrentSpread() const;
    [[nodiscard]] double getAverageSpread() const;
    [[nodiscard]] double getSpreadZScore() const;
    [[nodiscard]] bool isSpreadWide() const;
private:
    int ma_periods_;
    std::deque<double> spread_history_;
    [[nodiscard]] static double calculateMean(const std::deque<double>& xs);
    [[nodiscard]] static double calculateStdDev(const std::deque<double>& xs);
};

// Queue position signal estimates fill probability and expected fill
// time based on the number of shares ahead in the book.
class QueuePositionSignal : public SignalCalculator {
public:
    QueuePositionSignal() = default;
    [[nodiscard]] Signal calculate(const OrderBook& book) const override;
    [[nodiscard]] std::string getName() const override { return "QueuePosition"; }
    [[nodiscard]] double getExpectedFillTime(const Order& order, const OrderBook& book) const;
    [[nodiscard]] double getFillProbability(const Order& order, const OrderBook& book, int horizon_ms = 1000) const;
    [[nodiscard]] Quantity getQueueAhead(const Order& order, const OrderBook& book) const;
private:
    struct FillRateModel { double avg_fill_rate_per_ms{0.1}; double volatility{0.05}; } model_;
};

// Composite signal generator maintains a collection of calculators and
// returns signals on demand.  It also allows combining multiple
// signals with custom weights into a single custom signal.
class SignalGenerator {
public:
    SignalGenerator() = default;
    void addCalculator(std::unique_ptr<SignalCalculator> calc);
    [[nodiscard]] std::vector<Signal> generateSignals(const OrderBook& book);
    void update(const OrderBook& book);
    [[nodiscard]] std::optional<Signal> getSignal(const std::string& name, const OrderBook& book);
    [[nodiscard]] Signal combineSignals(const std::vector<Signal>& sigs, const std::vector<double>& weights);
    void reset();
private:
    std::vector<std::unique_ptr<SignalCalculator>> calculators_;
    std::unordered_map<std::string, SignalCalculator*> calculator_map_;
};

// Statistical utilities for signals
struct BookStats;
class SignalStatistics {
public:
    static double rollingMean(const std::deque<double>& v);
    static double rollingStdDev(const std::deque<double>& v);
    static double rollingSkewness(const std::deque<double>& v);
    static double rollingKurtosis(const std::deque<double>& v);
    static double correlation(const std::vector<double>& x, const std::vector<double>& y);
    static double zScore(double value, double mean, double stddev);
    static double ema(double new_value, double prev_ema, double alpha);
    static double percentile(std::vector<double> values, double pct);
};

// Machine learning feature extractor converts an order book snapshot and
// its recent history into a fixed‑length numeric vector.
class FeatureExtractor {
public:
    struct Features {
        double mid_price, spread, spread_pct;
        double bid_volume, ask_volume, volume_imbalance;
        double bid_depth_1, ask_depth_1, bid_depth_5, ask_depth_5;
        double microprice, book_pressure, queue_imbalance;
        double time_since_last_trade, time_of_day_normalized;
        double price_momentum, volume_momentum, volatility;
        [[nodiscard]] std::vector<double> toVector() const;
    };
    [[nodiscard]] static Features extractFeatures(const OrderBook& book);
    [[nodiscard]] static Features extractFeaturesWithHistory(const OrderBook& book, const std::deque<BookStats>& history);
    static void normalizeFeatures(Features& f, const Features& mean, const Features& stddev);
};

} // namespace lob