# Architecture

The system consists of three major subsystems:

- **LOB (L3/L2)** — The limit order book manages orders with price–time priority.  It stores full depth (L3) with per-level queues and aggregated book (L2).  Intrusive per-level queues and RB trees provide O(1) cancels and fast matching, while best bid/ask caches enable constant‑time mid and spread queries【541845463438230†screenshot】.
- **Backtester** — The backtester processes a stream of market data events and strategy-generated orders.  It maintains a portfolio, uses a data source abstraction to feed events, and triggers strategy callbacks on market data, signals, and fills.  At end of day it records snapshots and computes metrics【690010940282616†screenshot】.
- **Signals** — A research layer computes microstructure signals such as order imbalance, microprice, spread z‑score, trade flow, book pressure, and queue position.  A composite signal generator aggregates signals and provides normalized features for machine learning or rule‑based strategies【690010940282616†screenshot】.