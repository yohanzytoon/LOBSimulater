#include "lob/signals.hpp"
#include <algorithm>
#include <limits>

namespace lob {

// ---------- OrderImbalance ----------
double OrderImbalanceSignal::calculateImbalance(Quantity b, Quantity a) const {
    const double bd = static_cast<double>(b), ad = static_cast<double>(a);
    const double denom = bd + ad;
    return denom > 0.0 ? (bd - ad) / denom : 0.0;
}

double OrderImbalanceSignal::getVolumeImbalance(const OrderBook& book) const {
    return book.getOrderImbalance(levels_);
}
double OrderImbalanceSignal::getOrderCountImbalance(const OrderBook& book) const {
    // approximate by treating each level's order_count equal weight via aggregated book
    auto bids = book.getAggregatedBook(Side::BID, levels_);
    auto asks = book.getAggregatedBook(Side::ASK, levels_);
    double bcnt = static_cast<double>(bids.size());
    double acnt = static_cast<double>(asks.size());
    return calculateImbalance(static_cast<Quantity>(bcnt), static_cast<Quantity>(acnt));
}
double OrderImbalanceSignal::getWeightedImbalance(const OrderBook& book) const {
    // volume imbalance weighted by inverse distance from touch
    auto bids = book.getAggregatedBook(Side::BID, levels_);
    auto asks = book.getAggregatedBook(Side::ASK, levels_);
    if (bids.empty() || asks.empty()) return 0.0;
    const auto best_bid = bids.front().first;
    const auto best_ask = asks.front().first;

    double bw = 0.0, aw = 0.0;
    for (std::size_t i=0;i<bids.size();++i) {
        auto [p, q] = bids[i];
        const double w = 1.0 / (1.0 + static_cast<double>(best_bid - p));
        bw += w * static_cast<double>(q);
    }
    for (std::size_t i=0;i<asks.size();++i) {
        auto [p, q] = asks[i];
        const double w = 1.0 / (1.0 + static_cast<double>(p - best_ask));
        aw += w * static_cast<double>(q);
    }
    return calculateImbalance(static_cast<Quantity>(bw), static_cast<Quantity>(aw));
}

Signal OrderImbalanceSignal::calculate(const OrderBook& book) const {
    const double vimb = getVolumeImbalance(book);
    Signal s{Signal::ORDER_IMBALANCE, book.symbol(), vimb, 1.0};
    s.metadata["weighted_imbalance"] = getWeightedImbalance(book);
    s.metadata["count_imbalance"]    = getOrderCountImbalance(book);
    s.confidence = std::min(1.0, std::abs(vimb)/std::max(1e-6, threshold_));
    return s;
}

// ---------- Microprice ----------
double MicropriceSignal::getSimpleMicroprice(const OrderBook& book) const {
    return book.getMidPrice();
}
double MicropriceSignal::getWeightedMicroprice(const OrderBook& book) const {
    return book.getMicroPrice(levels_);
}
double MicropriceSignal::getDepthWeightedMicroprice(const OrderBook& book) const {
    // linear blend of best and level-weighted
    const double w = use_size_weighting_ ? 0.7 : 0.5;
    const double mp_w = book.getMicroPrice(levels_);
    const double mid  = book.getMidPrice();
    return w*mp_w + (1.0-w)*mid;
}
Signal MicropriceSignal::calculate(const OrderBook& book) const {
    const double mp = use_size_weighting_ ? getWeightedMicroprice(book)
                                          : getSimpleMicroprice(book);
    Signal s{Signal::MICROPRICE, book.symbol(), mp, 1.0};
    s.metadata["mid"] = book.getMidPrice();
    s.metadata["spread"] = book.getSpread();
    return s;
}

// ---------- BookPressure ----------
double BookPressureSignal::calculateAggression(const Order& ord, const OrderBook& book) const {
    // simple proxy: distance to touch (ticks) normalized by spread
    const auto best_bid = book.getBestBid();
    const auto best_ask = book.getBestAsk();
    const auto spread = (best_bid==0 || best_ask==0) ? 1 : (best_ask - best_bid);
    if (ord.isBuy()) return static_cast<double>(ord.price - best_bid) / std::max(1, spread);
    return static_cast<double>(best_ask - ord.price) / std::max(1, spread);
}
void BookPressureSignal::update(const OrderBook& book) {
    // Take front orders on both sides as recent "aggressive quoting" proxies
    auto bids = book.getAggregatedBook(Side::BID, 1);
    auto asks = book.getAggregatedBook(Side::ASK, 1);
    if (!bids.empty()) {
        // fabricate an order shell
        Order tmp{}; tmp.side = Side::BID; tmp.price = bids.front().first; tmp.timestamp = 0;
        recent_events_.push_back({0, Side::BID, calculateAggression(tmp, book)});
    }
    if (!asks.empty()) {
        Order tmp{}; tmp.side = Side::ASK; tmp.price = asks.front().first; tmp.timestamp = 0;
        recent_events_.push_back({0, Side::ASK, calculateAggression(tmp, book)});
    }
    while (static_cast<int>(recent_events_.size()) > lookback_events_) recent_events_.pop_front();
}
double BookPressureSignal::getBuyPressure() const {
    double s=0.0; for (auto& e: recent_events_) if (e.side==Side::BID) s+=e.aggression_score; return s;
}
double BookPressureSignal::getSellPressure() const {
    double s=0.0; for (auto& e: recent_events_) if (e.side==Side::ASK) s+=e.aggression_score; return s;
}
Signal BookPressureSignal::calculate(const OrderBook& book) const {
    const double net = getBuyPressure() - getSellPressure();
    Signal s{Signal::BOOK_PRESSURE, book.symbol(), net, 1.0};
    s.metadata["buy_pressure"] = getBuyPressure();
    s.metadata["sell_pressure"] = getSellPressure();
    return s;
}

// ---------- TradeFlow ----------
void TradeFlowSignal::onTrade(const Execution& exec) {
    // Heuristic: if match price equals ask->aggressor buy; equals bid->aggressor sell
    // Here we can't see aggressor directly; assume bid_id!=0 => buyer was resting (sell initiated)
    const Side aggr = (exec.bid_id==0) ? Side::BID : (exec.ask_id==0 ? Side::ASK : Side::BID);
    recent_trades_.push_back(Trade{exec.timestamp, exec.price, exec.quantity, aggr});
    while (static_cast<int>(recent_trades_.size()) > lookback_trades_) recent_trades_.pop_front();
    // decay volumes
    buy_volume_ *= decay_factor_;
    sell_volume_ *= decay_factor_;
    if (aggr==Side::BID) buy_volume_ += exec.quantity;
    else                 sell_volume_ += exec.quantity;
}
void TradeFlowSignal::update(const OrderBook&) {}
double TradeFlowSignal::getVWAP() const {
    double pv=0.0, v=0.0;
    for (auto& t: recent_trades_) { pv += priceToDouble(t.price)*t.quantity; v += t.quantity; }
    return v>0.0 ? pv/v : 0.0;
}
Signal TradeFlowSignal::calculate(const OrderBook& book) const {
    const double tf = (buy_volume_ - sell_volume_) / std::max(1.0, buy_volume_ + sell_volume_);
    Signal s{Signal::TRADE_FLOW, book.symbol(), tf, 1.0};
    s.metadata["vwap"] = getVWAP();
    s.metadata["buy_vol"] = buy_volume_;
    s.metadata["sell_vol"] = sell_volume_;
    return s;
}

// ---------- Spread ----------
static double clamp(double x, double a, double b) { return std::max(a, std::min(b,x)); }
double SpreadSignal::getCurrentSpread() const { return 0.0; } // not used directly
void   SpreadSignal::update(const OrderBook& book) {
    spread_history_.push_back(book.getSpread());
    while (static_cast<int>(spread_history_.size()) > ma_periods_) spread_history_.pop_front();
}
double SpreadSignal::calculateMean(const std::deque<double>& xs) {
    if (xs.empty()) return 0.0;
    double s=0.0; for (auto v: xs) s+=v; return s/xs.size();
}
double SpreadSignal::calculateStdDev(const std::deque<double>& xs) {
    if (xs.size()<2) return 0.0;
    const double m = calculateMean(xs);
    double ss=0.0; for (auto v: xs) { const double d=v-m; ss+=d*d; }
    return std::sqrt(ss/(xs.size()-1));
}
double SpreadSignal::getAverageSpread() const { return calculateMean(spread_history_); }
double SpreadSignal::getSpreadZScore() const {
    if (spread_history_.empty()) return 0.0;
    const double cur = spread_history_.back();
    const double m = calculateMean(spread_history_);
    const double s = calculateStdDev(spread_history_);
    return s>0.0 ? (cur - m)/s : 0.0;
}
bool SpreadSignal::isSpreadWide() const { return getSpreadZScore()>1.0; }
Signal SpreadSignal::calculate(const OrderBook& book) const {
    // produce z-score as value
    double cur = book.getSpread();
    // keep local history? update() should be called by engine; fall back to single-point
    double z = spread_history_.empty() ? 0.0 : getSpreadZScore();
    Signal s{Signal::SPREAD, book.symbol(), z, clamp(std::abs(z)/3.0,0.0,1.0)};
    s.metadata["spread"] = cur;
    s.metadata["avg_spread"] = spread_history_.empty()?cur:getAverageSpread();
    return s;
}

// ---------- QueuePosition ----------
Quantity QueuePositionSignal::getQueueAhead(const Order& order, const OrderBook& book) const {
    return book.getQueuePosition(order.id);
}
double QueuePositionSignal::getExpectedFillTime(const Order& order, const OrderBook&) const {
    const double ahead = static_cast<double>(order.remaining_quantity);
    return ahead / std::max(1e-6, model_.avg_fill_rate_per_ms);
}
double QueuePositionSignal::getFillProbability(const Order& order, const OrderBook&, int horizon_ms) const {
    const double lambda = model_.avg_fill_rate_per_ms * horizon_ms;
    // Approx Poisson fill for independent events
    const double p = 1.0 - std::exp(-lambda);
    return clamp(p, 0.0, 1.0);
}
Signal QueuePositionSignal::calculate(const OrderBook& book) const {
    // Queue metric at best bid/ask
    double qimb = book.getOrderImbalance(1);
    Signal s{Signal::QUEUE_POSITION, book.symbol(), qimb, 1.0};
    return s;
}

// ---------- SignalGenerator ----------
void SignalGenerator::addCalculator(std::unique_ptr<SignalCalculator> calc) {
    calculator_map_[calc->getName()] = calc.get();
    calculators_.push_back(std::move(calc));
}
std::vector<Signal> SignalGenerator::generateSignals(const OrderBook& book) {
    std::vector<Signal> out; out.reserve(calculators_.size());
    for (auto& c: calculators_) out.emplace_back(c->calculate(book));
    return out;
}
void SignalGenerator::update(const OrderBook& book) {
    for (auto& c: calculators_) c->update(book);
}
std::optional<Signal> SignalGenerator::getSignal(const std::string& name, const OrderBook& book) {
    auto it = calculator_map_.find(name);
    if (it==calculator_map_.end()) return std::nullopt;
    return it->second->calculate(book);
}
Signal SignalGenerator::combineSignals(const std::vector<Signal>& sigs, const std::vector<double>& w) {
    double v=0.0, wc=0.0, conf=0.0;
    for (std::size_t i=0;i<sigs.size() && i<w.size();++i) { v += w[i]*sigs[i].value; wc += w[i]; conf += w[i]*sigs[i].confidence; }
    Signal s{Signal::CUSTOM, sigs.empty()?std::string(""):sigs.front().symbol, wc>0.0 ? v/wc : 0.0, wc>0.0 ? conf/wc : 0.0};
    return s;
}
void SignalGenerator::reset() {
    calculator_map_.clear();
    calculators_.clear();
}

// ---------- SignalStatistics / Features ----------
double SignalStatistics::rollingMean(const std::deque<double>& v){ if(v.empty())return 0.0; double s=0;for(auto x:v)s+=x;return s/v.size();}
double SignalStatistics::rollingStdDev(const std::deque<double>& v){ if(v.size()<2)return 0.0; double m=rollingMean(v); double ss=0;for(auto x:v){double d=x-m;ss+=d*d;}return std::sqrt(ss/(v.size()-1));}
double SignalStatistics::rollingSkewness(const std::deque<double>& v){ if(v.size()<3)return 0.0; double m=rollingMean(v), s=rollingStdDev(v); if(s==0)return 0.0; double t=0; for(auto x:v){double z=(x-m)/s; t+=z*z*z;} return t/v.size();}
double SignalStatistics::rollingKurtosis(const std::deque<double>& v){ if(v.size()<4)return 0.0; double m=rollingMean(v), s=rollingStdDev(v); if(s==0)return 0.0; double t=0; for(auto x:v){double z=(x-m)/s; t+=z*z*z*z;} return t/v.size()-3.0;}
double SignalStatistics::correlation(const std::vector<double>& x,const std::vector<double>& y){ const auto n=std::min(x.size(),y.size()); if(n<2)return 0.0; double mx=0,my=0; for(size_t i=0;i<n;++i){mx+=x[i];my+=y[i];} mx/=n; my/=n; double sxx=0,syy=0,sxy=0; for(size_t i=0;i<n;++i){double dx=x[i]-mx,dy=y[i]-my;sxx+=dx*dx;syy+=dy*dy;sxy+=dx*dy;} return (sxx>0&&syy>0)? sxy/std::sqrt(sxx*syy):0.0;}
double SignalStatistics::zScore(double v,double m,double s){ return s>0? (v-m)/s:0.0;}
double SignalStatistics::ema(double nv,double pe,double a){ return a*nv+(1.0-a)*pe;}
double SignalStatistics::percentile(std::vector<double> vals,double pct){ if(vals.empty())return 0.0; std::sort(vals.begin(),vals.end()); pct = std::clamp(pct,0.0,1.0); double idx = pct*(vals.size()-1); size_t lo=(size_t)std::floor(idx), hi=(size_t)std::ceil(idx); if(lo==hi) return vals[lo]; double w=idx-lo; return (1-w)*vals[lo]+w*vals[hi]; }

std::vector<double> toVec(const std::deque<double>& d){ return std::vector<double>(d.begin(), d.end()); }

std::vector<double> FeatureExtractor::Features::toVector() const {
    return {mid_price,spread,spread_pct,bid_volume,ask_volume,volume_imbalance,
            bid_depth_1,ask_depth_1,bid_depth_5,ask_depth_5,
            microprice,book_pressure,queue_imbalance,
            time_since_last_trade,time_of_day_normalized,
            price_momentum,volume_momentum,volatility};
}

FeatureExtractor::Features FeatureExtractor::extractFeatures(const OrderBook& book) {
    auto stats = book.getStats();
    Features f{};
    f.mid_price = stats.mid_price;
    f.spread = stats.spread;
    f.spread_pct = (stats.mid_price>0.0)? stats.spread/std::max(1e-6, stats.mid_price):0.0;
    f.bid_volume = stats.bid_volume; f.ask_volume = stats.ask_volume;
    f.volume_imbalance = stats.imbalance;
    auto b1 = book.getAggregatedBook(Side::BID,1); auto a1 = book.getAggregatedBook(Side::ASK,1);
    f.bid_depth_1 = b1.empty()?0.0:b1[0].second; f.ask_depth_1 = a1.empty()?0.0:a1[0].second;
    auto b5 = book.getAggregatedBook(Side::BID,5); auto a5 = book.getAggregatedBook(Side::ASK,5);
    double s5b=0,s5a=0; for(auto& p:b5)s5b+=p.second; for(auto& p:a5)s5a+=p.second; f.bid_depth_5 = s5b; f.ask_depth_5 = s5a;
    f.microprice = book.getMicroPrice(1);
    f.book_pressure = stats.imbalance; // proxy
    f.queue_imbalance = stats.imbalance;
    f.time_since_last_trade = 0.0; f.time_of_day_normalized = 0.5;
    f.price_momentum = 0.0; f.volume_momentum = 0.0; f.volatility = 0.0;
    return f;
}
FeatureExtractor::Features FeatureExtractor::extractFeaturesWithHistory(const OrderBook& book, const std::deque<BookStats>& hist) {
    auto f = extractFeatures(book);
    if (hist.size()>1) {
        const auto& last = hist.back(); const auto& prev = hist[hist.size()-2];
        f.price_momentum = last.mid_price - prev.mid_price;
        f.volume_momentum = (static_cast<double>(last.bid_volume+last.ask_volume) -
                             static_cast<double>(prev.bid_volume+prev.ask_volume));
        // naive rolling vol
        double s=0; for (auto& h: hist) s += (h.mid_price - last.mid_price)*(h.mid_price - last.mid_price);
        f.volatility = std::sqrt(s/std::max<std::size_t>(1,hist.size()-1));
    }
    return f;
}
void FeatureExtractor::normalizeFeatures(Features& f, const Features& m, const Features& s) {
    auto v = f.toVector(); auto mv = m.toVector(); auto sv = s.toVector();
    for (std::size_t i=0;i<v.size();++i) v[i] = (sv[i]>0? (v[i]-mv[i])/sv[i] : 0.0);
    // assign back
    f = Features{}; // reset then re-pack
}

} // namespace lob