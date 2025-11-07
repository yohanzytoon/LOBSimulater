#include "lob/backtester.hpp"
#include "lob/event.hpp"
#include <sstream>
#include <chrono>

namespace lob {

// -------- Position / Portfolio -----------

void Position::updatePosition(int64_t dq, double px) noexcept {
    if (dq == 0) return;
    if ((quantity >= 0 && dq > 0) || (quantity <= 0 && dq < 0)) {
        // add to same direction: update avg price
        const double notional_old = std::abs(static_cast<double>(quantity)) * average_price;
        const double notional_new = std::abs(static_cast<double>(dq)) * px;
        const double size_new = std::abs(static_cast<double>(quantity + dq));
        average_price = (size_new > 0.0) ? (notional_old + notional_new) / size_new : 0.0;
    } else {
        // reduce or flip: realize PnL on closed portion
        const double closed = std::min(std::abs(static_cast<double>(quantity)), std::abs(static_cast<double>(dq)));
        realized_pnl += (px - average_price) * (quantity > 0 ? closed : -closed);
    }
    quantity += dq;
    total_traded += static_cast<uint64_t>(std::abs(dq));
    if (quantity == 0) average_price = 0.0;
}

double Position::getUnrealizedPnL(double current_price) const noexcept {
    return (current_price - average_price) * static_cast<double>(quantity);
}

double Position::getTotalPnL(double current_price) const noexcept {
    return realized_pnl + getUnrealizedPnL(current_price);
}

// Portfolio
Portfolio::Portfolio(double initial_capital)
    : initial_capital_(initial_capital), cash_(initial_capital) {}

void Portfolio::updatePosition(const std::string& sym, int64_t dq, double px) {
    auto& pos = positions_[sym];
    pos.symbol = sym;
    const double traded_notional = std::abs(static_cast<double>(dq)) * px;
    const double commission = commission_rate_ * traded_notional;
    double slip = 0.0;
    if (slippage_model_) slip = slippage_model_(Order{});
    cash_ -= (static_cast<double>(dq) * px) + commission + slip;
    total_commission_ += commission;
    total_slippage_ += slip;
    pos.updatePosition(dq, px);
}

const Position* Portfolio::getPosition(const std::string& sym) const {
    auto it = positions_.find(sym);
    return (it == positions_.end()) ? nullptr : &it->second;
}

int64_t Portfolio::getNetPosition(const std::string& sym) const {
    auto p = getPosition(sym); return p ? p->quantity : 0;
}

double Portfolio::getRealizedPnL() const noexcept {
    double s = 0.0; for (auto& kv : positions_) s += kv.second.realized_pnl; return s;
}

double Portfolio::getUnrealizedPnL(const std::unordered_map<std::string,double>& pxs) const {
    double s = 0.0; for (auto& kv : positions_) {
        auto it = pxs.find(kv.first);
        if (it != pxs.end()) s += kv.second.getUnrealizedPnL(it->second);
    }
    return s;
}

double Portfolio::getTotalPnL(const std::unordered_map<std::string,double>& pxs) const {
    return getRealizedPnL() + getUnrealizedPnL(pxs);
}

double Portfolio::getEquity(const std::unordered_map<std::string,double>& pxs) const {
    return cash_ + getTotalPnL(pxs);
}

double Portfolio::getMarginUsed() const noexcept { return 0.0; }

double Portfolio::getLeverage(const std::unordered_map<std::string,double>& pxs) const {
    double gross = 0.0;
    for (auto& kv : positions_) {
        auto it = pxs.find(kv.first);
        if (it != pxs.end()) gross += std::abs(kv.second.quantity * it->second);
    }
    const double eq = getEquity(pxs);
    return eq > 0.0 ? gross / eq : 0.0;
}

void Portfolio::updateDrawdown(double eq) noexcept {
    max_equity_ = std::max(max_equity_, eq);
    max_drawdown_ = std::max(max_drawdown_, (max_equity_ - eq) / std::max(1e-12, max_equity_));
}

Portfolio::Snapshot Portfolio::takeSnapshot(Timestamp ts, const std::unordered_map<std::string,double>& pxs) const {
    Snapshot s{ts, getEquity(pxs), cash_, getRealizedPnL(), getUnrealizedPnL(pxs), positions_};
    return s;
}

// -------- CSVDataSource ----------

CSVDataSource::CSVDataSource(const std::string& filepath) : filepath_(filepath) {
    reset();
}

bool CSVDataSource::hasNext() const { return !buffer_.empty(); }

Event CSVDataSource::getNext() { auto e = buffer_.front(); buffer_.pop(); return e; }

void CSVDataSource::reset() {
    file_.close(); file_.clear(); file_.open(filepath_);
    buffer_ = std::queue<Event>();
    loadBuffer();
}

static std::vector<std::string> splitCSV(const std::string& s) {
    std::vector<std::string> out;
    std::string cur; cur.reserve(64);
    bool in_q = false;
    for (char c : s) {
        if (c == '"') { in_q = !in_q; continue; }
        if (c == ',' && !in_q) { out.push_back(cur); cur.clear(); }
        else cur.push_back(c);
    }
    out.push_back(cur);
    return out;
}

Event CSVDataSource::parseLine(const std::string& line) {
    // Expected columns (example):
    // timestamp_ns,symbol,type,side,price,quantity,order_id
    auto cols = splitCSV(line);
    Event e{};
    if (cols.size() < 3) return e;
    e.timestamp = static_cast<Timestamp>(std::stoull(cols[0]));
    e.symbol = cols[1];
    const std::string type = cols[2];
    
    if (type == "ADD") {
        e.type = Event::MARKET_DATA;
        MarketDataUpdate u{}; u.type = MarketDataUpdate::ADD_ORDER;
        u.timestamp = e.timestamp; u.side = (cols[3] == "BID" ? Side::BID : Side::ASK);
        u.price = static_cast<Price>(std::stoll(cols[4])); u.quantity = static_cast<Quantity>(std::stoul(cols[5]));
        u.order_id = static_cast<OrderId>(std::stoull(cols[6]));
        e.market_update = u;
    } else if (type == "CANCEL") {
        e.type = Event::MARKET_DATA;
        MarketDataUpdate u{}; u.type = MarketDataUpdate::CANCEL_ORDER;
        u.timestamp = e.timestamp; u.side = (cols[3] == "BID" ? Side::BID : Side::ASK);
        u.order_id = static_cast<OrderId>(std::stoull(cols[6]));
        e.market_update = u;
    } else if (type == "MODIFY") {
        e.type = Event::MARKET_DATA;
        MarketDataUpdate u{}; u.type = MarketDataUpdate::MODIFY_ORDER;
        u.timestamp = e.timestamp; u.side = (cols[3] == "BID" ? Side::BID : Side::ASK);
        u.order_id = static_cast<OrderId>(std::stoull(cols[6]));
        u.quantity = static_cast<Quantity>(std::stoul(cols[5]));
        e.market_update = u;
    } else if (type == "TRADE") {
        e.type = Event::FILL;
        Execution ex{0,0, static_cast<Price>(std::stoll(cols[4])), static_cast<Quantity>(std::stoul(cols[5])), e.timestamp};
        e.execution = ex;
    } else if (type == "EOD") {
        e.type = Event::END_OF_DAY;
    } else {
        e.type = Event::MARKET_DATA;
    }
    return e;
}

void CSVDataSource::loadBuffer() {
    std::string line;
    // skip header if present
    if (file_.good()) {
        std::getline(file_, line);
        if (line.find("timestamp") == std::string::npos) {
            buffer_.push(parseLine(line));
        }
    }
    while (std::getline(file_, line)) {
        if (line.empty()) continue;
        buffer_.push(parseLine(line));
    }
}

// -------- Backtester ----------

Backtester::Backtester() {
    portfolio_ = std::make_unique<Portfolio>(initial_capital_);
    signal_generator_ = std::make_unique<SignalGenerator>();
    // default signals
    signal_generator_->addCalculator(std::make_unique<OrderImbalanceSignal>(5, 0.3));
    signal_generator_->addCalculator(std::make_unique<MicropriceSignal>(1, true));
    signal_generator_->addCalculator(std::make_unique<SpreadSignal>(50));
}

void Backtester::addStrategy(std::unique_ptr<Strategy> s) { strategies_.push_back(std::move(s)); }

void Backtester::setDataSource(std::unique_ptr<DataSource> src) { data_source_ = std::move(src); }

OrderBook& Backtester::getOrCreateOrderBook(const std::string& sym) {
    auto it = order_books_.find(sym);
    if (it == order_books_.end()) {
        it = order_books_.emplace(sym, std::make_unique<OrderBook>(sym)).first;
    }
    return *it->second;
}

void Backtester::processMarketData(const Event& e) {
    const auto& u = *e.market_update;
    auto& book = getOrCreateOrderBook(e.symbol);
    
    switch (u.type) {
        case MarketDataUpdate::ADD_ORDER: {
            Order o{u.order_id, u.price, u.quantity, u.side, u.timestamp};
            book.addOrder(std::move(o));
            break;
        }
        case MarketDataUpdate::MODIFY_ORDER: {
            book.modifyOrder(u.order_id, u.quantity);
            break;
        }
        case MarketDataUpdate::CANCEL_ORDER: {
            book.cancelOrder(u.order_id);
            break;
        }
        case MarketDataUpdate::TRADE: {
            // not generally present in L3 add/cancel streams; handled by FILL events
            break;
        }
        case MarketDataUpdate::CLEAR: {
            book.clear();
            break;
        }
        case MarketDataUpdate::SNAPSHOT: {
            // ignore here
            break;
        }
    }
    
    current_prices_[e.symbol] = book.getMidPrice();
    signal_generator_->update(book);
    
    auto t0 = std::chrono::steady_clock::now();
    for (auto& strat : strategies_) {
        strat->onMarketData(u, book, *portfolio_);
    }
    auto t1 = std::chrono::steady_clock::now();
    perf_stats_.total_strategy_time += std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0);
    ++perf_stats_.events_processed;
}

void Backtester::processSignal(const Event& e) {
    auto& book = getOrCreateOrderBook(e.symbol);
    auto sigs = signal_generator_->generateSignals(book);
    auto t0 = std::chrono::steady_clock::now();
    for (auto& s : sigs) {
        for (auto& strat : strategies_) strat->onSignal(s, book, *portfolio_);
    }
    auto t1 = std::chrono::steady_clock::now();
    perf_stats_.total_signal_time += std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0);
}

void Backtester::processOrder(const Event& e) {
    auto& book = getOrCreateOrderBook(e.symbol);
    const auto& ord = *e.order;
    if (ord.type == OrderType::MARKET) {
        auto execs = book.processMarketOrder(ord.side, ord.quantity, e.timestamp);
        for (auto& ex : execs) {
            Event f{}; f.type = Event::FILL; f.timestamp = ex.timestamp; f.symbol = e.symbol; f.execution = ex;
            processFill(f);
        }
    } else {
        // add passive order
        auto o = ord;
        o.timestamp = e.timestamp;
        const bool ok = book.addOrder(std::move(o));
        (void)ok;
    }
    ++perf_stats_.orders_sent;
}

void Backtester::processFill(const Event& e) {
    const auto& ex = *e.execution;
    // infer side and qty sign
    const bool buy_fill = (ex.bid_id != 0);
    const int64_t dq = buy_fill ? static_cast<int64_t>(ex.quantity) : -static_cast<int64_t>(ex.quantity);
    const double px = priceToDouble(ex.price);
    portfolio_->updatePosition(e.symbol, dq, px);
    for (auto& strat : strategies_) strat->onFill(ex, *portfolio_);
    ++perf_stats_.orders_filled;
}

void Backtester::updateMetrics(Timestamp ts) {
    const double eq = portfolio_->getEquity(current_prices_);
    portfolio_history_.push_back(portfolio_->takeSnapshot(ts, current_prices_));
    (void)eq;
}

BacktestResult Backtester::run() {
    if (!data_source_) return {};
    strategies_.shrink_to_fit();
    for (auto& s : strategies_) s->onStart();
    portfolio_history_.clear();
    
    while (data_source_->hasNext()) {
        auto e = data_source_->getNext();
        switch (e.type) {
            case Event::MARKET_DATA: processMarketData(e); break;
            case Event::ORDER:       processOrder(e); break;
            case Event::FILL:        processFill(e); break;
            case Event::SIGNAL:      processSignal(e); break;
            case Event::END_OF_DAY:  updateMetrics(e.timestamp); break;
        }
    }
    for (auto& s : strategies_) s->onEnd(*portfolio_);
    
    // Build equity series
    std::vector<std::pair<std::uint64_t,double>> eq;
    eq.reserve(portfolio_history_.size());
    for (auto& snap : portfolio_history_) eq.emplace_back(snap.timestamp, snap.equity);
    
    std::vector<TradeRecord> trades; // (if you log per-fill, you can fill this)
    last_result_ = computeMetrics(eq, trades);
    return last_result_;
}


void Backtester::step(const Event& event) {
    processEvent(event);
}

void Backtester::processEvent(const Event& event) {
    switch (event.type) {
        case Event::MARKET_DATA: processMarketData(event); break;
        case Event::ORDER:       processOrder(event); break;
        case Event::FILL:        processFill(event); break;
        case Event::SIGNAL:      processSignal(event); break;
        case Event::END_OF_DAY:  updateMetrics(event.timestamp); break;
    }
}

// -------- Example Strategies ----------

MarketMakerStrategy::MarketMakerStrategy(double spread_bps, double size, double inv_lim)
    : spread_bps_(spread_bps), order_size_(size), max_inventory_(inv_lim) {}

void MarketMakerStrategy::onMarketData(const MarketDataUpdate&, const OrderBook& book, Portfolio& pf) {
    updateQuotes(book, pf);
}

void MarketMakerStrategy::onSignal(const Signal&, const OrderBook& book, Portfolio& pf) {
    updateQuotes(book, pf);
}

void MarketMakerStrategy::onFill(const Execution&, Portfolio&) {}

void MarketMakerStrategy::cancelAllOrders() { active_orders_.clear(); }

void MarketMakerStrategy::updateQuotes(const OrderBook& book, const Portfolio& pf) {
    const double mid = book.getMidPrice();
    if (mid == 0.0) return;
    const double tick_px = mid * (spread_bps_ * 1e-4);
    const double bid = mid - tick_px;
    const double ask = mid + tick_px;
    
    // naive inventory skew
    double skew = 0.0;
    // (if we had position we could look it up here)
    
    // generate orders (this method only updates state; actual order submit occurs in generateOrders)
    active_orders_.clear();
    OrderId idb = 100000 + std::rand();
    OrderId ida = 200000 + std::rand();
    active_orders_[idb] = Order{idb, doubleToPrice(bid - skew), static_cast<Quantity>(order_size_), Side::BID, 0};
    active_orders_[ida] = Order{ida, doubleToPrice(ask + skew), static_cast<Quantity>(order_size_), Side::ASK, 0};
}

std::vector<Order> MarketMakerStrategy::generateOrders(const OrderBook&, const Portfolio&) {
    std::vector<Order> v;
    v.reserve(active_orders_.size());
    for (auto& kv : active_orders_) v.push_back(kv.second);
    return v;
}

MomentumStrategy::MomentumStrategy(int lb, double entry, double exit)
    : lookback_periods_(lb), entry_z_score_(entry), exit_z_score_(exit) {}

void MomentumStrategy::onMarketData(const MarketDataUpdate&, const OrderBook& book, Portfolio&) {
    const double mid = book.getMidPrice();
    if (mid > 0.0) {
        price_history_.push_back(mid);
        if (static_cast<int>(price_history_.size()) > lookback_periods_) price_history_.pop_front();
    }
}

void MomentumStrategy::onSignal(const Signal&, const OrderBook&, Portfolio&) {}

void MomentumStrategy::onFill(const Execution&, Portfolio&) {}

double MomentumStrategy::calculateZScore() const {
    if (static_cast<int>(price_history_.size()) < lookback_periods_) return 0.0;
    const double mean = std::accumulate(price_history_.begin(), price_history_.end(), 0.0) / price_history_.size();
    double ss = 0.0; for (auto p : price_history_) { double d = p - mean; ss += d*d; }
    const double s = std::sqrt(ss / std::max<int>(1, static_cast<int>(price_history_.size()) - 1));
    return s > 0.0 ? (price_history_.back() - mean) / s : 0.0;
}

bool MomentumStrategy::shouldEnterLong() const { return calculateZScore() > entry_z_score_; }

bool MomentumStrategy::shouldExitPosition() const { return std::abs(calculateZScore()) < exit_z_score_; }

} // namespace lob