#pragma once

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace lob {

/// Statistical utilities for performance metrics
class Statistics {
public:
    /// Calculate mean of a vector
    template<typename T>
    static double mean(const std::vector<T>& values) noexcept {
        if (values.empty()) return 0.0;
        double sum = std::accumulate(values.begin(), values.end(), 0.0);
        return sum / values.size();
    }

    /// Calculate standard deviation
    template<typename T>
    static double standard_deviation(const std::vector<T>& values) noexcept {
        if (values.size() < 2) return 0.0;
        
        double avg = mean(values);
        double variance = 0.0;
        
        for (const auto& value : values) {
            double diff = static_cast<double>(value) - avg;
            variance += diff * diff;
        }
        
        variance /= (values.size() - 1);  // Sample standard deviation
        return std::sqrt(variance);
    }

    /// Calculate skewness (third moment)
    template<typename T>
    static double skewness(const std::vector<T>& values) noexcept {
        if (values.size() < 3) return 0.0;
        
        double avg = mean(values);
        double std_dev = standard_deviation(values);
        if (std_dev == 0.0) return 0.0;
        
        double skew = 0.0;
        for (const auto& value : values) {
            double normalized = (static_cast<double>(value) - avg) / std_dev;
            skew += normalized * normalized * normalized;
        }
        
        return skew / values.size();
    }

    /// Calculate kurtosis (fourth moment)
    template<typename T>
    static double kurtosis(const std::vector<T>& values) noexcept {
        if (values.size() < 4) return 0.0;
        
        double avg = mean(values);
        double std_dev = standard_deviation(values);
        if (std_dev == 0.0) return 0.0;
        
        double kurt = 0.0;
        for (const auto& value : values) {
            double normalized = (static_cast<double>(value) - avg) / std_dev;
            double squared = normalized * normalized;
            kurt += squared * squared;
        }
        
        return (kurt / values.size()) - 3.0;  // Excess kurtosis
    }

    /// Calculate percentile
    template<typename T>
    static double percentile(std::vector<T> values, double p) noexcept {
        if (values.empty()) return 0.0;
        if (p < 0.0) p = 0.0;
        if (p > 1.0) p = 1.0;
        
        std::sort(values.begin(), values.end());
        
        if (p == 1.0) return static_cast<double>(values.back());
        if (p == 0.0) return static_cast<double>(values.front());
        
        double index = p * (values.size() - 1);
        size_t lower = static_cast<size_t>(std::floor(index));
        size_t upper = static_cast<size_t>(std::ceil(index));
        
        if (lower == upper) {
            return static_cast<double>(values[lower]);
        }
        
        double weight = index - lower;
        return static_cast<double>(values[lower]) * (1.0 - weight) + 
               static_cast<double>(values[upper]) * weight;
    }

    /// Calculate correlation coefficient
    template<typename T, typename U>
    static double correlation(const std::vector<T>& x, const std::vector<U>& y) noexcept {
        if (x.size() != y.size() || x.size() < 2) return 0.0;
        
        double mean_x = mean(x);
        double mean_y = mean(y);
        
        double numerator = 0.0;
        double sum_x_sq = 0.0;
        double sum_y_sq = 0.0;
        
        for (size_t i = 0; i < x.size(); ++i) {
            double diff_x = static_cast<double>(x[i]) - mean_x;
            double diff_y = static_cast<double>(y[i]) - mean_y;
            
            numerator += diff_x * diff_y;
            sum_x_sq += diff_x * diff_x;
            sum_y_sq += diff_y * diff_y;
        }
        
        double denominator = std::sqrt(sum_x_sq * sum_y_sq);
        return (denominator == 0.0) ? 0.0 : numerator / denominator;
    }

    /// Calculate linear regression (beta coefficient)
    template<typename T, typename U>
    static double beta(const std::vector<T>& x, const std::vector<U>& y) noexcept {
        if (x.size() != y.size() || x.size() < 2) return 0.0;
        
        double mean_x = mean(x);
        double mean_y = mean(y);
        
        double numerator = 0.0;
        double denominator = 0.0;
        
        for (size_t i = 0; i < x.size(); ++i) {
            double diff_x = static_cast<double>(x[i]) - mean_x;
            double diff_y = static_cast<double>(y[i]) - mean_y;
            
            numerator += diff_x * diff_y;
            denominator += diff_x * diff_x;
        }
        
        return (denominator == 0.0) ? 0.0 : numerator / denominator;
    }
};

/// Risk metrics calculator
class RiskMetrics {
public:
    /// Calculate Value at Risk using historical simulation
    static double value_at_risk(std::vector<double> returns, double confidence = 0.95) noexcept {
        if (returns.empty()) return 0.0;
        
        // Sort returns in ascending order (worst returns first)
        std::sort(returns.begin(), returns.end());
        
        // Find the percentile corresponding to (1 - confidence)
        double percentile_level = 1.0 - confidence;
        return -Statistics::percentile(returns, percentile_level);  // Negative for loss
    }

    /// Calculate Conditional Value at Risk (Expected Shortfall)
    static double conditional_var(std::vector<double> returns, double confidence = 0.95) noexcept {
        if (returns.empty()) return 0.0;
        
        std::sort(returns.begin(), returns.end());
        
        double percentile_level = 1.0 - confidence;
        size_t cutoff_index = static_cast<size_t>(percentile_level * returns.size());
        
        if (cutoff_index == 0) return -returns[0];
        
        double sum = 0.0;
        for (size_t i = 0; i < cutoff_index; ++i) {
            sum += returns[i];
        }
        
        return -sum / cutoff_index;  // Negative for loss
    }

    /// Calculate maximum drawdown
    static double max_drawdown(const std::vector<double>& portfolio_values) noexcept {
        if (portfolio_values.size() < 2) return 0.0;
        
        double max_value = portfolio_values[0];
        double max_dd = 0.0;
        
        for (size_t i = 1; i < portfolio_values.size(); ++i) {
            max_value = std::max(max_value, portfolio_values[i]);
            double drawdown = (max_value - portfolio_values[i]) / max_value;
            max_dd = std::max(max_dd, drawdown);
        }
        
        return max_dd;
    }

    /// Calculate maximum drawdown duration (in periods)
    static size_t max_drawdown_duration(const std::vector<double>& portfolio_values) noexcept {
        if (portfolio_values.size() < 2) return 0;
        
        double peak_value = portfolio_values[0];
        size_t peak_index = 0;
        size_t max_duration = 0;
        size_t current_duration = 0;
        
        for (size_t i = 1; i < portfolio_values.size(); ++i) {
            if (portfolio_values[i] > peak_value) {
                peak_value = portfolio_values[i];
                peak_index = i;
                max_duration = std::max(max_duration, current_duration);
                current_duration = 0;
            } else {
                current_duration = i - peak_index;
            }
        }
        
        return std::max(max_duration, current_duration);
    }

    /// Calculate downside deviation (for Sortino ratio)
    static double downside_deviation(const std::vector<double>& returns, 
                                   double target_return = 0.0) noexcept {
        if (returns.size() < 2) return 0.0;
        
        double sum_negative_variance = 0.0;
        size_t negative_count = 0;
        
        for (double ret : returns) {
            if (ret < target_return) {
                double diff = ret - target_return;
                sum_negative_variance += diff * diff;
                negative_count++;
            }
        }
        
        if (negative_count == 0) return 0.0;
        
        return std::sqrt(sum_negative_variance / negative_count);
    }

    /// Calculate Calmar ratio (Annual return / Max drawdown)
    static double calmar_ratio(double annualized_return, double max_drawdown) noexcept {
        if (max_drawdown == 0.0) return std::numeric_limits<double>::infinity();
        return annualized_return / max_drawdown;
    }

    /// Calculate Sterling ratio
    static double sterling_ratio(double annualized_return, double avg_drawdown) noexcept {
        if (avg_drawdown == 0.0) return std::numeric_limits<double>::infinity();
        return annualized_return / avg_drawdown;
    }

    /// Calculate Burke ratio
    static double burke_ratio(const std::vector<double>& returns, 
                            const std::vector<double>& portfolio_values,
                            double risk_free_rate = 0.0) noexcept {
        if (returns.empty() || portfolio_values.empty()) return 0.0;
        
        double excess_return = Statistics::mean(returns) - risk_free_rate;
        
        // Calculate sum of squared drawdowns
        double sum_sq_drawdowns = 0.0;
        double peak_value = portfolio_values[0];
        
        for (size_t i = 1; i < portfolio_values.size(); ++i) {
            peak_value = std::max(peak_value, portfolio_values[i]);
            double drawdown = (peak_value - portfolio_values[i]) / peak_value;
            sum_sq_drawdowns += drawdown * drawdown;
        }
        
        if (sum_sq_drawdowns == 0.0) return std::numeric_limits<double>::infinity();
        
        double sqrt_avg_sq_dd = std::sqrt(sum_sq_drawdowns / portfolio_values.size());
        return excess_return / sqrt_avg_sq_dd;
    }
};

/// Portfolio performance metrics
class PerformanceMetrics {
public:
    /// Calculate annualized return from total return and time period
    static double annualized_return(double total_return, double years) noexcept {
        if (years <= 0.0) return 0.0;
        if (total_return <= -1.0) return -1.0;  // Complete loss
        
        return std::pow(1.0 + total_return, 1.0 / years) - 1.0;
    }

    /// Calculate annualized volatility from periodic returns
    static double annualized_volatility(const std::vector<double>& returns, 
                                      double periods_per_year = 252.0) noexcept {
        double periodic_vol = Statistics::standard_deviation(returns);
        return periodic_vol * std::sqrt(periods_per_year);
    }

    /// Calculate Sharpe ratio
    static double sharpe_ratio(const std::vector<double>& returns, 
                              double risk_free_rate = 0.0,
                              double periods_per_year = 252.0) noexcept {
        if (returns.empty()) return 0.0;
        
        double mean_return = Statistics::mean(returns);
        double excess_return = mean_return - (risk_free_rate / periods_per_year);
        double volatility = Statistics::standard_deviation(returns);
        
        if (volatility == 0.0) return 0.0;
        
        return (excess_return / volatility) * std::sqrt(periods_per_year);
    }

    /// Calculate Sortino ratio
    static double sortino_ratio(const std::vector<double>& returns,
                               double target_return = 0.0,
                               double periods_per_year = 252.0) noexcept {
        if (returns.empty()) return 0.0;
        
        double mean_return = Statistics::mean(returns);
        double excess_return = mean_return - (target_return / periods_per_year);
        double downside_dev = RiskMetrics::downside_deviation(returns, target_return / periods_per_year);
        
        if (downside_dev == 0.0) return 0.0;
        
        return (excess_return / downside_dev) * std::sqrt(periods_per_year);
    }

    /// Calculate Information Ratio
    static double information_ratio(const std::vector<double>& portfolio_returns,
                                  const std::vector<double>& benchmark_returns) noexcept {
        if (portfolio_returns.size() != benchmark_returns.size() || portfolio_returns.empty()) {
            return 0.0;
        }
        
        std::vector<double> excess_returns;
        excess_returns.reserve(portfolio_returns.size());
        
        for (size_t i = 0; i < portfolio_returns.size(); ++i) {
            excess_returns.push_back(portfolio_returns[i] - benchmark_returns[i]);
        }
        
        double mean_excess = Statistics::mean(excess_returns);
        double tracking_error = Statistics::standard_deviation(excess_returns);
        
        return (tracking_error == 0.0) ? 0.0 : mean_excess / tracking_error;
    }

    /// Calculate Treynor ratio
    static double treynor_ratio(const std::vector<double>& portfolio_returns,
                               const std::vector<double>& market_returns,
                               double risk_free_rate = 0.0,
                               double periods_per_year = 252.0) noexcept {
        if (portfolio_returns.empty() || market_returns.empty()) return 0.0;
        
        double mean_portfolio_return = Statistics::mean(portfolio_returns);
        double excess_return = mean_portfolio_return - (risk_free_rate / periods_per_year);
        
        double portfolio_beta = Statistics::beta(market_returns, portfolio_returns);
        
        if (portfolio_beta == 0.0) return 0.0;
        
        return (excess_return / portfolio_beta) * periods_per_year;
    }

    /// Calculate Jensen's Alpha
    static double jensen_alpha(const std::vector<double>& portfolio_returns,
                              const std::vector<double>& market_returns,
                              double risk_free_rate = 0.0,
                              double periods_per_year = 252.0) noexcept {
        if (portfolio_returns.empty() || market_returns.empty()) return 0.0;
        
        double mean_portfolio_return = Statistics::mean(portfolio_returns);
        double mean_market_return = Statistics::mean(market_returns);
        double rf_rate_per_period = risk_free_rate / periods_per_year;
        
        double portfolio_beta = Statistics::beta(market_returns, portfolio_returns);
        
        double expected_return = rf_rate_per_period + 
                               portfolio_beta * (mean_market_return - rf_rate_per_period);
        
        return (mean_portfolio_return - expected_return) * periods_per_year;
    }

    /// Calculate win rate
    static double win_rate(const std::vector<double>& returns) noexcept {
        if (returns.empty()) return 0.0;
        
        size_t winning_trades = std::count_if(returns.begin(), returns.end(),
                                            [](double ret) { return ret > 0.0; });
        
        return static_cast<double>(winning_trades) / returns.size();
    }

    /// Calculate profit factor
    static double profit_factor(const std::vector<double>& returns) noexcept {
        if (returns.empty()) return 0.0;
        
        double gross_profit = 0.0;
        double gross_loss = 0.0;
        
        for (double ret : returns) {
            if (ret > 0.0) {
                gross_profit += ret;
            } else if (ret < 0.0) {
                gross_loss -= ret;  // Make positive
            }
        }
        
        return (gross_loss == 0.0) ? std::numeric_limits<double>::infinity() : 
               gross_profit / gross_loss;
    }

    /// Calculate average win and loss
    static std::pair<double, double> average_win_loss(const std::vector<double>& returns) noexcept {
        if (returns.empty()) return {0.0, 0.0};
        
        double total_wins = 0.0;
        double total_losses = 0.0;
        size_t win_count = 0;
        size_t loss_count = 0;
        
        for (double ret : returns) {
            if (ret > 0.0) {
                total_wins += ret;
                win_count++;
            } else if (ret < 0.0) {
                total_losses += ret;
                loss_count++;
            }
        }
        
        double avg_win = (win_count > 0) ? total_wins / win_count : 0.0;
        double avg_loss = (loss_count > 0) ? total_losses / loss_count : 0.0;
        
        return {avg_win, avg_loss};
    }

    /// Calculate expectancy (average profit per trade)
    static double expectancy(const std::vector<double>& returns) noexcept {
        if (returns.empty()) return 0.0;
        
        auto [avg_win, avg_loss] = average_win_loss(returns);
        double win_rate_val = win_rate(returns);
        double loss_rate = 1.0 - win_rate_val;
        
        return win_rate_val * avg_win + loss_rate * avg_loss;
    }
};

/// Portfolio capacity analysis
class CapacityAnalysis {
public:
    /// Estimate market impact based on order size and volume
    static double market_impact(double order_value, double daily_volume, 
                               double participation_rate = 0.1) noexcept {
        if (daily_volume <= 0.0) return 1.0;  // 100% impact if no volume
        
        double volume_participation = order_value / (daily_volume * participation_rate);
        
        // Square-root market impact model
        return 0.1 * std::sqrt(volume_participation);  // 10 bps * sqrt(participation)
    }

    /// Calculate optimal position size based on Kelly criterion
    static double kelly_position_size(double expected_return, double variance,
                                    double risk_free_rate = 0.0) noexcept {
        if (variance <= 0.0) return 0.0;
        
        double excess_return = expected_return - risk_free_rate;
        return excess_return / variance;
    }

    /// Calculate position size based on risk parity
    static double risk_parity_size(double target_risk, double asset_volatility) noexcept {
        if (asset_volatility <= 0.0) return 0.0;
        
        return target_risk / asset_volatility;
    }

    /// Estimate strategy capacity
    struct CapacityEstimate {
        double max_position_size{0.0};
        double estimated_capacity{0.0};
        double impact_cost_bps{0.0};
        double turnover_impact{0.0};
    };

    static CapacityEstimate estimate_capacity(double daily_volume,
                                            double average_turnover,
                                            double max_participation_rate = 0.05,
                                            double max_impact_bps = 50.0) noexcept {
        CapacityEstimate result;
        
        // Maximum position based on participation rate
        result.max_position_size = daily_volume * max_participation_rate;
        
        // Impact cost calculation
        result.impact_cost_bps = market_impact(result.max_position_size, daily_volume, 
                                             max_participation_rate) * 10000;
        
        // Capacity considering turnover
        result.turnover_impact = average_turnover * result.impact_cost_bps;
        
        // Estimated capacity (position size where impact cost is acceptable)
        if (result.impact_cost_bps <= max_impact_bps) {
            result.estimated_capacity = result.max_position_size;
        } else {
            // Scale down based on acceptable impact
            double scale_factor = max_impact_bps / result.impact_cost_bps;
            result.estimated_capacity = result.max_position_size * scale_factor;
        }
        
        return result;
    }
};

} // namespace lob