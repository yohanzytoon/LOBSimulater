#include "lob/metrics.hpp"
#include <numeric>

namespace lob {

static std::vector<double> diffs(const std::vector<std::pair<std::uint64_t,double>>& eq) {
    std::vector<double> r; r.reserve(eq.size()>1?eq.size()-1:0);
    for (size_t i=1;i<eq.size();++i) {
        const double ret = (eq[i].second - eq[i-1].second) / std::max(1e-12, eq[i-1].second);
        r.push_back(ret);
    }
    return r;
}

double computeSharpe(const std::vector<double>& rets, double rf) {
    if (rets.size()<2) return 0.0;
    double m=0.0; for (auto r: rets) m += (r - rf); m /= rets.size();
    double v=0.0; for (auto r: rets) { double d=(r - rf) - m; v+=d*d; }
    const double s = std::sqrt(v/std::max<size_t>(1, rets.size()-1));
    return s>0.0 ? m/s : 0.0;
}
double computeSortino(const std::vector<double>& rets, double rf) {
    if (rets.empty()) return 0.0;
    double m=0.0; for (auto r: rets) m += (r - rf); m /= rets.size();
    double dd=0.0; size_t n=0; for (auto r: rets) { double dr = r - rf; if (dr<0){ dd += dr*dr; ++n; } }
    const double s = (n>0) ? std::sqrt(dd / n) : 0.0;
    return s>0.0 ? m/s : 0.0;
}
double computeMaxDrawdown(const std::vector<std::pair<std::uint64_t,double>>& eq,
                          std::vector<DrawdownPoint>* out) {
    if (eq.empty()) return 0.0;
    double peak = eq.front().second, maxdd = 0.0;
    if (out) out->clear();
    for (auto& p: eq) {
        peak = std::max(peak, p.second);
        const double dd = (peak - p.second) / std::max(1e-12, peak);
        maxdd = std::max(maxdd, dd);
        if (out) out->push_back(DrawdownPoint{p.first, p.second, peak, dd});
    }
    return maxdd;
}
double computeTurnover(const std::vector<TradeRecord>& trades) {
    double gross=0.0; for (auto& t: trades) gross += std::abs(static_cast<double>(t.qty)) * t.price;
    return gross;
}
double estimateCapacity(const std::vector<TradeRecord>& trades, double impact_coef_bps) {
    // Simple Almgren-Chriss style linear impact proxy
    const double turnover = computeTurnover(trades);
    return turnover>0 ? std::max(0.0, 1.0 - impact_coef_bps*1e-4*turnover) : 1.0;
}

BacktestResult computeMetrics(const std::vector<std::pair<std::uint64_t,double>>& equity_ts,
                              const std::vector<TradeRecord>& trades,
                              double rf_annual,
                              double trading_day_seconds) {
    BacktestResult r{};
    if (equity_ts.size()<2) return r;

    const double start = equity_ts.front().second;
    const double end   = equity_ts.back().second;
    r.total_return = (end - start) / start;

    // Infer periodization from timestamps (seconds)
    const double seconds = static_cast<double>(equity_ts.back().first - equity_ts.front().first) / 1e9;
    const double days = std::max(1.0, seconds / trading_day_seconds);
    const double periods_per_year = 252.0; // daily-ish equity snapshots after EOD
    const auto rets = diffs(equity_ts);

    r.volatility = std::sqrt(std::max(0.0, std::accumulate(rets.begin(), rets.end(), 0.0,
                         [](double a,double b){return a + b*b;}) / std::max<size_t>(1, rets.size()-1))) * std::sqrt(periods_per_year);
    const double rf_per_period = rf_annual / periods_per_year;
    r.sharpe  = computeSharpe(rets, rf_per_period) * std::sqrt(periods_per_year);
    r.sortino = computeSortino(rets, rf_per_period) * std::sqrt(periods_per_year);
    r.max_drawdown = computeMaxDrawdown(equity_ts, &r.equity_curve);
    r.calmar = (r.max_drawdown>0.0) ? (r.total_return / r.max_drawdown) : 0.0;
    r.turnover = computeTurnover(trades);
    r.capacity_estimate = estimateCapacity(trades);
    r.annualized_return = std::pow(1.0 + r.total_return, 252.0/days) - 1.0;
    r.num_trades = trades.size();
    return r;
}

} // namespace lob