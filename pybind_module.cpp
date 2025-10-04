#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <pybind11/chrono.h>
#include <pybind11/functional.h>

#include <lob/order_book.hpp>
#include <lob/signals.hpp>
#include <lob/backtester.hpp>
#include <lob/metrics.hpp>

namespace py = pybind11;

PYBIND11_MODULE(lob_backtester, m) {
    m.doc() = "High-performance Limit Order Book Simulator & Event-Driven Backtester";
    
    // Enumerations
    py::enum_<lob::Side>(m, "Side")
        .value("Buy", lob::Side::Buy)
        .value("Sell", lob::Side::Sell);
        
    py::enum_<lob::OrderType>(m, "OrderType")
        .value("Market", lob::OrderType::Market)
        .value("Limit", lob::OrderType::Limit)
        .value("Stop", lob::OrderType::Stop)
        .value("StopLimit", lob::OrderType::StopLimit);
        
    py::enum_<lob::OrderStatus>(m, "OrderStatus")
        .value("New", lob::OrderStatus::New)
        .value("PartiallyFilled", lob::OrderStatus::PartiallyFilled)
        .value("Filled", lob::OrderStatus::Filled)
        .value("Cancelled", lob::OrderStatus::Cancelled)
        .value("Rejected", lob::OrderStatus::Rejected)
        .value("Expired", lob::OrderStatus::Expired);

    py::enum_<lob::EventType>(m, "EventType")
        .value("Market", lob::EventType::Market)
        .value("Signal", lob::EventType::Signal)
        .value("Order", lob::EventType::Order)
        .value("Fill", lob::EventType::Fill)
        .value("Timer", lob::EventType::Timer);

    // Type aliases
    m.attr("Price") = py::cast(static_cast<lob::Price*>(nullptr));
    m.attr("Quantity") = py::cast(static_cast<lob::Quantity*>(nullptr));
    m.attr("OrderId") = py::cast(static_cast<lob::OrderId*>(nullptr));

    // Order structure
    py::class_<lob::Order>(m, "Order")
        .def(py::init<>())
        .def_readwrite("order_id", &lob::Order::order_id)
        .def_readwrite("symbol", &lob::Order::symbol)
        .def_readwrite("side", &lob::Order::side)
        .def_readwrite("type", &lob::Order::type)
        .def_readwrite("price", &lob::Order::price)
        .def_readwrite("quantity", &lob::Order::quantity)
        .def_readwrite("filled_quantity", &lob::Order::filled_quantity)
        .def_readwrite("status", &lob::Order::status)
        .def_readwrite("timestamp", &lob::Order::timestamp)
        .def_readwrite("client_id", &lob::Order::client_id)
        .def("is_active", &lob::Order::is_active)
        .def("remaining_quantity", &lob::Order::remaining_quantity)
        .def("is_filled", &lob::Order::is_filled)
        .def("__repr__", [](const lob::Order& order) {
            return "<Order " + std::to_string(order.order_id) + " " + 
                   order.symbol + " " + (order.side == lob::Side::Buy ? "BUY" : "SELL") + 
                   " " + std::to_string(order.quantity) + "@" + std::to_string(order.price) + ">";
        });

    // Level structure
    py::class_<lob::Level>(m, "Level")
        .def(py::init<>())
        .def_readonly("price", &lob::Level::price)
        .def_readonly("total_quantity", &lob::Level::total_quantity)
        .def_readonly("orders", &lob::Level::orders)
        .def("empty", &lob::Level::empty)
        .def("__repr__", [](const lob::Level& level) {
            return "<Level " + std::to_string(level.price) + 
                   " qty=" + std::to_string(level.total_quantity) +
                   " orders=" + std::to_string(level.orders.size()) + ">";
        });

    // Trade structure
    py::class_<lob::Trade>(m, "Trade")
        .def(py::init<>())
        .def_readonly("aggressor_order_id", &lob::Trade::aggressor_order_id)
        .def_readonly("passive_order_id", &lob::Trade::passive_order_id)
        .def_readonly("symbol", &lob::Trade::symbol)
        .def_readonly("aggressor_side", &lob::Trade::aggressor_side)
        .def_readonly("price", &lob::Trade::price)
        .def_readonly("quantity", &lob::Trade::quantity)
        .def_readonly("timestamp", &lob::Trade::timestamp)
        .def("__repr__", [](const lob::Trade& trade) {
            return "<Trade " + trade.symbol + " " + 
                   std::to_string(trade.quantity) + "@" + std::to_string(trade.price) + ">";
        });

    // OrderBook class
    py::class_<lob::OrderBook>(m, "OrderBook")
        .def(py::init<std::string, lob::Price>(), 
             py::arg("symbol"), py::arg("tick_size") = 1)
        .def("add_order", &lob::OrderBook::add_order,
             py::arg("side"), py::arg("price"), py::arg("quantity"), 
             py::arg("type") = lob::OrderType::Limit, py::arg("client_id") = "",
             "Add new order to the book")
        .def("cancel_order", &lob::OrderBook::cancel_order, py::arg("order_id"),
             "Cancel existing order")
        .def("modify_order", &lob::OrderBook::modify_order,
             py::arg("order_id"), py::arg("new_price"), py::arg("new_quantity"),
             "Modify existing order")
        .def("get_order", &lob::OrderBook::get_order, py::arg("order_id"),
             "Get order by ID")
        .def("best_bid", &lob::OrderBook::best_bid, "Get best bid price")
        .def("best_ask", &lob::OrderBook::best_ask, "Get best ask price")
        .def("mid_price", &lob::OrderBook::mid_price, "Get mid price")
        .def("spread", &lob::OrderBook::spread, "Get bid-ask spread")
        .def("best_bid_quantity", &lob::OrderBook::best_bid_quantity, "Get best bid quantity")
        .def("best_ask_quantity", &lob::OrderBook::best_ask_quantity, "Get best ask quantity")
        .def("get_bid_levels", &lob::OrderBook::get_bid_levels, py::arg("max_levels") = 10,
             "Get bid levels (L2 data)")
        .def("get_ask_levels", &lob::OrderBook::get_ask_levels, py::arg("max_levels") = 10,
             "Get ask levels (L2 data)")
        .def("get_trades", &lob::OrderBook::get_trades, "Get all trades")
        .def("clear_trades", &lob::OrderBook::clear_trades, "Clear trade history")
        .def("symbol", &lob::OrderBook::symbol, "Get symbol")
        .def("tick_size", &lob::OrderBook::tick_size, "Get tick size")
        .def("is_crossed", &lob::OrderBook::is_crossed, "Check if book is crossed")
        .def("order_count", &lob::OrderBook::order_count, "Get total order count")
        .def("get_stats", &lob::OrderBook::get_stats, "Get book statistics")
        .def("print_book", [](const lob::OrderBook& book, size_t max_levels) {
            std::ostringstream oss;
            book.print_book(oss, max_levels);
            return oss.str();
        }, py::arg("max_levels") = 5, "Print book state as string")
        .def("__repr__", [](const lob::OrderBook& book) {
            return "<OrderBook " + book.symbol() + 
                   " bid=" + std::to_string(book.best_bid()) +
                   " ask=" + std::to_string(book.best_ask()) +
                   " orders=" + std::to_string(book.order_count()) + ">";
        });

    // BookStats structure
    py::class_<lob::OrderBook::BookStats>(m, "BookStats")
        .def(py::init<>())
        .def_readonly("total_orders", &lob::OrderBook::BookStats::total_orders)
        .def_readonly("bid_levels", &lob::OrderBook::BookStats::bid_levels)
        .def_readonly("ask_levels", &lob::OrderBook::BookStats::ask_levels)
        .def_readonly("total_bid_quantity", &lob::OrderBook::BookStats::total_bid_quantity)
        .def_readonly("total_ask_quantity", &lob::OrderBook::BookStats::total_ask_quantity)
        .def_readonly("total_trades", &lob::OrderBook::BookStats::total_trades);

    // Signals class
    py::class_<lob::Signals>(m, "Signals")
        .def(py::init<const lob::OrderBook*>(), py::arg("book"))
        .def("order_imbalance", &lob::Signals::order_imbalance,
             "Calculate order imbalance (0.5 = balanced)")
        .def("microprice", &lob::Signals::microprice,
             "Calculate microprice using Stoikov's formulation")
        .def("weighted_mid_price", &lob::Signals::weighted_mid_price,
             "Calculate weighted mid-price")
        .def("book_pressure", &lob::Signals::book_pressure,
             py::arg("levels") = 5, py::arg("decay") = 0.5,
             "Calculate depth-weighted order book pressure")
        .def("price_impact", &lob::Signals::price_impact,
             py::arg("side"), py::arg("order_size"),
             "Estimate price impact of market order")
        .def("effective_spread", &lob::Signals::effective_spread,
             "Calculate effective spread")
        .def("order_flow_toxicity", &lob::Signals::order_flow_toxicity,
             py::arg("recent_trades"), py::arg("lookback_trades") = 50,
             "Calculate order flow toxicity (VPIN)")
        .def("realized_spread", &lob::Signals::realized_spread,
             py::arg("execution_price"), py::arg("execution_side"), py::arg("future_mid_price"),
             "Calculate realized spread")
        .def("book_resilience", &lob::Signals::book_resilience,
             "Calculate order book resilience")
        .def("get_market_quality", &lob::Signals::get_market_quality,
             "Get comprehensive market quality metrics")
        .def("queue_position", &lob::Signals::queue_position,
             py::arg("side"), py::arg("price"),
             "Calculate queue position for hypothetical order")
        .def("order_arrival_rate", &lob::Signals::order_arrival_rate,
             "Calculate order arrival rate proxy");

    // MarketQualityMetrics structure
    py::class_<lob::Signals::MarketQualityMetrics>(m, "MarketQualityMetrics")
        .def(py::init<>())
        .def_readonly("spread_bps", &lob::Signals::MarketQualityMetrics::spread_bps)
        .def_readonly("depth", &lob::Signals::MarketQualityMetrics::depth)
        .def_readonly("imbalance", &lob::Signals::MarketQualityMetrics::imbalance)
        .def_readonly("microprice", &lob::Signals::MarketQualityMetrics::microprice)
        .def_readonly("effective_spread", &lob::Signals::MarketQualityMetrics::effective_spread)
        .def_readonly("resilience", &lob::Signals::MarketQualityMetrics::resilience)
        .def_readonly("pressure", &lob::Signals::MarketQualityMetrics::pressure)
        .def_readonly("volatility_proxy", &lob::Signals::MarketQualityMetrics::volatility_proxy);

    // Position structure
    py::class_<lob::Position>(m, "Position")
        .def(py::init<>())
        .def_readonly("symbol", &lob::Position::symbol)
        .def_readonly("quantity", &lob::Position::quantity)
        .def_readonly("avg_price", &lob::Position::avg_price)
        .def_readonly("realized_pnl", &lob::Position::realized_pnl)
        .def_readonly("unrealized_pnl", &lob::Position::unrealized_pnl)
        .def_readonly("total_fees", &lob::Position::total_fees)
        .def("total_pnl", &lob::Position::total_pnl)
        .def("update", &lob::Position::update,
             py::arg("fill_side"), py::arg("fill_qty"), py::arg("fill_price"), py::arg("commission") = 0.0)
        .def("update_unrealized_pnl", &lob::Position::update_unrealized_pnl, py::arg("market_price"));

    // Portfolio class
    py::class_<lob::Portfolio>(m, "Portfolio")
        .def(py::init<double>(), py::arg("initial_capital"))
        .def("process_fill", &lob::Portfolio::process_fill, py::arg("fill"))
        .def("get_position", &lob::Portfolio::get_position, py::arg("symbol"),
             py::return_value_policy::reference_internal)
        .def("portfolio_value", &lob::Portfolio::portfolio_value, py::arg("market_prices"))
        .def("total_realized_pnl", &lob::Portfolio::total_realized_pnl)
        .def("total_unrealized_pnl", &lob::Portfolio::total_unrealized_pnl)
        .def("available_cash", &lob::Portfolio::available_cash)
        .def("trade_history", &lob::Portfolio::trade_history,
             py::return_value_policy::reference_internal)
        .def("reset", &lob::Portfolio::reset);

    // Event classes
    py::class_<lob::Event>(m, "Event")
        .def_readonly("type", &lob::Event::type)
        .def_readonly("timestamp", &lob::Event::timestamp)
        .def_readonly("symbol", &lob::Event::symbol);

    py::class_<lob::MarketEvent, lob::Event>(m, "MarketEvent")
        .def(py::init<lob::Timestamp, std::string, lob::MarketEvent::UpdateType, lob::OrderId, lob::Side, lob::Price, lob::Quantity>(),
             py::arg("timestamp"), py::arg("symbol"), py::arg("update_type"), 
             py::arg("order_id") = 0, py::arg("side") = lob::Side::Buy, 
             py::arg("price") = 0, py::arg("quantity") = 0)
        .def_readonly("update_type", &lob::MarketEvent::update_type)
        .def_readonly("order_id", &lob::MarketEvent::order_id)
        .def_readonly("side", &lob::MarketEvent::side)
        .def_readonly("price", &lob::MarketEvent::price)
        .def_readonly("quantity", &lob::MarketEvent::quantity);

    py::enum_<lob::MarketEvent::UpdateType>(m, "UpdateType")
        .value("Add", lob::MarketEvent::UpdateType::Add)
        .value("Modify", lob::MarketEvent::UpdateType::Modify)
        .value("Cancel", lob::MarketEvent::UpdateType::Cancel)
        .value("Trade", lob::MarketEvent::UpdateType::Trade);

    // Signal event
    py::class_<lob::SignalEvent, lob::Event>(m, "SignalEvent")
        .def(py::init<lob::Timestamp, std::string, lob::SignalEvent::SignalType, double, std::string>(),
             py::arg("timestamp"), py::arg("symbol"), py::arg("signal_type"), 
             py::arg("confidence"), py::arg("strategy_id"))
        .def_readonly("signal_type", &lob::SignalEvent::signal_type)
        .def_readonly("confidence", &lob::SignalEvent::confidence)
        .def_readonly("strategy_id", &lob::SignalEvent::strategy_id)
        .def_readonly("metadata", &lob::SignalEvent::metadata);

    py::enum_<lob::SignalEvent::SignalType>(m, "SignalType")
        .value("Buy", lob::SignalEvent::SignalType::Buy)
        .value("Sell", lob::SignalEvent::SignalType::Sell)
        .value("Hold", lob::SignalEvent::SignalType::Hold);

    // Fill event
    py::class_<lob::FillEvent, lob::Event>(m, "FillEvent")
        .def(py::init<lob::Timestamp, std::string, lob::OrderId, lob::Side, lob::Price, lob::Quantity, std::string, double>(),
             py::arg("timestamp"), py::arg("symbol"), py::arg("order_id"), 
             py::arg("side"), py::arg("fill_price"), py::arg("fill_quantity"), 
             py::arg("strategy_id"), py::arg("commission") = 0.0)
        .def_readonly("order_id", &lob::FillEvent::order_id)
        .def_readonly("side", &lob::FillEvent::side)
        .def_readonly("fill_price", &lob::FillEvent::fill_price)
        .def_readonly("fill_quantity", &lob::FillEvent::fill_quantity)
        .def_readonly("strategy_id", &lob::FillEvent::strategy_id)
        .def_readonly("commission", &lob::FillEvent::commission);

    // Strategy base class (abstract)
    py::class_<lob::Strategy>(m, "Strategy")
        .def("strategy_id", &lob::Strategy::strategy_id,
             py::return_value_policy::reference_internal)
        .def("initialize", &lob::Strategy::initialize)
        .def("on_market_data", &lob::Strategy::on_market_data, py::arg("event"))
        .def("on_fill", &lob::Strategy::on_fill, py::arg("event"))
        .def("on_timer", &lob::Strategy::on_timer, py::arg("event"));

    // Backtester class
    py::class_<lob::Backtester>(m, "Backtester")
        .def(py::init<double>(), py::arg("initial_capital") = 1000000.0)
        .def("add_strategy", [](lob::Backtester& self, std::unique_ptr<lob::Strategy> strategy) {
            self.add_strategy(std::move(strategy));
        }, py::arg("strategy"))
        .def("load_market_data", &lob::Backtester::load_market_data, py::arg("filename"))
        .def("add_event", &lob::Backtester::add_event, py::arg("event"))
        .def("run", &lob::Backtester::run)
        .def("get_portfolio", &lob::Backtester::get_portfolio,
             py::return_value_policy::reference_internal)
        .def("current_time", &lob::Backtester::current_time)
        .def("is_running", &lob::Backtester::is_running)
        .def("stop", &lob::Backtester::stop)
        .def("get_order_book", &lob::Backtester::get_order_book, py::arg("symbol"),
             py::return_value_policy::reference_internal)
        .def("set_commission_rate", &lob::Backtester::set_commission_rate, py::arg("rate"));

    // BacktestResults structure
    py::class_<lob::BacktestResults>(m, "BacktestResults")
        .def(py::init<>())
        .def_readonly("total_return", &lob::BacktestResults::total_return)
        .def_readonly("annualized_return", &lob::BacktestResults::annualized_return)
        .def_readonly("sharpe_ratio", &lob::BacktestResults::sharpe_ratio)
        .def_readonly("sortino_ratio", &lob::BacktestResults::sortino_ratio)
        .def_readonly("max_drawdown", &lob::BacktestResults::max_drawdown)
        .def_readonly("volatility", &lob::BacktestResults::volatility)
        .def_readonly("beta", &lob::BacktestResults::beta)
        .def_readonly("alpha", &lob::BacktestResults::alpha)
        .def_readonly("var_95", &lob::BacktestResults::var_95)
        .def_readonly("cvar_95", &lob::BacktestResults::cvar_95)
        .def_readonly("calmar_ratio", &lob::BacktestResults::calmar_ratio)
        .def_readonly("total_trades", &lob::BacktestResults::total_trades)
        .def_readonly("winning_trades", &lob::BacktestResults::winning_trades)
        .def_readonly("win_rate", &lob::BacktestResults::win_rate)
        .def_readonly("avg_win", &lob::BacktestResults::avg_win)
        .def_readonly("avg_loss", &lob::BacktestResults::avg_loss)
        .def_readonly("profit_factor", &lob::BacktestResults::profit_factor)
        .def_readonly("total_fees", &lob::BacktestResults::total_fees)
        .def_readonly("turnover", &lob::BacktestResults::turnover)
        .def_readonly("backtest_duration", &lob::BacktestResults::backtest_duration)
        .def_readonly("simulation_time", &lob::BacktestResults::simulation_time);

    // Performance analyzer
    py::class_<lob::PerformanceAnalyzer>(m, "PerformanceAnalyzer")
        .def(py::init<const lob::Portfolio*>(), py::arg("portfolio"))
        .def("add_snapshot", &lob::PerformanceAnalyzer::add_snapshot,
             py::arg("timestamp"), py::arg("portfolio_value"))
        .def("calculate_performance", &lob::PerformanceAnalyzer::calculate_performance)
        .def("calculate_sharpe_ratio", &lob::PerformanceAnalyzer::calculate_sharpe_ratio,
             py::arg("risk_free_rate") = 0.0)
        .def("calculate_max_drawdown", &lob::PerformanceAnalyzer::calculate_max_drawdown)
        .def("calculate_var", &lob::PerformanceAnalyzer::calculate_var,
             py::arg("confidence") = 0.95)
        .def("export_to_csv", &lob::PerformanceAnalyzer::export_to_csv, py::arg("filename"));

    // Statistics utilities
    py::class_<lob::Statistics>(m, "Statistics")
        .def_static("mean", [](const std::vector<double>& values) {
            return lob::Statistics::mean(values);
        })
        .def_static("standard_deviation", [](const std::vector<double>& values) {
            return lob::Statistics::standard_deviation(values);
        })
        .def_static("skewness", [](const std::vector<double>& values) {
            return lob::Statistics::skewness(values);
        })
        .def_static("kurtosis", [](const std::vector<double>& values) {
            return lob::Statistics::kurtosis(values);
        })
        .def_static("percentile", [](std::vector<double> values, double p) {
            return lob::Statistics::percentile(values, p);
        })
        .def_static("correlation", [](const std::vector<double>& x, const std::vector<double>& y) {
            return lob::Statistics::correlation(x, y);
        })
        .def_static("beta", [](const std::vector<double>& x, const std::vector<double>& y) {
            return lob::Statistics::beta(x, y);
        });

    // Performance metrics
    py::class_<lob::PerformanceMetrics>(m, "PerformanceMetrics")
        .def_static("annualized_return", &lob::PerformanceMetrics::annualized_return)
        .def_static("annualized_volatility", &lob::PerformanceMetrics::annualized_volatility,
                    py::arg("returns"), py::arg("periods_per_year") = 252.0)
        .def_static("sharpe_ratio", &lob::PerformanceMetrics::sharpe_ratio,
                    py::arg("returns"), py::arg("risk_free_rate") = 0.0, py::arg("periods_per_year") = 252.0)
        .def_static("sortino_ratio", &lob::PerformanceMetrics::sortino_ratio,
                    py::arg("returns"), py::arg("target_return") = 0.0, py::arg("periods_per_year") = 252.0)
        .def_static("win_rate", &lob::PerformanceMetrics::win_rate)
        .def_static("profit_factor", &lob::PerformanceMetrics::profit_factor);

    // Risk metrics
    py::class_<lob::RiskMetrics>(m, "RiskMetrics")
        .def_static("value_at_risk", &lob::RiskMetrics::value_at_risk,
                    py::arg("returns"), py::arg("confidence") = 0.95)
        .def_static("conditional_var", &lob::RiskMetrics::conditional_var,
                    py::arg("returns"), py::arg("confidence") = 0.95)
        .def_static("max_drawdown", &lob::RiskMetrics::max_drawdown)
        .def_static("downside_deviation", &lob::RiskMetrics::downside_deviation,
                    py::arg("returns"), py::arg("target_return") = 0.0);

    // Capacity analysis
    py::class_<lob::CapacityAnalysis::CapacityEstimate>(m, "CapacityEstimate")
        .def(py::init<>())
        .def_readonly("max_position_size", &lob::CapacityAnalysis::CapacityEstimate::max_position_size)
        .def_readonly("estimated_capacity", &lob::CapacityAnalysis::CapacityEstimate::estimated_capacity)
        .def_readonly("impact_cost_bps", &lob::CapacityAnalysis::CapacityEstimate::impact_cost_bps)
        .def_readonly("turnover_impact", &lob::CapacityAnalysis::CapacityEstimate::turnover_impact);

    py::class_<lob::CapacityAnalysis>(m, "CapacityAnalysis")
        .def_static("market_impact", &lob::CapacityAnalysis::market_impact,
                    py::arg("order_value"), py::arg("daily_volume"), py::arg("participation_rate") = 0.1)
        .def_static("kelly_position_size", &lob::CapacityAnalysis::kelly_position_size,
                    py::arg("expected_return"), py::arg("variance"), py::arg("risk_free_rate") = 0.0)
        .def_static("estimate_capacity", &lob::CapacityAnalysis::estimate_capacity,
                    py::arg("daily_volume"), py::arg("average_turnover"), 
                    py::arg("max_participation_rate") = 0.05, py::arg("max_impact_bps") = 50.0);

    // Helper functions for creating timestamps
    m.def("current_time", []() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch());
    }, "Get current timestamp");

    m.def("time_from_seconds", [](double seconds) {
        return std::chrono::nanoseconds(static_cast<int64_t>(seconds * 1e9));
    }, py::arg("seconds"), "Create timestamp from seconds");

    // Version information
    m.attr("__version__") = "1.0.0";
}