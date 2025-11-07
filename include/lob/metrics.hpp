#pragma once

#include <vector>
#include <unordered_map>
#include <string>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <cmath>

namespace lob {

// Drawdown point used to build equity curve with corresponding drawdown.
struct DrawdownPoint {
    std::uint64_t t{};
    double equity{};
    double peak{};
    double dd{};
};

// Aggregate result structure returned after running a backtest.  It
// contains risk and performance metrics including total return,
// annualised return, volatility, Sharpe/Sortino, maximum drawdown,
// turnover and capacity estimate.
struct BacktestResult {
    double total_return{0.0};
    double annualized_return{0.0};
    double volatility{0.0};
    double sharpe{0.0};
    double sortino{0.0};
    double max_drawdown{0.0};
    double calmar{0.0};
    double turnover{0.0};
    double capacity_estimate{0.0};
    std::size_t num_trades{0};
    std::vector<DrawdownPoint> equity_curve;
};

// Record of a single trade used in turnover and capacity calculations.
struct TradeRecord {
    std::uint64_t timestamp{};
    std::string symbol;
    int64_t qty{};
    double price{};
    double commission{};
    double slippage{};
};

// Compute a BacktestResult from an equity time series and a list of
// trade records.  The risk free rate and assumed trading day length can
// be customised.
BacktestResult computeMetrics(const std::vector<std::pair<std::uint64_t,double>>& equity_ts,
                              const std::vector<TradeRecord>& trades,
                              double risk_free_rate_annual = 0.0,
                              double trading_day_seconds = 6.5*3600.0);

double computeSharpe(const std::vector<double>& rets, double rf_per_period = 0.0);
double computeSortino(const std::vector<double>& rets, double rf_per_period = 0.0);
double computeMaxDrawdown(const std::vector<std::pair<std::uint64_t,double>>& equity_ts,
                          std::vector<DrawdownPoint>* curve_out = nullptr);
double computeTurnover(const std::vector<TradeRecord>& trades);
double estimateCapacity(const std::vector<TradeRecord>& trades, double impact_coef_bps = 0.1);

} // namespace lob