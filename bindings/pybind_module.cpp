#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "lob/order_book.hpp"
#include "lob/signals.hpp"
#include "lob/backtester.hpp"

namespace py = pybind11;
using namespace lob;

PYBIND11_MODULE(lobpy, m) {
    m.doc() = "C++17 LOB & backtester bindings";

    py::enum_<Side>(m, "Side")
        .value("BID", Side::BID)
        .value("ASK", Side::ASK);

    py::class_<Order>(m, "Order")
        .def(py::init<OrderId, Price, Quantity, Side, Timestamp>(),
             py::arg("id"), py::arg("price"), py::arg("quantity"), py::arg("side"), py::arg("ts"))
        .def_readwrite("id", &Order::id)
        .def_readwrite("price", &Order::price)
        .def_readwrite("quantity", &Order::quantity)
        .def_readwrite("side", &Order::side);

    py::class_<OrderBook>(m, "OrderBook")
        .def(py::init<const std::string&>())
        .def("add_order", [](OrderBook& b, OrderId id, double px, Quantity q, Side s, Timestamp ts){
            return b.addOrder(Order{id, doubleToPrice(px), q, s, ts});
        })
        .def("process_market", [](OrderBook& b, Side s, Quantity q, Timestamp ts){ return b.processMarketOrder(s,q,ts); })
        .def("best_bid", &OrderBook::getBestBid)
        .def("best_ask", &OrderBook::getBestAsk)
        .def("mid", &OrderBook::getMidPrice)
        .def("spread", &OrderBook::getSpread)
        .def("stats", &OrderBook::getStats);

    py::class_<Backtester>(m, "Backtester")
        .def(py::init<>())
        .def("set_data_csv", [](Backtester& bt, const std::string& path){ bt.setDataSource(std::make_unique<CSVDataSource>(path)); })
        .def("add_market_maker", [](Backtester& bt){ bt.addStrategy(std::make_unique<MarketMakerStrategy>()); })
        .def("run", &Backtester::run)
        .def("results", &Backtester::getResults);
}