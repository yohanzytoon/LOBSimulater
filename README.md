# Limit Order Book Simulator & Event‑Driven Backtester (C++17)

This repository provides a high‑performance limit order book engine and event‑driven backtesting framework written in modern C++17.  It is designed for quantitative researchers and developers building algorithmic trading strategies who need fast, accurate and reproducible simulations.

The project originated from a set of well‑tested order book and backtester headers.  Those APIs have been preserved and fully implemented here, and the system has been extended with a research layer, Python bindings and automated CI/CD.

## Features

* **Limit Order Book** – Supports both L2 (aggregated) and L3 (full depth) books with price–time priority.  Orders are stored in intrusive linked lists per price level for O(1) cancels and modifications, while red‑black trees keep levels sorted.  Cache‑aware data structures and reserved storage minimise heap allocations.
* **Backtester** – Fully event driven.  Feeds replay historical market data from CSV into the book, emits fills and signals, and lets user strategies submit orders.  Portfolio accounting tracks positions, cash, P&L and risk metrics.  Performance stats record per‑event latency.
* **Research layer** – Implements microstructure signals such as order imbalance, microprice, spread z‑score, trade flow and queue position.  A composite `SignalGenerator` aggregates multiple signals and exposes them to strategies.  A `FeatureExtractor` produces rich feature vectors for machine learning.
* **Python bindings** – Via `pybind11`, the core classes (`OrderBook`, `Backtester`, etc.) are accessible from Python.  This enables seamless integration with pandas, NumPy and scikit‑learn for data analysis and modelling.
* **Engineering** – Built with modern CMake.  Unit tests with Catch2 ensure correctness.  GitHub Actions builds on Linux and macOS, runs sanitizers (ASan/UBSan), and performs static analysis using clang‑tidy and cppcheck.  Documentation is generated with Doxygen.

## Quickstart

Clone the repository and build with CMake.  The following commands build the library, examples and tests with default options:

```bash
git clone <REPO_URL>
cd lob-backtester
cmake -S . -B build -G Ninja -DLOB_BUILD_TESTS=ON -DLOB_BUILD_BINDINGS=ON
cmake --build build -j
ctest --test-dir build
```

### Running examples

Several example strategies are provided.  Each uses synthetic CSV data located in `data/` (you should supply your own LOB tick data for real experiments).

```bash
./build/lob_main                    # runs a simple backtest driver
./build/example_market_maker        # runs a market making strategy
./build/example_momentum            # runs a momentum strategy
./build/example_signal_exec         # runs a skeleton signal‑based strategy
./build/bench_order_book            # stress test throughput/latency
```

### Python bindings

If you enabled bindings during CMake configuration, a Python extension module named `lobpy` will be built in the `bindings` directory.  You can install it into a virtual environment with pip:

```bash
pip install ./build/bindings

# or import directly from the build directory:
python -c "import sys; sys.path.append('build/bindings'); import lobpy; print(lobpy.OrderBook('TEST').mid())"
```

## Benchmarking

The `benchmarks/bench_order_book.cpp` program inserts a large number of orders into the book and reports throughput (operations per second).  This can be useful to tune compiler flags, allocators and data structures.  On modern hardware, millions of operations per second can be achieved in release builds.

## Repository structure

```
lob-backtester/
├── CMakeLists.txt               # Top‑level build system
├── README.md                    # This file
├── LICENSE                      # MIT license
├── .gitignore                   # Ignore patterns
├── .clang-format                # clang‑format configuration
├── .clang-tidy                  # clang‑tidy configuration
├── .github/workflows/ci.yml     # GitHub Actions CI
├── include/lob/                 # Public headers
│   ├── order_book.hpp           # Order book API (from original code)
│   ├── event.hpp                # Event wrapper for backtester
│   ├── order.hpp                # Light wrapper to re-export order types
│   ├── backtester.hpp           # Backtester API (from original code)
│   ├── signals.hpp              # Signals and feature extraction
│   └── metrics.hpp              # Backtest metrics and analytics
├── src/                         # Library implementation
│   ├── order_book.cpp           # Order book implementation (from original code)
│   ├── backtester.cpp           # Backtester and strategies implementation
│   ├── signals.cpp              # Signal calculators implementation
│   ├── metrics.cpp              # Metrics computation implementation
│   └── main.cpp                 # Simple CLI driver
├── bindings/                    # Python bindings via pybind11
│   ├── CMakeLists.txt
│   └── pybind_module.cpp        # pybind11 glue code
├── tests/                       # Unit tests (Catch2)
│   ├── test_order_book.cpp
│   ├── test_backtester.cpp
│   └── test_signals.cpp
├── benchmarks/                  # Performance benchmarks
│   └── bench_order_book.cpp
├── examples/                    # Example strategies
│   ├── market_maker.cpp
│   ├── momentum.cpp
│   └── signal_execution.cpp
├── docs/                        # Documentation and design notes
│   ├── Doxyfile                 # Doxygen configuration
│   ├── architecture.md          # High‑level architecture description
│   └── signals.md               # Details of microstructure signals
└── data/                        # Placeholder for CSV data (not included)
```
